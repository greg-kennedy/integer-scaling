#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>

// input width/height
#define IN_WIDTH 640U
#define IN_HEIGHT 480U
#define IN_SIZE (IN_WIDTH * IN_HEIGHT)

// target width/height
#define OUT_WIDTH 1920U
#define OUT_HEIGHT 1080U
#define OUT_SIZE (OUT_WIDTH * OUT_HEIGHT)

// TEST() macro
#define TEST(x) { clock_t start = clock(); \
			for (unsigned int count=0; count<iterations; count++) \
			{ x } \
			clock_t end = clock(); \
			printf("\tComplete: %lf seconds\n", (double)(end-start) / CLOCKS_PER_SEC); }

// CALL_TEST() macro
#define CALL_TEST(x) { printf("Begin test " #x "\n"); \
			x(); \
			dump_ppm(#x ".ppm",(unsigned int*)image_out,OUT_WIDTH,OUT_HEIGHT); }

// globals
unsigned int image_in[IN_HEIGHT][IN_WIDTH];
unsigned int image_out[OUT_HEIGHT][OUT_WIDTH];

unsigned int iterations=500;

void dump_ppm(const char* filename, const unsigned int* img, unsigned int w, unsigned int h)
{
	// Comment this line if you want to produce PPM images.
	return;

	FILE *fp = fopen(filename, "w");
	fprintf(fp, "P6\n%u %u\n255\n",w,h);
	for (unsigned int y=0; y<h; ++y)
	{
		for (unsigned int x=0; x<w; ++x)
		{
			unsigned int color = img[y*w+x];
			fprintf(fp,"%c%c%c", (color & 0x00FF0000) >> 16,
						(color & 0x0000FF00) >> 8,
						(color & 0x000000FF));
		}
	}
	fclose(fp);
}

void method_1()
{
	// Naive division scaling.  Each output x,y multiplies by the ratio
	//  of in-to-out to find its source location.
	TEST( {
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
				image_out[y][x] = image_in[y * IN_HEIGHT / OUT_HEIGHT][x * IN_WIDTH / OUT_WIDTH];
	} );
}

void method_1b()
{
	// Slightly better division scaling (promotes the y index out
	//  of the main loop)
	// Compilers will (hopefully) recognize y * IN_HEIGHT / OUT_HEIGHT
	//  as constant for every iteration, and produce this from the previous
	//  version anyway
	TEST( {
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			unsigned int y_idx = y * IN_HEIGHT / OUT_HEIGHT;
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
				image_out[y][x] = image_in[y_idx][x * IN_WIDTH / OUT_WIDTH];
		}
	} );
}

void method_1c()
{
	// Boost the array reference out of one loop level
	//  This is essentially trying pointer wizardry to do to the
	//  output array as we did with the previous source array
	TEST( {
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			unsigned int (*in_ptr)[IN_WIDTH] = &image_in[y * IN_HEIGHT / OUT_HEIGHT];
			unsigned int (*out_ptr)[OUT_WIDTH] = &image_out[y];
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
				(*out_ptr)[x] = (*in_ptr)[x * IN_WIDTH / OUT_WIDTH];
		}
	} );
}

void method_1d()
{
	// Hoist out_ptr all the way out of the loop
	//  On ARM clang v3.8.0 this is actually much slower than 1c
	TEST( {
		unsigned int* restrict out_ptr = &image_out[0][0];
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			unsigned int (*restrict in_ptr)[IN_WIDTH] = &image_in[y * IN_HEIGHT / OUT_HEIGHT];
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
			{
				*out_ptr = (*in_ptr)[x * IN_WIDTH / OUT_WIDTH];
				out_ptr ++;
			}
		}
	} );
}

void method_1e()
{
	// Like 1 except count backwards
	//  May not be faster than count-forwards on modern compilers,
	//  and possibly even bad for memory access patterns,
	// but it CAN be done in-place!
	TEST( {
		unsigned int y = OUT_HEIGHT;
		while (y > 0)
		{
			--y;
			unsigned int x = OUT_WIDTH;
			while (x > 0)
			{
				--x;
				image_out[y][x] = image_in[y * IN_HEIGHT / OUT_HEIGHT][x * IN_WIDTH / OUT_WIDTH];
			}
		}
	} );
}

