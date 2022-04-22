#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb/stb_image_write.h"

typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} pixel;

int main(int argc, char **argv)
{
    stbi_set_flip_vertically_on_load(true);
    stbi_flip_vertically_on_write(true);

    int width;
    int height;
    int channels;

    unsigned char *char_pixels_1;
    unsigned char *char_pixels_2;

    bool arguments_provided = argc > 1;
    if (arguments_provided)
    {
        char_pixels_1 = stbi_load(argv[1], &width, &height, &channels, STBI_rgb_alpha);
        char_pixels_2 = stbi_load(argv[2], &width, &height, &channels, STBI_rgb_alpha);
    }
    else
    {
        char_pixels_1 = stbi_load("input_1.png", &width, &height, &channels, STBI_rgb_alpha);
        char_pixels_2 = stbi_load("input_2.png", &width, &height, &channels, STBI_rgb_alpha);
    }

    if (char_pixels_1 == NULL || char_pixels_2 == NULL)
    {
        exit(1);
    }
    //Task 2
    pixel *pixels_1 = (pixel *)char_pixels_1;
    pixel *pixels_2 = (pixel *)char_pixels_2;

    //Task 3
    int nr_of_pixels = height * width;
    pixel *pixels_out = malloc(sizeof(pixel) * nr_of_pixels);
    if (pixels_out == NULL)
    {
        exit(1);
    }

    //Task 4
    for (size_t i = 0; i < nr_of_pixels; i++)
    {
        pixels_out[i].r = (pixels_1[i].r + pixels_2[i].r) / 2;
        pixels_out[i].g = (pixels_1[i].g + pixels_2[i].g) / 2;
        pixels_out[i].b = (pixels_1[i].b + pixels_2[i].b) / 2;
        pixels_out[i].a = 255;
    }
    stbi_write_png("output.png", width, height, STBI_rgb_alpha, pixels_out, sizeof(pixel) * width);

    //Task 5
    free(pixels_1);
    free(pixels_2);
    free(pixels_out);

    return 0;
}
