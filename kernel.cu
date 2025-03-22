#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <cmath>
#include <iostream>
#include "stb_image.h"
#include "stb_image_write.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <stdio.h>

using namespace std;

#define CHANNELS 3

void average_filter(int N, float* coeff) {
    for (int i = 0; i <= N; i++)
        for (int j = 0; j <= N; j++)
            coeff[i * (N + 1) + j] = 1;
}

__global__ void lowpass_filter_kernel(unsigned char* d_data, int width, int height, unsigned char* d_out, int N, float* d_coeff, float divider) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return; // Bounds check

    for (int c = 0; c < 3; c++) {  // RGB
        int index = (y * width + x) * 3 + c;

        
        if (y < N || x < N || y >= height - N || x >= width - N) {
            d_out[index] = d_data[index];
            continue;
        }

        float sum = 0.0f;
        for (int a = -N; a <= N; a++) {
            for (int b = -N; b <= N; b++) {
                int neighbor_idx = ((y + b) * width + (x + a)) * 3 + c;
                sum += d_data[neighbor_idx] * d_coeff[(abs(a) * (N + 1)) + abs(b)];
            }
        }
        d_out[index] = static_cast<unsigned char>(sum / divider);
    }
}

void sharpening_filter(int N, float* coeff) {
    int w = 2 * N + 1; // filtri tegelik laius

    float dist = sqrt(2 * N * N);
    float mul = dist / asin(1);
    //printf("%.3f %.3f %.3f\n", dist, asin(1), mul);

    for (int i = 0; i <= N; i++)
        for (int j = 0; j <= N; j++) {
            coeff[i * (N + 1) + j] = -cos(sqrt(i * i + j * j) / mul);
            //printf("%d,%d(%.3f): %.3f\n", i, j, sqrt(i * i + j * j), coeff[i * (N + 1) + j]);
        }
    coeff[0] = 0;
    float divider = 0;
    for (int i = -N; i <= N; i++)
        for (int j = -N; j <= N; j++)
            divider = divider + coeff[abs(i) * (N + 1) + abs(j)];
    coeff[0] = -divider;
    //printf("%.3f\n", divider);
}

__global__ void highpass_filter_kernel(unsigned char* d_data, int width, int height,
    unsigned char* d_out, int N, float* d_coeff, float sharp_mul) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;  // Bounds check

    for (int c = 0; c < 3; c++) {  // RGB
        int index = (y * width + x) * 3 + c;

        
        if (y < N || x < N || y >= height - N || x >= width - N) {
            d_out[index] = d_data[index];
            continue;
        }

        
        float sum = 0;
        for (int a = -N; a <= N; a++) {
            for (int b = -N; b <= N; b++) {
                int neighbor_idx = ((y + b) * width + (x + a)) * 3 + c;
                sum += d_data[neighbor_idx] * d_coeff[abs(a) * (N + 1) + abs(b)];
            }
        }

        
        float new_value = d_data[index] + sum * sharp_mul;
        d_out[index] = max(0, min(255, (int)new_value));  // Clamp to [0,255]
    }
}

