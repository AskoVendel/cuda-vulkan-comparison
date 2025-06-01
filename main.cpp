#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include <cmath>
#include <iostream>
#include "stb_image.h"
#include "stb_image_write.h"
using namespace std;

#include <chrono>

/*void rotate_img(unsigned char* data, int x, int y) {
	double new_height;
	double new_width;

	double angleRad = 45 * (M_PI / 180);
	double cosine = cos(angleRad);
	double sine = sin(angleRad);

	new_height = (int)round(abs(y * cosine) + abs(x * sine)) + 1;
	new_width = (int)round(abs(x * cosine) + abs(y * sine)) + 1;

	cout << "angleRad is: " << angleRad << "\n";
	cout << "cosine is: " << cosine << "\n";
	cout << "sine is: " << sine << "\n";
	cout << "New height is: " << new_height << "\n";
	cout << "New width is: " << new_width << "\n";
	cout << "-------------------------" << "\n";


	int original_centre_h = round(((y + 1) / 2) - 1);
	int original_centre_w = round(((x + 1) / 2) - 1);

	int new_centre_h = round(((new_height + 1) / 2) - 1);
	int new_centre_w = round(((new_width + 1) / 2) - 1);

	cout << "original_centre_h is: " << original_centre_h << "\n";
	cout << "original_centre_w is: " << original_centre_w << "\n";
	cout << "new_centre_h is: " << new_centre_h << "\n";
	cout << "new_centre_w is: " << new_centre_w << "\n";
	cout << "-------------------------" << "\n";

	unsigned char (*col)[361][357][3] = reinterpret_cast<unsigned char (*)[361][357][3]>(data);

	unsigned char zeros[509 * 509 * 3] = { (unsigned char)0 }; //[new height * new width * channels]
	unsigned char (*empty)[509][509][3] = reinterpret_cast<unsigned char (*)[509][509][3]>(zeros); // [new height][new width][channels]

	for (int i = 0; i < y; i++) {
		for (int j = 0; j < x; j++) {
			int y2 = y - 1 - i - original_centre_h;
			int x2 = x - 1 - j - original_centre_w;


			double tangent = tan(angleRad / 2);

			int new_x = round(x2 - y2 * tangent);
			int new_y = y2;

			new_y = round(new_x * sin(angleRad) + new_y);
			new_x = round(new_x - new_y * tangent);

			new_y = new_centre_h - new_y;
			new_x = new_centre_w - new_x;


			(*empty)[new_y][(new_x)][0] = (*col)[i][j][0];
			(*empty)[new_y][(new_x)][1] = (*col)[i][j][1];
			(*empty)[new_y][(new_x)][2] = (*col)[i][j][2];

		}
	}

	stbi_write_bmp("output.bmp", new_width, new_height, 3, zeros);
}*/

void lowpass_filter(unsigned char* data, int width, int height, unsigned char *out, int N, float *coeff){
	// leiame koefitsientide summa, sellega on vaja läbi jagada
	float divider = 0;
	for(int i = -N; i <= N; i++)
		for(int j = -N; j <= N; j++)
			divider = divider + coeff[abs(i) * (N + 1) + abs(j)];
	//printf("%.3f\n", divider);
	int index;
	float sum;
	// arvutame väljundi
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			for (int c = 0; c < 3; c++){
				// kui on servad, siis me üldse ei rakenda filtrit
				if(y < N || x < N || y >= height - N || x >= width - N){
					index = (y * width + x) * 3 + c;
					out[index] = data[index];
					continue;
				}
				// rakendame filtrit
				sum = 0;
				for(int a = -N; a <= N; a++) // siin on vaja uut kahte tsüklit filtri suuruse järgi
					for(int b = -N; b <= N; b++){
						int index = ((y+b) * width + x+a) * 3 + c;
						sum = sum + data[index] * coeff[abs(a) * (N + 1) + abs(b)];
					}
				sum = sum / divider;
				index = (y * width + x) * 3 + c;
				out[index] = sum;
			}
}

void highpass_filter(unsigned char* data, int width, int height, unsigned char *out, int N, float *coeff, float sharp_mul){
	// highpass filtri kaalude summa peab olema 0, seega pole vaja arvutada siin jagajat
	int index;
	float sum;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			for (int c = 0; c < 3; c++){
				// kui on servad, siis me üldse ei rakenda filtrit
				if(y < N || x < N || y >= height - N || x >= width - N){
					index = (y * width + x) * 3 + c;
					out[index] = data[index];
					continue;
				}
				// rakendame filtrit
				sum = 0;
				for(int a = -N; a <= N; a++) // siin on vaja uut kahte tsüklit filtri suuruse järgi
					for(int b = -N; b <= N; b++){
						int index = ((y+b) * width + x+a) * 3 + c;
						sum = sum + data[index] * coeff[abs(a) * (N + 1) + abs(b)];
					}
				index = (y * width + x) * 3 + c;
				sum = data[index] + sum * sharp_mul;
				out[index] = max(0, min(255, (int)sum));
			}
}

