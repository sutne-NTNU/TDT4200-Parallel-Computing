#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb/stb_image_write.h"

typedef struct pixel_struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} pixel;

void bilinear(pixel *Im, float row, float col, pixel *pix, int width, int height)
{
    int cm = (int)ceil(row);
    int fm = (int)floor(row);
    int cn = (int)ceil(col);
    int fn = (int)floor(col);
    double alpha = ceil(row) - row;
    double beta = ceil(col) - col;

    pix->r = (unsigned char)(alpha * beta * Im[fm * width + fn].r                 //
                             + (1 - alpha) * beta * Im[cm * width + fn].r         //
                             + alpha * (1 - beta) * Im[fm * width + cn].r         //
                             + (1 - alpha) * (1 - beta) * Im[cm * width + cn].r); //
    pix->g = (unsigned char)(alpha * beta * Im[fm * width + fn].g                 //
                             + (1 - alpha) * beta * Im[cm * width + fn].g         //
                             + alpha * (1 - beta) * Im[fm * width + cn].g         //
                             + (1 - alpha) * (1 - beta) * Im[cm * width + cn].g); //
    pix->b = (unsigned char)(alpha * beta * Im[fm * width + fn].b                 //
                             + (1 - alpha) * beta * Im[cm * width + fn].b         //
                             + alpha * (1 - beta) * Im[fm * width + cn].b         //
                             + (1 - alpha) * (1 - beta) * Im[cm * width + cn].b); //
    pix->a = 255;                                                                 //
}

void bilinear_kernel(pixel *d_pixels_in, pixel *d_pixels_out,
                     int in_width, int in_height,
                     int out_width, int out_height)
{
    for (int i = 0; i < out_height; i++)
    {
        for (int j = 0; j < out_width; j++)
        {
            pixel new_pixel;

            float row = i * (in_height - 1) / (float)out_height;
            float col = j * (in_width - 1) / (float)out_width;

            bilinear(d_pixels_in, row, col, &new_pixel, in_width, in_height);

            d_pixels_out[i * out_width + j] = new_pixel;
        }
    }
}

int main(int argc, char **argv)
{
    stbi_set_flip_vertically_on_load(true);
    stbi_flip_vertically_on_write(true);

    int in_width;
    int in_height;

    pixel *h_pixels_in;
    int channels;
    h_pixels_in = (pixel *)stbi_load(argv[1], &in_width, &in_height, &channels, STBI_rgb_alpha);
    if (h_pixels_in == NULL)
        exit(1);

    printf("Image dimensions: %dx%d\n", in_width, in_height);

    double scale_x = argc > 2 ? atof(argv[2]) : 1;
    double scale_y = argc > 3 ? atof(argv[3]) : 1;

    int out_width = in_width * scale_x;
    int out_height = in_height * scale_y;

    pixel *h_pixels_out = (pixel *)malloc(sizeof(pixel) * out_width * out_height);

    clock_t start = clock();
    bilinear_kernel(h_pixels_in, h_pixels_out, in_width, in_height, out_width, out_height);
    clock_t end = clock();

    float time = ((float)(end - start)) / CLOCKS_PER_SEC;
    printf("Time spent %.3f seconds\n", time);

    stbi_write_png("output.png", out_width, out_height, STBI_rgb_alpha, h_pixels_out, sizeof(pixel) * out_width);
    return 0;
}
