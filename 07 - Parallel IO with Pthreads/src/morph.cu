#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <pthread.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define WALLTIME(t) ((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)

using namespace std;

typedef struct pix
{
    unsigned char r, g, b, a;
} pixel;

typedef struct SimplePoint_struct
{
    float x, y;
} SimplePoint;

typedef struct SimpleFeatureLine_struct
{
    SimplePoint startPoint;
    SimplePoint endPoint;
} SimpleFeatureLine;


template <typename T>
__host__ __device__ T CLAMP(T value, T low, T high)
{
    return (value < low) ? low : ((value > high) ? high : value);
}

#define cudaErrorCheck(ans)                   \
    {                                         \
        gpuAssert((ans), __FILE__, __LINE__); \
    }

inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort = true)
{
    if (code == cudaSuccess) return;
    fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
    if (abort) exit(code);
}

//////////////////////////////////////////////////////////
// GLOBALS                                              //
int imageWidth, imageHeight, numLines, steps;           //
float p, a, b, t, stepSize;                             //
pixel *sourceImage, *destinationImage;                  //
SimpleFeatureLine *sourceLines, *destinationLines;      //
string outputPath;                                      //
pixel **morphedImages;                                  //
SimpleFeatureLine **allMorphLines;                      //
//////////////////////////////////////////////////////////

/** Using the total steps and the currently completed step to print a progressbar.
*  The progress overwrites itself until it reaches 100%.*/
void printProgress(const char *prefix, int step, int total)
{
    const int increments = 50;
    double percent_completed = 100.0 * (double)step / total;
    printf("\r%s: \t%.0f%%\t|", prefix, percent_completed);
    for (int i = 0; i < increments; ++i)
    {
        double bar_percent = 100.0 * (double)i / increments;
        printf(bar_percent < percent_completed ? "█" : " ");
    }
    printf("| %d/%d", step, total);
    fflush(stdout);
    if (step == total) printf("\n");
}

void imgRead(string filename, pixel *&map, int &imgW, int &imgH)
{
    if (filename.empty())
    {
        cout << "The input file name cannot be empty" << endl;
        exit(1);
    }
    stbi_set_flip_vertically_on_load(true);
    int x, y, componentsPerPixel;
    map = (pixel *)stbi_load(filename.c_str(), &x, &y, &componentsPerPixel, STBI_rgb_alpha);

    if (imgW != 0 || imgH != 0)
    {
        if (x != imgW || y != imgH)
        {
            fprintf(stderr, "Images must have equal dimensions! but was: %dx%d and %dx%d\n", imgW, imgH, x, y);
            exit(1);
        }
    }
    else
    {
        imgW = x;
        imgH = y;
    }
    cout << "Loaded image from: \t\"" << filename << "\"" << endl;
}

void imgWrite(string filename, pixel *map, int imgW, int imgH)
{
    if (filename.empty())
    {
        cout << "The output file name cannot be empty" << endl;
        exit(1);
    }
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), imgW, imgH, STBI_rgb_alpha, map, sizeof(pixel) * imgW);
}

void loadLines(const char *filename, SimpleFeatureLine *&linesSrc, SimpleFeatureLine *&linesDst, int *numLines)
{
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        printf("Error opening file %s! \n", filename);
        exit(1);
    }
    fscanf(f, "%d", numLines);
    linesSrc = (SimpleFeatureLine *)malloc(sizeof(SimpleFeatureLine) * (*numLines));
    linesDst = (SimpleFeatureLine *)malloc(sizeof(SimpleFeatureLine) * (*numLines));
    SimpleFeatureLine *line;
    int fac = 2;
    for (int i = 0; i < (*numLines) * fac; i++)
    {
        line = (i % fac) ? &linesDst[(i - 1) / fac] : &linesSrc[i / fac];
        fscanf(f, "%f,%f,%f,%f", &(line->startPoint.x), &(line->startPoint.y),
               &(line->endPoint.x), &(line->endPoint.y));
    }
    printf("Loaded %d lines from: \t\"%s\"\n", *numLines, filename);
}

