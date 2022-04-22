#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include "mpi.h"

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

void bilinear(pixel *image, float row, float col, pixel *new_pixel, int width, int height)
{
	int cm = (int)ceil(row);
	int fm = (int)floor(row);
	int cn = (int)ceil(col);
	int fn = (int)floor(col);
	double alpha = ceil(row) - row;
	double beta = ceil(col) - col;

	new_pixel->r = (unsigned char)(alpha * beta * image[fm * width + fn].r				 //
								   + (1 - alpha) * beta * image[cm * width + fn].r		 //
								   + alpha * (1 - beta) * image[fm * width + cn].r		 //
								   + (1 - alpha) * (1 - beta) * image[cm * width + cn].r //
	);
	new_pixel->g = (unsigned char)(alpha * beta * image[fm * width + fn].g				 //
								   + (1 - alpha) * beta * image[cm * width + fn].g		 //
								   + alpha * (1 - beta) * image[fm * width + cn].g		 //
								   + (1 - alpha) * (1 - beta) * image[cm * width + cn].g //
	);
	new_pixel->b = (unsigned char)(alpha * beta * image[fm * width + fn].b				 //
								   + (1 - alpha) * beta * image[cm * width + fn].b		 //
								   + alpha * (1 - beta) * image[fm * width + cn].b		 //
								   + (1 - alpha) * (1 - beta) * image[cm * width + cn].b //
	);
	new_pixel->a = 255;
}

void save_partition(int rank, int w, int h, pixel *buffer)
{
	char *out_file;
	if (rank == 0)
		out_file = "rank-0.png";
	if (rank == 1)
		out_file = "rank-1.png";
	if (rank == 2)
		out_file = "rank-2.png";
	if (rank == 3)
		out_file = "rank-3.png";
	stbi_write_png(out_file, w, h, STBI_rgb_alpha, buffer, sizeof(pixel) * w);
}

/// malloc for the amount of pixels given, if allocation fails program exits.
pixel *malloc_pixels(int amount)
{
	pixel *allocated = (pixel *)malloc(sizeof(pixel) * amount);
	if (allocated == NULL)
	{
		printf("Memory allocation failed for %d pixels\n", amount);
		exit(1);
	}
	return allocated;
}

int main(int argc, char **argv)
{
	stbi_set_flip_vertically_on_load(true);
	stbi_flip_vertically_on_write(true);

	char *filename = argv[1];
	double width_scale = argc > 2 ? atof(argv[2]) : 2;
	double height_scale = argc > 3 ? atof(argv[3]) : 8;

	// TODO 1 _________________________________________________________________________________________________
	// Initialize the MPI environment and retrieve the size of the MPI COMM WORLD communicator, as
	// well as each process’ rank within this communicator. Finalize MPI at the end of main() before
	// it returns.
	MPI_Init(NULL, NULL);
	int comm_size, rank;
	MPI_Comm comm = MPI_COMM_WORLD;
	MPI_Comm_size(comm, &comm_size);
	MPI_Comm_rank(comm, &rank);
	// TODO END _______________________________________________________________________________________________

	// TODO 2 _________________________________________________________________________________________________
	// Only the rank 0 process should read the image. Broadcast the dimensions, as well as the image
	// itself, from rank 0 to the other processes.
	int root_rank = 0;
	pixel *image;
	int image_width, image_height, channels;

	if (rank == root_rank)
	{
		image = (pixel *)stbi_load(argv[1], &image_width, &image_height, &channels, STBI_rgb_alpha);
		if (image == NULL)
		{
			printf("error loading image\n");
			exit(1);
		}
	}

	MPI_Bcast(&image_width, 1, MPI_INT, root_rank, comm);
	MPI_Bcast(&image_height, 1, MPI_INT, root_rank, comm);
	if (rank != root_rank)
	{
		image = malloc_pixels(image_width * image_height);
	}
	MPI_Bcast(image, sizeof(pixel) * image_height * image_width, MPI_BYTE, root_rank, comm);
	// TODO END _______________________________________________________________________________________________

	// TODO 3 _________________________________________________________________________________________________
	// While each process has access to the whole input, they should only produce part of the output.
	// Set the local dimensions to partition the output evenly among the processes. Allocate space
	// for the local partial output.
	int scaled_width = image_width * width_scale;
	int scaled_height = image_height * height_scale;

	int local_width = image_width;
	int local_height = image_height / comm_size;
	int local_scaled_width = local_width * width_scale;
	int local_scaled_height = local_height * height_scale;

	pixel *scaled_partition = malloc_pixels(local_scaled_width * local_scaled_height);
	// TODO END _______________________________________________________________________________________________

	// TODO 4 _________________________________________________________________________________________________
	// Perform the computation. You need to iterate through pixels in the local partition of the
	// output, and save the output from the bilinear() function accordingly. Note, however, that
	// bilinear() expects global row and column coordinates for the image. This means you need to find a
	// mapping between the local and global indices when you calculate the variables ”image_row” and ”image_column”.
	int offset = rank * local_height;
	for (int column = 0; column < local_scaled_width; column++)
	{
		for (int row = 0; row < local_scaled_height; row++)
		{
			pixel new_pixel;
			float image_column = (image_width - 1) / (float)scaled_width * column;
			float image_row = (image_height - 1) / (float)scaled_height * row + offset;
			bilinear(image, image_row, image_column, &new_pixel, image_width, image_height);
			scaled_partition[local_scaled_width * row + column] = new_pixel;
		}
	}
	free(image);
	// TODO END _______________________________________________________________________________________________

	// TODO 5 _________________________________________________________________________________________________
	// Allocate space in rank 0 for a complete output image and gather the results from all the
	// processes. If you have assumed image dimensions that are divisible by the number of processes,
	// you can use MPI Gather(), otherwise you need to use MPI Gatherv(). Only rank 0 should write
	// the image to file.
	pixel *scaled_image = NULL;
	if (rank == root_rank)
	{
		scaled_image = malloc_pixels(scaled_height * scaled_width);
	}

	int send_count = sizeof(pixel) * local_scaled_width * local_scaled_height;
	int receive_count = send_count;
	MPI_Gather(
		// SEND
		scaled_partition, //
		send_count,		  //
		MPI_BYTE,		  //
		// RECEIVE
		scaled_image,  //
		receive_count, //
		MPI_BYTE,	   //
		// Master
		root_rank, //
		comm	   //
	);
	free(scaled_partition);

	if (rank == root_rank)
	{
		stbi_write_png("output.png", scaled_width, scaled_height, STBI_rgb_alpha, scaled_image, sizeof(pixel) * scaled_width);
		free(scaled_image);
	}
	// TODO END _______________________________________________________________________________________________

	// TODO 1 _________________________________________________________________________________________________
	MPI_Finalize();
	if (rank == root_rank)
	{
		printf("original=%dx%d -> scaled=%dx%d\n", image_width, image_height, scaled_width, scaled_height);
	}
	// TODO END _______________________________________________________________________________________________
	return 0;
}