void average_filter(int N, float *coeff){
	for(int i = 0; i <= N; i++)
		for(int j = 0; j <= N; j++)
			coeff[i * (N + 1) + j] = 1;
}

void gaussian_filter(int N, float *coeff){
	int w = 2 * N + 1; // filtri tegelik laius
	float sigma = w / 6.0; // 6 sigma oleks ruudu külg
	//printf("sigma: %.3f\n", sigma);
	float div1 = 4 * asin(1) * sigma * sigma;
	float div2 = 2 * sigma * sigma;
	for(int i = 0; i <= N; i++)
		for(int j = 0; j <= N; j++){
			coeff[i * (N + 1) + j] = exp( -((i * i + j * j) / div2)) / div1;
			//printf("%d,%d(%.3f): %.3f\n", i, j, sqrt(i * i + j * j), coeff[i * (N + 1) + j]);
		}
}

void sharpening_filter(int N, float *coeff){
	int w = 2 * N + 1; // filtri tegelik laius

	float dist = sqrt(2*N*N);
	float mul = dist / asin(1);
	//printf("%.3f %.3f %.3f\n", dist, asin(1), mul);

	for(int i = 0; i <= N; i++)
		for(int j = 0; j <= N; j++){
			coeff[i * (N + 1) + j] = -cos(sqrt(i * i + j * j)/mul);
			//printf("%d,%d(%.3f): %.3f\n", i, j, sqrt(i * i + j * j), coeff[i * (N + 1) + j]);
		}
	coeff[0] = 0;
	float divider = 0;
	for(int i = -N; i <= N; i++)
		for(int j = -N; j <= N; j++)
			divider = divider + coeff[abs(i) * (N + 1) + abs(j)];
	coeff[0] = -divider;
	//printf("%.3f\n", divider);
}

void process_noise(void) {
	int x;
	int y;
	int n;
	int ok;

	ok = stbi_info("noise.bmp", &x, &y, &n);

	cout << "OK?: " << ok << "\n";
	cout << "width: " << x << "\n";
	cout << "height: " << y << "\n";
	cout << "bytes per pixel: " << n << "\n";
	cout << "-------------------------" << "\n";

	unsigned char* data = stbi_load("noise.bmp", &x, &y, &n, 3);

	unsigned char* out  = new unsigned char[x * y * 3];
	unsigned char* out1 = new unsigned char[x * y * 3];
	unsigned char* out2 = new unsigned char[x * y * 3];
	unsigned char* out3 = new unsigned char[x * y * 3];

	n = 4;
	float *c1 = new float[(n+1) * (n+1)];
	average_filter(n, c1);

	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	auto t1 = high_resolution_clock::now();

	lowpass_filter(data, x, y, out, n, c1);

	auto t2 = high_resolution_clock::now();

	duration<double, std::milli> ms_double = t2 - t1;
    std:cout << ms_double.count() << "ms\n";

	stbi_write_bmp("noise_blur1.bmp", x, y, 3, out);

	
	/*n = 8;
	float *c2 = new float[(n+1) * (n+1)];
	gaussian_filter(n, c2);
	lowpass_filter(data, x, y, out1, n, c2);
	stbi_write_bmp("noise_blur2.bmp", x, y, 3, out1);//*/

	n = 1;
	float c3[4] = {3, -1, -1, 0.25}; // need artikli koefitsiendid, selle pildi jaoks lahja

	//avg blur
	/*highpass_filter(out, x, y, out2, n, c3, 1); // rakendame hägustatud pildile
	stbi_write_bmp("noise_sharp11.bmp", x, y, 3, out2);
	highpass_filter(out2, x, y, out3, n, c3, 1); // rakendame veel kord
	stbi_write_bmp("noise_sharp12.bmp", x, y, 3, out3);//*/

	//gauss blur
	/*highpass_filter(out1, x, y, out2, n, c3, 1); // rakendame hägustatud pildile
	stbi_write_bmp("noise_sharp21.bmp", x, y, 3, out2);
	highpass_filter(out2, x, y, out3, n, c3, 1); // rakendame veel kord
	stbi_write_bmp("noise_sharp22.bmp", x, y, 3, out3);//*/


	n = 2;
	float *c4 = new float[(n+1) * (n+1)];
	sharpening_filter(n, c4);

	//avg blur
	highpass_filter(out, x, y, out2, n, c4, 1.2); // rakendame hägustatud pildile
	stbi_write_bmp("output_sharp_11.bmp", x, y, 3, out2);
	/*highpass_filter(out2, x, y, out3, n, c4, 1); // rakendame veel kord
	stbi_write_bmp("output_sharp_12.bmp", x, y, 3, out3);//*/

	//gauss blur
	/*highpass_filter(out1, x, y, out2, n, c4, 1); // rakendame hägustatud pildile
	stbi_write_bmp("output_sharp_21.bmp", x, y, 3, out2);
	highpass_filter(out2, x, y, out3, n, c4, 1); // rakendame veel kord
	stbi_write_bmp("output_sharp_22.bmp", x, y, 3, out3);//*/

	n = 3;
	float *c5 = new float[(n+1) * (n+1)];
	sharpening_filter(n, c5);

	//avg blur
	highpass_filter(out, x, y, out2, n, c5, 0.4); // rakendame hägustatud pildile
	stbi_write_bmp("output_sharp_101.bmp", x, y, 3, out2);
	/*highpass_filter(out2, x, y, out3, n, c4, 1); // rakendame veel kord
	stbi_write_bmp("output_sharp_102.bmp", x, y, 3, out3);//*/

	delete[] data;
	delete[] out;
	delete[] out2;
	delete[] out3;
}