void simpleLineInterpolate(SimpleFeatureLine *sourceLines,
                           SimpleFeatureLine *destLines,
                           SimpleFeatureLine **allMorphLines,
                           int numLines, float t)
{
    SimpleFeatureLine *interLines = (SimpleFeatureLine *)malloc(sizeof(SimpleFeatureLine) * numLines);
    for (int i = 0; i < numLines; i++)
    {
        interLines[i].startPoint.x = (1 - t) * (sourceLines[i].startPoint.x) //
                                     + t * (destLines[i].startPoint.x);      //
        interLines[i].startPoint.y = (1 - t) * (sourceLines[i].startPoint.y) //
                                     + t * (destLines[i].startPoint.y);      //
        interLines[i].endPoint.x = (1 - t) * (sourceLines[i].endPoint.x)     //
                                   + t * (destLines[i].endPoint.x);          //
        interLines[i].endPoint.y = (1 - t) * (sourceLines[i].endPoint.y)     //
                                   + t * (destLines[i].endPoint.y);          //
    }
    *allMorphLines = interLines;
}

__host__ __device__ void warp(const SimplePoint *interPt,
                              SimpleFeatureLine *interLines,
                              SimpleFeatureLine *sourceLines,
                              const int sourceLinesSize,
                              SimplePoint *src)
{
    int i;
    float interLength, srcLength;
    float weight, weightSum, dist;
    float sum_x, sum_y; // weighted sum of the coordination of the point "src"
    float u, v;
    SimplePoint pd, pq, qd;
    float X, Y;

    sum_x = 0;
    sum_y = 0;
    weightSum = 0;

    for (i = 0; i < sourceLinesSize; i++)
    {
        pd.x = interPt->x - interLines[i].startPoint.x;
        pd.y = interPt->y - interLines[i].startPoint.y;
        pq.x = interLines[i].endPoint.x - interLines[i].startPoint.x;
        pq.y = interLines[i].endPoint.y - interLines[i].startPoint.y;
        interLength = pq.x * pq.x + pq.y * pq.y;
        u = (pd.x * pq.x + pd.y * pq.y) / interLength;

        interLength = sqrt(interLength); // length of the vector PQ

        v = (pd.x * pq.y - pd.y * pq.x) / interLength;

        pq.x = sourceLines[i].endPoint.x - sourceLines[i].startPoint.x;
        pq.y = sourceLines[i].endPoint.y - sourceLines[i].startPoint.y;

        srcLength = sqrt(pq.x * pq.x + pq.y * pq.y); // length of the vector P'Q'
        // corresponding point based on the ith line
        X = sourceLines[i].startPoint.x + u * pq.x + v * pq.y / srcLength;
        Y = sourceLines[i].startPoint.y + u * pq.y - v * pq.x / srcLength;

        // the distance from the corresponding point to the line P'Q'
        if (u < 0)
            dist = sqrt(pd.x * pd.x + pd.y * pd.y);
        else if (u > 1)
        {
            qd.x = interPt->x - interLines[i].endPoint.x;
            qd.y = interPt->y - interLines[i].endPoint.y;
            dist = sqrt(qd.x * qd.x + qd.y * qd.y);
        }
        else
        {
            dist = abs(v);
        }

        weight = pow(1.0f / (1.0f + dist), 2.0f);
        sum_x += X * weight;
        sum_y += Y * weight;
        weightSum += weight;
    }

    src->x = sum_x / weightSum;
    src->y = sum_y / weightSum;
}

