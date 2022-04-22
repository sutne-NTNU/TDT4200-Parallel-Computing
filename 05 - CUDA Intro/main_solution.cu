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

#define cudaErrCheck(ans)                     \
    {                                         \
        gpuAssert((ans), __FILE__, __LINE__); \
    }

inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort = true)
{
    if (code != cudaSuccess)
    {
        fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
        if (abort)
            exit(code);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

__device__ void bilinear( // TODO 2 b: Change to device function
    pixel *image,
    float row,
    float col,
    pixel *pixel,
    int width,
    int height)
{
    int cm = (int)ceil(row);
    int fm = (int)floor(row);
    int cn = (int)ceil(col);
    int fn = (int)floor(col);
    double alpha = ceil(row) - row;
    double beta = ceil(col) - col;

    pixel->r = (unsigned char)(alpha * beta * image[fm * width + fn].r                 //
                               + (1 - alpha) * beta * image[cm * width + fn].r         //
                               + alpha * (1 - beta) * image[fm * width + cn].r         //
                               + (1 - alpha) * (1 - beta) * image[cm * width + cn].r); //
    pixel->g = (unsigned char)(alpha * beta * image[fm * width + fn].g                 //
                               + (1 - alpha) * beta * image[cm * width + fn].g         //
                               + alpha * (1 - beta) * image[fm * width + cn].g         //
                               + (1 - alpha) * (1 - beta) * image[cm * width + cn].g); //
    pixel->b = (unsigned char)(alpha * beta * image[fm * width + fn].b                 //
                               + (1 - alpha) * beta * image[cm * width + fn].b         //
                               + alpha * (1 - beta) * image[fm * width + cn].b         //
                               + (1 - alpha) * (1 - beta) * image[cm * width + cn].b); //
    pixel->a = 255;                                                                    //
}
/////////////////////////////////////////////////////////////////////////////////////////

__global__ void bilinear_kernel( // TODO 2 a: Change to kernel
    pixel *device_pixels_in,
    pixel *device_pixels_out,
    int in_width,
    int in_height,
    int out_width,
    int out_height)
{
    // TODO 2 c - Parallelize the kernel ////////////////////////////////////////////////
    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;

    bool pixel_out_of_range = (out_width <= x) || (out_height <= y);
    if (pixel_out_of_range)
        return;

    pixel new_pixel;
    float col = x * (in_width - 1) / (float)out_width;
    float row = y * (in_height - 1) / (float)out_height;
    bilinear(device_pixels_in, row, col, &new_pixel, in_width, in_height);
    device_pixels_out[y * out_width + x] = new_pixel;
    /////////////////////////////////////////////////////////////////////////////////////
}

int main(int argc, char **argv)
{
    stbi_set_flip_vertically_on_load(true);
    stbi_flip_vertically_on_write(true);

    int in_width, in_height, channels;
    pixel *host_pixels_in;

    host_pixels_in = (pixel *)stbi_load(argv[1], &in_width, &in_height, &channels, STBI_rgb_alpha);
    if (host_pixels_in == NULL)
    {
        exit(1);
    }
    printf("Image dimensions: %dx%d\n", in_width, in_height);

    double scale_x = argc > 2 ? atof(argv[2]) : 2;
    double scale_y = argc > 3 ? atof(argv[3]) : 8;

    int out_width = in_width * scale_x;
    int out_height = in_height * scale_y;

    long size_in = sizeof(pixel) * in_width * in_height;
    long size_out = sizeof(pixel) * out_width * out_height;

    pixel *host_pixels_out = (pixel *)malloc(size_out);

    // TODO 1 a - cuda malloc ///////////////////////////////////////////////////////////
    pixel *device_pixels_in;
    pixel *device_pixels_out;
    cudaMalloc((void **)&device_pixels_in, size_in);
    cudaMalloc((void **)&device_pixels_out, size_out);
    ////////////////////////////////////////////////////////////////////////////////////

    cudaEvent_t start_transfer, stop_transfer;
    cudaEventCreate(&start_transfer);
    cudaEventCreate(&stop_transfer);
    cudaEventRecord(start_transfer);

    // TODO 1 b - cuda memcpy /////////////////////////////////////////////////////////
    cudaMemcpy(device_pixels_in, host_pixels_in, size_in, cudaMemcpyHostToDevice);
    ///////////////////////////////////////////////////////////////////////////////////

    // TODO 1 c - block size and grid size. ///////////////////////////////////////////
    // gridSize should depend on the blockSize and output dimensions.
    dim3 blockSize(32, 32);
    int num_blocks_x = (out_width / blockSize.x);
    int num_blocks_y = (out_height / blockSize.y);
    dim3 gridSize(num_blocks_x, num_blocks_y);
    ///////////////////////////////////////////////////////////////////////////////////

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    // TODO 2 a - GPU computation /////////////////////////////////////////////////////
    // Change the function call so that it becomes a kernel call. Change the input
    // and output pixel variables to be device-side instead of host-side.
    bilinear_kernel<<<gridSize, blockSize>>>(device_pixels_in, device_pixels_out, in_width, in_height, out_width, out_height);
    ////////////////////////////////////////////////////////////////////////////////////

    cudaEventRecord(stop);
    cudaDeviceSynchronize();
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
        printf("%s\n", cudaGetErrorString(err));
    cudaDeviceSynchronize();
    cudaEventSynchronize(stop);
    float spentTime = 0.0;
    cudaEventElapsedTime(&spentTime, start, stop);
    printf("Time spent %.3f seconds\n", spentTime / 1000);

    // TODO 3 a - Copy the device-side data into the host-side variable ////////////////
    cudaMemcpy(host_pixels_out, device_pixels_out, size_out, cudaMemcpyDeviceToHost);
    ////////////////////////////////////////////////////////////////////////////////////

    cudaEventRecord(stop_transfer);
    cudaEventSynchronize(stop_transfer);
    float spentTimeTransfer = 0.0;
    cudaEventElapsedTime(&spentTimeTransfer, start_transfer, stop_transfer);
    printf("Time spent including transfer: %.3f seconds\n", spentTimeTransfer / 1000);
    stbi_write_png("output.png", out_width, out_height, STBI_rgb_alpha, host_pixels_out, sizeof(pixel) * out_width);

    // TODO 3 b - Free heap-allocated memory on device and host ////////////////////////
    free(host_pixels_in);
    free(host_pixels_out);
    cudaFree(device_pixels_in);
    cudaFree(device_pixels_out);
    ////////////////////////////////////////////////////////////////////////////////////

    return 0;
}