void process_noise(void) {
    int x;
    int y;
    int n;
    int ok;

    unsigned char* d_data;
    float* d_c1;
    unsigned char* d_out1;
    float* d_c4;
    unsigned char* d_out2;

    ok = stbi_info("noise.bmp", &x, &y, &n);

    cout << "OK?: " << ok << "\n";
    cout << "width: " << x << "\n";
    cout << "height: " << y << "\n";
    cout << "bytes per pixel: " << n << "\n";
    cout << "-------------------------" << "\n";

    unsigned char* data = stbi_load("noise.bmp", &x, &y, &n, 3);

    unsigned char* out = new unsigned char[x * y * 3];
    unsigned char* out1 = new unsigned char[x * y * 3];
    unsigned char* out2 = new unsigned char[x * y * 3];
    unsigned char* out3 = new unsigned char[x * y * 3];

    // Allocate Data memory on device
    cudaError_t errDataMalloc = cudaMalloc((void**)&d_data, (sizeof(unsigned char) * y * x * CHANNELS));
    if (errDataMalloc != cudaSuccess) {
        std::cerr << "CUDA DataMalloc failed: " << cudaGetErrorString(errDataMalloc) << std::endl;
    }

    // Data to device memory
    cudaError_t errDataMemCpy = cudaMemcpy(d_data, data, (sizeof(unsigned char) * y * x * CHANNELS), cudaMemcpyHostToDevice);
    if (errDataMemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    n = 4;
    float* c1 = new float[(n + 1) * (n + 1)];
    average_filter(n, c1);

    // Allocate c1 memory on device
    cudaError_t errC1Malloc = cudaMalloc((void**)&d_c1, (sizeof(float) * (n + 1) * (n + 1)));
    if (errC1Malloc != cudaSuccess) {
        std::cerr << "CUDA DataMalloc failed: " << cudaGetErrorString(errDataMalloc) << std::endl;
    }

    // c1 to device memory
    cudaError_t errC1MemCpy = cudaMemcpy(d_c1, c1, (sizeof(float) * (n + 1) * (n + 1)), cudaMemcpyHostToDevice);
    if (errC1MemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    // Allocate out1 memory on device
    cudaError_t errOut1Malloc = cudaMalloc((void**)&d_out1, (sizeof(unsigned char) * y * x * CHANNELS));
    if (errOut1Malloc != cudaSuccess) {
        std::cerr << "CUDA DataMalloc failed: " << cudaGetErrorString(errDataMalloc) << std::endl;
    }

    // out1 to device memory
    cudaError_t errOut1MemCpy = cudaMemcpy(d_out1, out1, (sizeof(unsigned char) * y * x * CHANNELS), cudaMemcpyHostToDevice);
    if (errOut1MemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    float divider = 0;
    for (int i = -n; i <= n; i++)
        for (int j = -n; j <= n; j++)
            divider += c1[abs(i) * (n + 1) + abs(j)];
    
    dim3 blockSize(16, 16);
    dim3 gridSize((x + blockSize.x - 1) / blockSize.x, (y + blockSize.y - 1) / blockSize.y);
    lowpass_filter_kernel << <gridSize, blockSize >> > (d_data, x, y, d_out1, n, d_c1, divider);
    cudaDeviceSynchronize();

    // out1 back to host memory
    cudaError_t errOutMemCpy = cudaMemcpy(out1, d_out1, (sizeof(unsigned char) * y * x * CHANNELS), cudaMemcpyDeviceToHost);
    if (errOutMemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    stbi_write_bmp("noise_blur1.bmp", x, y, CHANNELS, out1);

    n = 2;
    float* c4 = new float[(n + 1) * (n + 1)];
    sharpening_filter(n, c4);

    // Allocate c4 memory on device
    cudaError_t errC4Malloc = cudaMalloc((void**)&d_c4, (sizeof(float) * (n + 1) * (n + 1)));
    if (errC4Malloc != cudaSuccess) {
        std::cerr << "CUDA DataMalloc failed: " << cudaGetErrorString(errDataMalloc) << std::endl;
    }

    // c4 to device memory
    cudaError_t errC4MemCpy = cudaMemcpy(d_c4, c1, (sizeof(float) * (n + 1) * (n + 1)), cudaMemcpyHostToDevice);
    if (errC4MemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    // Allocate out2 memory on device
    cudaError_t errOut2Malloc = cudaMalloc((void**)&d_out2, (sizeof(unsigned char) * y * x * CHANNELS));
    if (errOut2Malloc != cudaSuccess) {
        std::cerr << "CUDA DataMalloc failed: " << cudaGetErrorString(errDataMalloc) << std::endl;
    }


    highpass_filter_kernel << <gridSize, blockSize >> > (d_out1, x, y, d_out2, n, d_c4, 1.2);
    cudaDeviceSynchronize();

    // out1 back to host memory
    cudaError_t errOut2MemCpy = cudaMemcpy(out2, d_out2, (sizeof(unsigned char) * y * x * CHANNELS), cudaMemcpyDeviceToHost);
    if (errOut2MemCpy != cudaSuccess) {
        std::cerr << "CUDA DataMemCpy failed: " << cudaGetErrorString(errDataMemCpy) << std::endl;
    }

    stbi_write_bmp("noise_sharp1.bmp", x, y, CHANNELS, out2);

}


int main() {
    process_noise();

    cudaError_t cudaStatus = cudaDeviceReset();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceReset failed!");
        return 1;
    }

    return 0;
}