__host__ __device__ void bilinear(pixel *Im, float row, float col, pixel *pix, int dImgWidth)
{
    int cm = (int)ceil(row);
    int fm = (int)floor(row);
    int cn = (int)ceil(col);
    int fn = (int)floor(col);
    double alpha = ceil(row) - row;
    double beta = ceil(col) - col;
    pix->r = (unsigned int)(alpha * beta * Im[fm * dImgWidth + fn].r                 //
                            + (1 - alpha) * beta * Im[cm * dImgWidth + fn].r         //
                            + alpha * (1 - beta) * Im[fm * dImgWidth + cn].r         //
                            + (1 - alpha) * (1 - beta) * Im[cm * dImgWidth + cn].r); //
    pix->g = (unsigned int)(alpha * beta * Im[fm * dImgWidth + fn].g                 //
                            + (1 - alpha) * beta * Im[cm * dImgWidth + fn].g         //
                            + alpha * (1 - beta) * Im[fm * dImgWidth + cn].g         //
                            + (1 - alpha) * (1 - beta) * Im[cm * dImgWidth + cn].g); //
    pix->b = (unsigned int)(alpha * beta * Im[fm * dImgWidth + fn].b                 //
                            + (1 - alpha) * beta * Im[cm * dImgWidth + fn].b         //
                            + alpha * (1 - beta) * Im[fm * dImgWidth + cn].b         //
                            + (1 - alpha) * (1 - beta) * Im[cm * dImgWidth + cn].b); //
    pix->a = 255;
}

__host__ __device__ void ColorInterPolate(const SimplePoint *Src_P,
                                          const SimplePoint *Dest_P, float t,
                                          pixel *imgSrc, pixel *imgDest,
                                          pixel *rgb, int dImgWidth)
{
    pixel srcColor, destColor;

    bilinear(imgSrc, Src_P->y, Src_P->x, &srcColor, dImgWidth);
    bilinear(imgDest, Dest_P->y, Dest_P->x, &destColor, dImgWidth);

    rgb->b = srcColor.b * (1 - t) + destColor.b * t;
    rgb->g = srcColor.g * (1 - t) + destColor.g * t;
    rgb->r = srcColor.r * (1 - t) + destColor.r * t;
    rgb->a = 255;
}

/** Parses all arguments and reads the input images and lines */
void parseAndReadFiles(int argc, char *argv[])
{
    printf("\n");
    if (!(argc == 6 || argc == 9)) // has to be either 6 or 9
    {
        cout << "Usage: ./morph source.png destination.png lines.txt outputPath steps [p] [a] [b]" << endl;
        exit(1);
    }
    string fileSourceImage = argv[1];
    string fileDestinationImage = argv[2];
    string fileLines = argv[3];
    outputPath = argv[4];
    istringstream(argv[5]) >> steps;
    stepSize = 1.0 / steps;
    if (argc == 9)
    {
        istringstream(argv[6]) >> p;
        istringstream(argv[7]) >> a;
        istringstream(argv[8]) >> b;
    }
    else
    {
        p = 0;
        a = 1;
        b = 2;
    }
    t = 0.5;
    imgRead(fileSourceImage, sourceImage, imageWidth, imageHeight);
    imgRead(fileDestinationImage, destinationImage, imageWidth, imageHeight);
    loadLines(fileLines.c_str(), sourceLines, destinationLines, &numLines);
}