void method_2()
{
	// Precomputed division arrays
	//  This precomputes the division and stores the result
	// The loop needs to only do a lookup instead.
	unsigned int target_col[OUT_WIDTH];
	for (unsigned int x=0; x<OUT_WIDTH; ++x)
		target_col[x] = (x*IN_WIDTH)/OUT_WIDTH;

	unsigned int target_row[OUT_HEIGHT];
	for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		target_row[y] = (y*IN_HEIGHT)/OUT_HEIGHT;

	TEST( {
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
				image_out[y][x] = image_in[target_row[y]][target_col[x]];
	} );
}

void method_2b()
{
	// Same as above but with smaller datatypes?
	//  ARM clang 3.8.0 makes 2b slower than 2!
	// ARM gcc 5.4 makes them equivalent.
	unsigned short target_col[OUT_WIDTH];
	for (unsigned short x=0; x<OUT_WIDTH; ++x)
		target_col[x] = (x*IN_WIDTH)/OUT_WIDTH;

	unsigned short target_row[OUT_HEIGHT];
	for (unsigned short y=0; y<OUT_HEIGHT; ++y)
		target_row[y] = (y*IN_HEIGHT)/OUT_HEIGHT;

	TEST( {
		for (unsigned short y=0; y<OUT_HEIGHT; ++y)
			for (unsigned short x=0; x<OUT_WIDTH; ++x)
				image_out[y][x] = image_in[target_row[y]][target_col[x]];
	} );
}

void method_2c()
{
	// Same as method 2 but use the heap for these computed values
	unsigned int* target_col = malloc(sizeof(unsigned int) * OUT_WIDTH);
	for (unsigned int x=0; x<OUT_WIDTH; ++x)
		target_col[x] = (x*IN_WIDTH)/OUT_WIDTH;

	unsigned int* target_row = malloc(sizeof(unsigned int) * OUT_HEIGHT);
	for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		target_row[y] = (y*IN_HEIGHT)/OUT_HEIGHT;

	TEST( {
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
				image_out[y][x] = image_in[target_row[y]][target_col[x]];
	} );

	free(target_row);
	free(target_col);
}

void method_3()
{
	// Precomputed "flat" array
	//  This makes an OUT_WIDTH * OUT_HEIGHT array, where each entry is the index
	//  of image_in from which to take the source value.
	// The problem with this method is that the arrays are so large,
	//  it often does not fit into CPU cache.
	unsigned int target_idx[OUT_SIZE];
	for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		for (unsigned int x=0; x<OUT_WIDTH; ++x)
			target_idx[y * OUT_WIDTH + x] =
				(y * IN_HEIGHT / OUT_HEIGHT) * IN_WIDTH +
				(x * IN_WIDTH / OUT_WIDTH);

	unsigned int *flat_image_in = &image_in[0][0];
	unsigned int *flat_image_out = &image_out[0][0];

	TEST( {
		for (unsigned int idx=0; idx<OUT_SIZE; ++idx)
			flat_image_out[idx] = flat_image_in[target_idx[idx]];
	} );
}

void method_3b()
{
	// Could method 3 be faster with pointer arithmetic?
	unsigned int target_idx[OUT_SIZE];
	for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		for (unsigned int x=0; x<OUT_WIDTH; ++x)
			target_idx[y * OUT_WIDTH + x] =
				(y * IN_HEIGHT / OUT_HEIGHT) * IN_WIDTH +
				(x * IN_WIDTH / OUT_WIDTH);

	// begin counting from first element of target_idx
	const unsigned int *flat_image_end = &image_out[OUT_HEIGHT][0];

	const unsigned int *flat_image_in = &image_in[0][0];

	TEST( {
		unsigned int *target_ptr = &target_idx[0];
		unsigned int *flat_image_out = &image_out[0][0];
		while (flat_image_out < flat_image_end)
		{
			*flat_image_out = flat_image_in[*target_ptr];
			flat_image_out++; target_ptr ++;
		}
	} );
}

void method_4()
{
	// "Change trigger" method
	//  Similar to precomputed division, but this method uses a bitfield
	//  to determine when to increment the input row / column.
	// The calculated arrays can be pretty small.
	unsigned char target_col[OUT_WIDTH] = {0};
	for (unsigned int x=1; x<OUT_WIDTH; ++x)
		if (((x*IN_WIDTH)/OUT_WIDTH) > ((x-1)*IN_WIDTH)/OUT_WIDTH)
			target_col[x] = 1;

	unsigned char target_row[OUT_HEIGHT] = {0};
	for (unsigned int y=1; y<OUT_HEIGHT; ++y)
		if (((y*IN_HEIGHT)/OUT_HEIGHT) > ((y-1)*IN_HEIGHT)/OUT_HEIGHT)
			target_row[y] = 1;

	TEST( {
		unsigned int y_out = 0;
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			if (target_row[y]) y_out++;
			unsigned int x_out = 0;
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
			{
				if (target_col[x]) x_out++;
				image_out[y][x] = image_in[y_out][x_out];
			}
		}
	} );
}

