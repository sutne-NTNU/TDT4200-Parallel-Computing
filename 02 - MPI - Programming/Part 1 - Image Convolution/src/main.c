#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <image_utils.h>
#include <argument_utils.h>
#include <mpi.h>

/**
 * Using the total total_iterations and the current iteration to print a progressbar. 
 * The progress should overwrite itself until it reaches 100%.
 */
void printProgress(int iteration, int total_iterations)
{
    const int increments = 100;
    char prefix[100], suffix[100];
    char progress[increments + 1];

    double percent_completed = iteration / (double)total_iterations * 100;

    sprintf(prefix, "Morphing images: [");
    sprintf(suffix, "] %.1f%%", percent_completed);

    progress[0] = '\0';
    for (int i = 0; i < increments; ++i)
    {
        double bar_percent = (i / (double)increments) * 100;
        sprintf(progress, "%s%s", progress, percent_completed > bar_percent ? "#" : " ");
    }

    printf("\r%s%s%s", prefix, progress, suffix);
    fflush(stdout);
    if (iteration == total_iterations) // Completed
    {
        printf("\n");
    }
}

// Apply convolutional kernel on image data
void applyKernel(pixel **out, pixel **in, unsigned int width, unsigned int height, int *kernel, unsigned int kernelDim, float kernelFactor)
{
    unsigned int const kernelCenter = (kernelDim / 2);
    for (unsigned int imageY = 0; imageY < height; imageY++)
    {
        for (unsigned int imageX = 0; imageX < width; imageX++)
        {
            unsigned int ar = 0, ag = 0, ab = 0;
            for (unsigned int kernelY = 0; kernelY < kernelDim; kernelY++)
            {
                int nky = kernelDim - 1 - kernelY;
                for (unsigned int kernelX = 0; kernelX < kernelDim; kernelX++)
                {
                    int nkx = kernelDim - 1 - kernelX;

                    int yy = imageY + (kernelY - kernelCenter);
                    int xx = imageX + (kernelX - kernelCenter);
                    if (xx >= 0 && xx < (int)width && yy >= 0 && yy < (int)height)
                    {
                        ar += in[yy][xx].r * kernel[nky * kernelDim + nkx];
                        ag += in[yy][xx].g * kernel[nky * kernelDim + nkx];
                        ab += in[yy][xx].b * kernel[nky * kernelDim + nkx];
                    }
                }
            }
            if (ar || ag || ab)
            {
                ar *= kernelFactor;
                ag *= kernelFactor;
                ab *= kernelFactor;
                out[imageY][imageX].r = (ar > 255) ? 255 : ar;
                out[imageY][imageX].g = (ag > 255) ? 255 : ag;
                out[imageY][imageX].b = (ab > 255) ? 255 : ab;
                out[imageY][imageX].a = 255;
            }
            else
            {
                out[imageY][imageX].r = 0;
                out[imageY][imageX].g = 0;
                out[imageY][imageX].b = 0;
                out[imageY][imageX].a = 255;
            }
        }
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int world_size, my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    const int ROOT_RANK = 0;
    const int LAST_RANK = world_size - 1;

    OPTIONS my_options;
    OPTIONS *options = &my_options;

    if (my_rank == ROOT_RANK)
    {
        options = parse_args(argc, argv);
        if (options == NULL)
        {
            fprintf(stderr, "Options == NULL\n");
            exit(1);
        }
    }

    MPI_Bcast(options, sizeof(OPTIONS), MPI_BYTE, ROOT_RANK, MPI_COMM_WORLD);

    if (my_rank != ROOT_RANK)
    {
        options->input = NULL;
        options->output = NULL;
    }

    image_t image_object = {.rawdata = NULL, .data = NULL};
    image_t *image = &image_object;

    if (my_rank == ROOT_RANK)
    {
        image = loadImage(options->input);
        if (image == NULL)
        {
            fprintf(stderr, "Could not load image '%s'!\n", options->input);
            freeImage(image);
            abort();
        }
        printf(
            "\nApply kernel '%s' on image with %u x %u pixels for %u iterations\n",
            kernelNames[options->kernelIndex],
            image->width,
            image->height,
            options->iterations //
        );
    }

    MPI_Bcast(
        image,           // Send Buffer
        sizeof(image_t), // Send Count
        MPI_BYTE,        // Send Type
        ROOT_RANK,       // Root
        MPI_COMM_WORLD   // Communicator
    );

    //////////////////////////////////////////////////////////
    // Calculate how much of the image to send to each rank //
    //////////////////////////////////////////////////////////

    int rows_to_receive[world_size];
    int bytes_to_transfer[world_size];
    int displacements[world_size];
    displacements[0] = 0;

    int rows_per_rank = image->height / world_size;
    int remainder_rows = image->height % world_size;

    for (int i = 0; i < world_size; i++)
    {
        int rows_this_rank = rows_per_rank;
        if (i < remainder_rows)
        {
            rows_this_rank++;
        }
        int bytes_this_rank = rows_this_rank * (sizeof(pixel) * image->width);
        rows_to_receive[i] = rows_this_rank;
        bytes_to_transfer[i] = bytes_this_rank;
        if (i != 0)
        {
            displacements[i] = displacements[i - 1] + bytes_to_transfer[i - 1];
        }
    }
    // rows i need from each of my neighbours
    const int num_border_rows = (kernelDims[options->kernelIndex] - 1) / 2;

    // Height of my partition of the image
    const int my_partition_height = rows_to_receive[my_rank];

    //////////////////////////////////////////////////////////////////////////
    // Make space for border-exchange                                       //
    // ------------------------------------------------------------         //
    // This should include space for the rows that are to be exchanged both //
    // at the top and at the bottom of each respective partition.           //
    //////////////////////////////////////////////////////////////////////////

    // First and last process only need space for border on one side, the others need two sides
    const int total_halo_rows = (my_rank == ROOT_RANK || my_rank == LAST_RANK ? 1 : 2) * num_border_rows;
    // my_image contains space for my partition + space for appropriate border-exhange
    image_t *my_image = newImage(image->width, my_partition_height + total_halo_rows);
    // Total number of pixels in my partition without the border-exchange
    const int num_pixels_in_my_partition = my_image->width * my_partition_height;
    // Number of pixels one each side for the border- exchange
    const int num_border_pixels = num_border_rows * my_image->width;
    // number of bytes for each side of the border-exchange
    const size_t num_border_bytes = sizeof(pixel) * num_border_pixels;

    ///////////////////////////////////////////////////////////////////////////
    // The recv buffer pointer.                                              //
    //-----------------------------------------------------------------------//
    // Should point to the start of where this rank's partition of the image //
    // starts. The topmost and bottom-most rows should not be written by the //
    // scatter operation                                                     //
    ///////////////////////////////////////////////////////////////////////////

    // The pixels in my partition of the image
    pixel *my_pixels = my_image->rawdata + (my_rank == ROOT_RANK ? 0 : num_border_pixels);

    // Scatter partition of the original image from root to my pixels
    MPI_Scatterv(
        image->rawdata,             // Send Buffer
        bytes_to_transfer,          // Send Counts
        displacements,              // Displacements
        MPI_BYTE,                   // Send Type
        my_pixels,                  // Recv Buffer
        bytes_to_transfer[my_rank], // Recv Count
        MPI_BYTE,                   // Recv Type
        ROOT_RANK,                  // Root
        MPI_COMM_WORLD              // Communicator
    );

    ///////////////////////////////////
    // time measurement from here    //
    ///////////////////////////////////

    double starttime, endtime;
    if (my_rank == ROOT_RANK)
    {
        starttime = MPI_Wtime();
    }

    // Image to write values to when applying the kernel
    image_t *process_image = newImage(my_image->width, my_image->height);

    // I am thinking of the pixels as a continous list of pixels (as they are in memory).
    // The "halo"/border is therefore only some pixels in front of, or behind, my own pixels/partition. This
    // will be the process in front of me's last pixels, or the process behind me's first pixels.
    // The pointers below must be updated inside the loop because the pointer to my_image swaps between
    // the process_image and my_image to prevent reading and writitng to the same image at the same time.
    int neighbour_in_front_of_me = my_rank - 1; // Not used for ROOT_RANK
    int neighbour_behind_me = my_rank + 1;      // Not used for LAST_RANK

    // Neighbour in front of me's last pixels
    pixel *pixels_in_front_of_me;
    // Neighbour behind me's first pixels
    pixel *pixels_behind_me;
    // Pixels in front of the neighbour behind me
    pixel *my_last_pixels;

    // Perform the iterations with the kernel
    for (unsigned int i = 0; i < options->iterations; i++)
    {
        if (world_size > 1) // no need to send any data unless the image is partitioned
        {
            /////////////////////
            // border-exchange //
            /////////////////////
            pixels_in_front_of_me = (my_rank == ROOT_RANK) ? NULL : my_image->rawdata;
            my_pixels = (my_rank == ROOT_RANK) ? my_image->rawdata : pixels_in_front_of_me + num_border_pixels;
            pixels_behind_me = my_pixels + num_pixels_in_my_partition;
            my_last_pixels = pixels_behind_me - num_border_pixels;

            if (my_rank == ROOT_RANK) // I only have one neighbour behind me
            {
                // Send my neighbour the pixels in front of them
                MPI_Send(my_last_pixels, num_border_bytes, MPI_BYTE, neighbour_behind_me, 1, MPI_COMM_WORLD);
                // Receive the pixels behind me (which is their first pixels)
                MPI_Recv(pixels_behind_me, num_border_bytes, MPI_BYTE, neighbour_behind_me, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else if (my_rank != LAST_RANK) // I have two neighbours, one in front of me, and one behind me
            {
                // I have to get the pixels in front of me
                MPI_Recv(pixels_in_front_of_me, num_border_bytes, MPI_BYTE, neighbour_in_front_of_me, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // and in gratitude i return the pixels behind them
                MPI_Send(my_pixels, num_border_bytes, MPI_BYTE, neighbour_in_front_of_me, 1, MPI_COMM_WORLD);

                // Then i am spreading the love by sending the neighour behind me the pixels in front of them
                MPI_Send(my_last_pixels, num_border_bytes, MPI_BYTE, neighbour_behind_me, 1, MPI_COMM_WORLD);
                // Hopefully they are in a good mood, and i can receive the pixels that are behind me
                MPI_Recv(pixels_behind_me, num_border_bytes, MPI_BYTE, neighbour_behind_me, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else // I only have one neighbour in front of me
            {
                // My neighbour was very kind and sent me the pixels in front of me
                MPI_Recv(pixels_in_front_of_me, num_border_bytes, MPI_BYTE, neighbour_in_front_of_me, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                // I should return the favor and send them the pixels behind them
                MPI_Send(my_pixels, num_border_bytes, MPI_BYTE, neighbour_in_front_of_me, 1, MPI_COMM_WORLD);
            }
        }

        applyKernel(
            process_image->data,                // Out
            my_image->data,                     // In
            my_image->width,                    // Width
            my_image->height,                   // Height
            kernels[options->kernelIndex],      // Kernel
            kernelDims[options->kernelIndex],   // Kernel-Dimensions
            kernelFactors[options->kernelIndex] // Kernel-Factor
        );

        swapImage(&process_image, &my_image);

        // We have to wait until all processes finish this iteration to use their new pixels in next iteration
        MPI_Barrier(MPI_COMM_WORLD);

        if (my_rank == ROOT_RANK)
        {
            printProgress(i + 1, options->iterations);
        }
    }
    freeImage(process_image);

    ///////////////////////////////////////////////////////////
    // Update the "Send Buffer" pointer such that it points  //
    // to the starting location in each respective partition.//
    ///////////////////////////////////////////////////////////
    my_pixels = my_image->rawdata + (my_rank == ROOT_RANK ? 0 : num_border_pixels);

    // Gather and merge all partitions in `image->rawdata`
    MPI_Gatherv(
        my_pixels,                  // Send Buffer
        bytes_to_transfer[my_rank], // Send Count
        MPI_BYTE,                   // Send Type
        image->rawdata,             // Recv Buffer
        bytes_to_transfer,          // Recv Counts
        displacements,              // Recv Displacements
        MPI_BYTE,                   // Recv Type
        ROOT_RANK,                  // Root
        MPI_COMM_WORLD              // Communicator
    );
    freeImage(my_image);

    //////////////////////////////
    // time measurement to here //
    //////////////////////////////
    if (my_rank == ROOT_RANK)
    {
        endtime = MPI_Wtime();
        printf("%d Processes used: %f seconds\n", world_size, endtime - starttime);

        int status = saveImage(image, options->output);
        freeImage(image);
        if (status < 1)
        {
            fprintf(stderr, "Could not save output to '%s'!\n", options->output);
            abort();
        };
    }
    MPI_Finalize();

graceful_exit:
    options->ret = 0;
error_exit:
    if (options->input != NULL)
        free(options->input);
    if (options->output != NULL)
        free(options->output);
    return options->ret;
};