__global__ void morphKernel(SimpleFeatureLine *sourceLines,
                            SimpleFeatureLine *destinationLines,
                            SimpleFeatureLine *morphLines,
                            pixel *sourceImage,
                            pixel *destinationImage,
                            pixel *morphedImage,
                            int imageWidth, int imageHeight,
                            int numLines, float dT)
{
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    if (imageWidth <= x || imageHeight <= y)
        return; // Out of range of image

    extern __shared__ SimpleFeatureLine lines[]; // Shared memory for all feature lines
    // Split shared memory into three sections for the three feature lines
    SimpleFeatureLine *sSrcLines = &lines[0 * numLines];
    SimpleFeatureLine *sDstLines = &lines[1 * numLines];
    SimpleFeatureLine *sMrpLines = &lines[2 * numLines];

    // Each thread fills a specific index from the global to shared memory for its block
    // This assumes that there are more pixels/threads in each block than lines, which i would
    // say is a fair assumption (in Part 2 number of threads per block is 64, and numLines is 33)
    int blockIndex = threadIdx.y * blockDim.x + threadIdx.x;
    if (blockIndex < numLines)
    {
        sSrcLines[blockIndex] = sourceLines[blockIndex];
        sDstLines[blockIndex] = destinationLines[blockIndex];
        sMrpLines[blockIndex] = morphLines[blockIndex];
    }
    __syncthreads(); // wait for all shared memory to be ready in this block

    SimplePoint q{.x = float(x), .y = float(y)};
    SimplePoint src, dest;

    warp(&q, sMrpLines, sSrcLines, numLines, &src);
    warp(&q, sMrpLines, sDstLines, numLines, &dest);

    src.x = CLAMP<double>(src.x, 0, imageWidth - 1);
    src.y = CLAMP<double>(src.y, 0, imageHeight - 1);
    dest.x = CLAMP<double>(dest.x, 0, imageWidth - 1);
    dest.y = CLAMP<double>(dest.y, 0, imageHeight - 1);

    pixel interColor;
    ColorInterPolate(&src, &dest, dT, sourceImage, destinationImage, &interColor, imageWidth);

    int index = y * imageWidth + x;
    morphedImage[index].r = interColor.r;
    morphedImage[index].g = interColor.g;
    morphedImage[index].b = interColor.b;
    morphedImage[index].a = interColor.a;
}

void performMorphing(int argc, char *argv[])
{
    parseAndReadFiles(argc, argv);

    // Calculate all sizes
    size_t morphArrSize = sizeof(pixel *) * (steps + 1);
    size_t lineArrSize = sizeof(SimpleFeatureLine *) * (steps + 1);
    size_t imageSize = sizeof(pixel) * imageWidth * imageHeight;
    size_t lineSize = sizeof(SimpleFeatureLine) * numLines;
    // Shared memory will contain sourceLines, destinationLines and morphLines
    size_t sharedMemSize = 3 * lineSize;

    // Create arrays for all outputimages and all the morph lines
    morphedImages = (pixel **)malloc(morphArrSize);
    allMorphLines = (SimpleFeatureLine **)malloc(lineArrSize);
    for (int i = 0; i < steps + 1; i++)
    {
        morphedImages[i] = (pixel *)malloc(imageSize);
        simpleLineInterpolate(sourceLines, destinationLines, &(allMorphLines[i]), numLines, t);
    }

    // Allocate space on device (GPU) for lines and images
    pixel *dSourceImage, *dDestinationImage, *dMorphedImage;
    cudaMalloc((void **)&dSourceImage, imageSize);
    cudaMalloc((void **)&dDestinationImage, imageSize);
    cudaMalloc((void **)&dMorphedImage, imageSize);
    SimpleFeatureLine *dSourceLines, *dDestinationLines, *dMorphLines;
    cudaMalloc((void **)&dSourceLines, lineSize);
    cudaMalloc((void **)&dDestinationLines, lineSize);
    cudaMalloc((void **)&dMorphLines, lineSize);

    // Copy source and destination data to device
    cudaMemcpy(dSourceImage, sourceImage, imageSize, cudaMemcpyHostToDevice);
    cudaMemcpy(dSourceLines, sourceLines, lineSize, cudaMemcpyHostToDevice);
    cudaMemcpy(dDestinationImage, destinationImage, imageSize, cudaMemcpyHostToDevice);
    cudaMemcpy(dDestinationLines, destinationLines, lineSize, cudaMemcpyHostToDevice);

    // Defining Block and Grid Size
    dim3 blockSize(8, 8);
    int num_blocks_x = (imageWidth / blockSize.x);
    int num_blocks_y = (imageHeight / blockSize.y);
    dim3 gridSize(num_blocks_x, num_blocks_y);

    for (int i = 0; i < steps + 1; i++)
    {
        // Copy morph lines for this step to device
        cudaMemcpy(dMorphLines, allMorphLines[i], lineSize, cudaMemcpyHostToDevice);

        // Launching Kernel
        morphKernel<<<gridSize, blockSize, sharedMemSize>>>(
            dSourceLines, dDestinationLines, dMorphLines,
            dSourceImage, dDestinationImage, dMorphedImage,
            imageWidth, imageHeight, numLines, i * stepSize);

        // Copy morphed image for this step from device to host
        cudaMemcpy(morphedImages[i], dMorphedImage, imageSize, cudaMemcpyDeviceToHost);

        printProgress("Morphing Images", i + 1, steps + 1);
    }

    // Free all Cuda memory
    cudaFree(dSourceImage);
    cudaFree(dSourceLines);
    cudaFree(dDestinationImage);
    cudaFree(dDestinationLines);
    cudaFree(dMorphedImage);
    cudaFree(dMorphLines);
}