void method_5()
{
	// Fixed-point math using bitshift
	//  First two statements set the decimal position for max precision
	unsigned int x_shift = 0;
	while ((UINT_MAX >> (x_shift + 1)) > OUT_WIDTH) x_shift ++;
	// Now calculate the amount to increment each time
	unsigned int x_increment = (IN_WIDTH << x_shift) / OUT_WIDTH;
	// In case of exact divisor this works fine, but if inexact, we can
	//  have fixedpoint precision errors from the truncation.
	// One workaround is to always round up in those cases.
	//  This is safe as long as (1 << x_shift) < OUT_WIDTH, else it could overflow
	if (x_increment * OUT_WIDTH != (IN_WIDTH << x_shift)) x_increment ++;

	unsigned int y_shift = 0;
	while ((UINT_MAX >> (y_shift + 1)) > OUT_HEIGHT) y_shift ++;
	unsigned int y_increment = (IN_HEIGHT << y_shift) / OUT_HEIGHT;
	if (y_increment * OUT_HEIGHT != (IN_HEIGHT << y_shift)) y_increment ++;
	// The method below is also OK, but a bit more like rounding than truncation
	//  It produces a different output image!
	//unsigned int y_increment = (IN_HEIGHT << y_shift) / (OUT_HEIGHT - 1) - 1;

	TEST( {
		unsigned int y_out = 0;
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			unsigned int x_out = 0;
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
			{
				image_out[y][x] = image_in[y_out >> y_shift][x_out >> x_shift];
				x_out += x_increment;
			}
			y_out += y_increment;
		}
	} );
}

void method_best()
{
	// Testing ground for combining the best of all methods above
	// Use bitshift fixedpoint (method_5)
	unsigned int x_shift = 0;
	while ((UINT_MAX >> (x_shift + 1)) > OUT_WIDTH) x_shift ++;
	const unsigned int x_increment = (IN_WIDTH << x_shift) / OUT_WIDTH + 1;

	unsigned int y_shift = 0;
	while ((UINT_MAX >> (y_shift + 1)) > OUT_HEIGHT) y_shift ++;
	const unsigned int y_increment = (IN_HEIGHT << y_shift) / OUT_HEIGHT + 1;

	// now boost out the y index
	TEST( {
		unsigned int y_out = 0;
		for (unsigned int y=0; y<OUT_HEIGHT; ++y)
		{
			unsigned int x_out = 0;
			const unsigned int y_idx = y_out >> y_shift;
			for (unsigned int x=0; x<OUT_WIDTH; ++x)
			{
				image_out[y][x] = image_in[y_idx][x_out >> x_shift];
				x_out += x_increment;
			}
			y_out += y_increment;
		}
	} );
}

int main(int argc, char* argv[])
{
	printf("Integer scaling test\n\tGreg Kennedy 2016\n");

	if (argc > 1) iterations = atoi(argv[1]);
	if (iterations < 1) iterations = 1;
	printf("\tIterations: %u\n",iterations);

	// Make a IN_WIDTHxIN_HEIGHT test input image
	for (unsigned int y=0; y<IN_HEIGHT; ++y)
		for (unsigned int x=0; x<IN_WIDTH; ++x)
			image_in[y][x] = (0x00FF0000 * (y % 2 == 0)) |
						(0x000000FF * (x % 3 == 0)) |
						(0x0000FF00 * ((x + y) % 5 == 0));

	dump_ppm("image_in.ppm",(unsigned int*)image_in,IN_WIDTH,IN_HEIGHT);

	CALL_TEST(method_1);
	CALL_TEST(method_1b);
	CALL_TEST(method_1c);
	CALL_TEST(method_1d);
	CALL_TEST(method_1e);
	CALL_TEST(method_2);
	CALL_TEST(method_2b);
	CALL_TEST(method_2c);
	CALL_TEST(method_3);
	CALL_TEST(method_3b);
	CALL_TEST(method_4);
	CALL_TEST(method_5);

	CALL_TEST(method_best);

	return 0;
}
