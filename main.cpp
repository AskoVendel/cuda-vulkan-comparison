#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include <cmath>
#include <iostream>
#include "stb_image.h"
#include "stb_image_write.h"
using namespace std;

void rotate_img(unsigned char* data, int x, int y) {
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

	stbi_write_bmp("test_out.bmp", new_width, new_height, 3, zeros);
}

void moving_average_filter_3(unsigned char* data, int width, int height) {

	unsigned char* temp = new unsigned char[width * height * 3];
	unsigned char* output = new unsigned char[width * height * 3];

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			for (int c = 0; c < 3; c++) {
				int index = (y * width + x) * 3 + c;

				int previous = (x > 0) ? data[index - 3] : data[index];
				int current = data[index];
				int next = (x < width - 1) ? data[index + 3] : data[index];

				temp[index] = (previous + current + next) / 3;
			}
		}
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			for (int c = 0; c < 3; c++) {
				int index = (y * width + x) * 3 + c;

				int previous = (y > 0) ? temp[index - (width * 3)] : temp[index];
				int current = temp[index];
				int next = (y < height - 1) ? temp[index + (width * 3)] : temp[index];

				output[index] = (previous + current + next) / 3;
			}
		}
	}

	stbi_write_bmp("output.bmp", width, height, 3, output);
}

void process_image(const char* filename) {
	int x;
	int y;
	int n;
	int ok;

	ok = stbi_info(filename, &x, &y, &n);

	cout << "OK?: " << ok << "\n";
	cout << "width: " << x << "\n";
	cout << "height: " << y << "\n";
	cout << "bytes per pixel: " << n << "\n";
	cout << "-------------------------" << "\n";

	unsigned char* data = stbi_load(filename, &x, &y, &n, 3);

	//rotate_img(data, x, y);
	moving_average_filter_3(data, x, y);
}

int main() {
	process_image("rotate.bmp");

	return 0;
}