/////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////// ALL RELEVANT CODE IS BELOW /////////////////////////////////////////
//////////////// Above is only code from previous assignment ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////



// Struct for arguments for pthread for writing image to file
typedef struct Thread_struct
{
    string filename;
    pixel *image;
} Thread;

// Track progress of threads using a shared counter
int numImagesWritten = 0;
pthread_mutex_t mutex;

/** Writes a single image to file */
void *pthread_write_image(void *args)
{
    // Deconstruct arguments
    Thread thread = *(Thread *)args;
    // Write to file 
    imgWrite(thread.filename, thread.image, imageWidth, imageHeight);

    // Update counter and print current progress
    pthread_mutex_lock(&mutex);
    {
        numImagesWritten += 1;
        printProgress("\tpthreads", numImagesWritten, steps + 1);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

/**
 * MAIN
 * 
 * performs the morphing then writes the morphed images to file, 
 * first serially, then using pthreads
 */
int main(int argc, char *argv[])
{
    performMorphing(argc, argv);
    printf("Writing To File:\n");

    // SERIAL //////////////////////////////////////////////////////////////////////
    struct timeval start, end;   
    gettimeofday(&start, NULL);
    {
        // Write the morphed images to file and free the host memory
        printProgress("\tSerial", 0, steps + 1);
        for (int i = 0; i < steps + 1; i++)
        {
            string filename = outputPath + to_string(stepSize * i) + ".png";
            imgWrite(filename, morphedImages[i], imageWidth, imageHeight);
            printProgress("\tSerial", i + 1, steps + 1);
        }
    }
    gettimeofday(&end, NULL);
    double serial_time = WALLTIME(end) - WALLTIME(start);
    printf("\tTime: \t%.2f seconds\n", serial_time);

        
    
    // PTHREADS ////////////////////////////////////////////////////////////////////
    gettimeofday(&start, NULL);
    {
        // Allocate space for threads and their argments
        pthread_t thread_id[steps + 1];
        Thread thread_args[steps + 1];

        printProgress("\tpthreads", 0, steps + 1);
        for (int i = 0; i < steps + 1; i++)
        {
            // Set the arguments
            thread_args[i].filename = outputPath + to_string(stepSize * i) + ".png";
            thread_args[i].image = morphedImages[i];
            // Create thread that writes an image to file
            pthread_create(&thread_id[i], NULL, &pthread_write_image, &thread_args[i]);
        }

        // Join the threads (wait until everyone is finished)
        for (int i = 0; i < steps + 1; i++) pthread_join(thread_id[i], NULL);
    }
    gettimeofday(&end, NULL);
    double pthread_time = WALLTIME(end) - WALLTIME(start);
    printf("\tTime: \t%.2f seconds \t(%.2f x Faster)\n", pthread_time, serial_time/pthread_time);



    // Free all memory
    for(int i = 0; i < steps + 1; i++)
    {
        free(morphedImages[i]);
        free(allMorphLines[i]);
    }
    free(morphedImages);
    free(allMorphLines);

    return 0;
}