void process_rotate(void) {
	int x;
	int y;
	int n;
	int ok;

	ok = stbi_info("rotate.bmp", &x, &y, &n);

	cout << "OK?: " << ok << "\n";
	cout << "width: " << x << "\n";
	cout << "height: " << y << "\n";
	cout << "bytes per pixel: " << n << "\n";
	cout << "-------------------------" << "\n";

	unsigned char* data = stbi_load("rotate.bmp", &x, &y, &n, 3);

	unsigned char* out  = new unsigned char[x * y * 3];
	unsigned char* out1 = new unsigned char[x * y * 3];
	unsigned char* out2 = new unsigned char[x * y * 3];
	unsigned char* out3 = new unsigned char[x * y * 3];

	/*n = 1;
	float *c1 = new float[(n+1) * (n+1)];
	average_filter(n, c1);
	lowpass_filter(data, x, y, out, n, c1);
	stbi_write_bmp("rotate_blur1.bmp", x, y, 3, out);//*/

	n = 2;
	float *c2 = new float[(n+1) * (n+1)];
	gaussian_filter(n, c2);
	lowpass_filter(data, x, y, out1, n, c2);
	stbi_write_bmp("rotate_blur2.bmp", x, y, 3, out1);

	n = 1;
	float c3[4] = {3, -1, -1, 0.25}; // need artikli koefitsiendid

	//avg blur
	/*highpass_filter(out, x, y, out2, n, c3, 1); // rakendame hägustatud pildile
	stbi_write_bmp("rotate_sharp11.bmp", x, y, 3, out2);
	highpass_filter(out2, x, y, out3, n, c3, 1); // rakendame veel kord
	stbi_write_bmp("rotate_sharp12.bmp", x, y, 3, out3);//*/

	//gauss blur
	highpass_filter(out1, x, y, out2, n, c3, 1); // rakendame hägustatud pildile
	stbi_write_bmp("rotate_sharp21.bmp", x, y, 3, out2);
	/*highpass_filter(out2, x, y, out3, n, c3, 1); // rakendame veel kord
	stbi_write_bmp("rotate_sharp22.bmp", x, y, 3, out3);//*/


	n = 1;
	float *c4 = new float[(n+1) * (n+1)];
	sharpening_filter(n, c4);

	//avg blur
	/*highpass_filter(out, x, y, out2, n, c4, 1); // rakendame hägustatud pildile
	stbi_write_bmp("rotate_sharp_11.bmp", x, y, 3, out2);
	highpass_filter(out2, x, y, out3, n, c4, 1); // rakendame veel kord
	stbi_write_bmp("rotate_sharp_12.bmp", x, y, 3, out3);//*/

	//gauss blur
	highpass_filter(out1, x, y, out2, n, c4, 1); // rakendame hägustatud pildile
	stbi_write_bmp("rotate_sharp_21.bmp", x, y, 3, out2);
	/*highpass_filter(out2, x, y, out3, n, c4, 1); // rakendame veel kord
	stbi_write_bmp("rotate_sharp_22.bmp", x, y, 3, out3);//*/

	delete[] data;
	delete[] out;
	delete[] out2;
	delete[] out3;
}


int main() {
	//process_rotate();
	process_noise();

	return 0;
}
