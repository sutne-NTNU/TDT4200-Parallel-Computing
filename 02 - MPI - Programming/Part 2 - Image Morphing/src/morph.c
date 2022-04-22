#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <morph.h>
#include <mpi.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define true 1
#define false 0

int world_size;
int world_rank;
const int ROOT = 0;

// Number of steps for the morphing
int steps;
// Height of this ranks slice in numer of pixels
int mySliceHeight;
// number of vertical rows to skip in original image to get to my slice
int myHeightOffset;

SimpleFeatureLine *hSrcLines;
SimpleFeatureLine *hDstLines;

/**
 * Using the total steps and the current count (completed steps) to print
 * a progressbar. The progress should overwrite itself until it reaches 100%.
 */
void printProgress(int count, int steps)
{
    const int increments = 100;
    char prefix[100], suffix[100];
    char progress[increments + 1];

    double percent_completed = count / (double)steps * 100;

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
    if (count == steps) // Completed
    {
        printf("\n");
    }
}

double CLAMP(double value, double low, double high)
{
    return (value < low) ? low : ((value > high) ? high : value);
}

void imgRead(const char *filename, pixel **map, int *imgW, int *imgH)
{
    stbi_set_flip_vertically_on_load(true);

    unsigned char *pixelMap;
    int x, y, componentsPerPixel;
    if (strlen(filename) > 0)
    {
        *map = (pixel *)stbi_load(filename, &x, &y, &componentsPerPixel, STBI_rgb_alpha);
    }
    else
    {
        printf("The input file name cannot be empty\n");
        exit(1);
    }
    *imgW = x;
    *imgH = y;
}

void imgWrite(const char *filename, pixel *map, int imgW, int imgH)
{
    if (strlen(filename) < 1)
    {
        printf("The output file name cannot be empty\n");
        exit(1);
    }
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename, imgW, imgH, STBI_rgb_alpha, map, sizeof(pixel) * imgW);
}

void simpleLineInterpolate(
    SimpleFeatureLine **morphLines, //
    int numLines,                   //
    float t                         //
)
{
    SimpleFeatureLine *interLines = malloc(sizeof(SimpleFeatureLine) * (numLines));
    if (interLines == NULL)
    {
        fprintf(stderr, "Failed to allocate interLines\n");
        exit(1);
    }

    for (int i = 0; i < numLines; i++)
    {
        interLines[i].startPoint.x = (1 - t) * (hSrcLines[i].startPoint.x) //
                                     + t * (hDstLines[i].startPoint.x);
        interLines[i].startPoint.y = (1 - t) * (hSrcLines[i].startPoint.y) //
                                     + t * (hDstLines[i].startPoint.y);
        interLines[i].endPoint.x = (1 - t) * (hSrcLines[i].endPoint.x) //
                                   + t * (hDstLines[i].endPoint.x);
        interLines[i].endPoint.y = (1 - t) * (hSrcLines[i].endPoint.y) //
                                   + t * (hDstLines[i].endPoint.y);
    }
    *morphLines = interLines;
}

SimpleFeatureLine **loadLines(int *numLines, const char *name)
{
    FILE *f = fopen(name, "r");
    if (f == NULL)
    {
        fprintf(stderr, "Error opening file!\n");
        exit(1);
    }
    fscanf(f, "%d", numLines);
    char c;
    c = getc(f);

    size_t lineArraySize = sizeof(SimpleFeatureLine) * (*numLines);
    SimpleFeatureLine *linesSrc = (SimpleFeatureLine *)malloc(lineArraySize);
    SimpleFeatureLine *linesDst = (SimpleFeatureLine *)malloc(lineArraySize);
    SimpleFeatureLine **pairs = (SimpleFeatureLine **)malloc(sizeof(SimpleFeatureLine *) * 2);

    pairs[0] = linesSrc;
    pairs[1] = linesDst;

    for (int i = 0; i < 2 * (*numLines); i++)
    {
        SimpleFeatureLine *which;
        which = pairs[i % 2];

        int idx = i / 2;

        fscanf(f, "%lf,%lf,%lf,%lf[^\n]", &(which[idx].startPoint.x),
               &(which[idx].startPoint.y), &(which[idx].endPoint.x),
               &(which[idx].endPoint.y));

        c = getc(f);
    }
    return pairs;
}

void warp(
    const SimplePoint *interPt,           //
    const SimpleFeatureLine *interLines,  //
    const SimpleFeatureLine *sourceLines, //
    const int sourceLinesSize,            //
    float p, float a, float b,            //
    SimplePoint *src                      //
)
{
    int i;
    float interLength, srcLength;
    float weight, weightSum, dist;
    float sum_x, sum_y;
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

        interLength = sqrt(interLength);

        v = (pd.x * pq.y - pd.y * pq.x) / interLength;

        pq.x = sourceLines[i].endPoint.x - sourceLines[i].startPoint.x;
        pq.y = sourceLines[i].endPoint.y - sourceLines[i].startPoint.y;

        srcLength = sqrt(pq.x * pq.x + pq.y * pq.y);
        X = sourceLines[i].startPoint.x + u * pq.x + v * pq.y / srcLength;
        Y = sourceLines[i].startPoint.y + u * pq.y - v * pq.x / srcLength;

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
            dist = fabsf(v);
        }

        weight = pow(pow(interLength, p) / (a + dist), b);
        sum_x += X * weight;
        sum_y += Y * weight;
        weightSum += weight;
    }

    src->x = sum_x / weightSum;
    src->y = sum_y / weightSum;
}

void bilinear(pixel *Im, float row, float col, pixel *pix)
{
    int cm, cn, fm, fn;
    double alpha, beta;

    cm = (int)ceil(row);
    fm = (int)floor(row);
    cn = (int)ceil(col);
    fn = (int)floor(col);
    alpha = ceil(row) - row;
    beta = ceil(col) - col;

    pix->r = (unsigned int)(alpha * beta * Im[fm * imgWidthOrig + fn].r               //
                            + (1 - alpha) * beta * Im[cm * imgWidthOrig + fn].r       //
                            + alpha * (1 - beta) * Im[fm * imgWidthOrig + cn].r       //
                            + (1 - alpha) * (1 - beta) * Im[cm * imgWidthOrig + cn].r //
    );
    pix->g = (unsigned int)(alpha * beta * Im[fm * imgWidthOrig + fn].g               //
                            + (1 - alpha) * beta * Im[cm * imgWidthOrig + fn].g       //
                            + alpha * (1 - beta) * Im[fm * imgWidthOrig + cn].g       //
                            + (1 - alpha) * (1 - beta) * Im[cm * imgWidthOrig + cn].g //
    );
    pix->b = (unsigned int)(alpha * beta * Im[fm * imgWidthOrig + fn].b               //
                            + (1 - alpha) * beta * Im[cm * imgWidthOrig + fn].b       //
                            + alpha * (1 - beta) * Im[fm * imgWidthOrig + cn].b       //
                            + (1 - alpha) * (1 - beta) * Im[cm * imgWidthOrig + cn].b //
    );
    pix->a = 255;
}

void ColorInterPolate(
    const SimplePoint *Src_P,  //
    const SimplePoint *Dest_P, //
    float t,                   //
    pixel *imgSrc,             //
    pixel *imgDest,            //
    pixel *rgb                 //
)
{
    pixel srcColor, destColor;

    bilinear(imgSrc, Src_P->y, Src_P->x, &srcColor);
    bilinear(imgDest, Dest_P->y, Dest_P->x, &destColor);

    rgb->b = srcColor.b * (1 - t) + destColor.b * t;
    rgb->g = srcColor.g * (1 - t) + destColor.g * t;
    rgb->r = srcColor.r * (1 - t) + destColor.r * t;
    rgb->a = 255;
}

/**
 * Parse the input arguments and set the global variables (this is only done in ROOT),
 * and the appropriate values are broadcasted later
 */
void parseArgs(int argc, char *argv[])
{
    /////////////////////////////////////
    // ARGUMENT PARSING - DO NOT TOUCH // oops i reformatted a little
    /////////////////////////////////////
    if (!(argc == 6 || argc == 9))
    {
        fprintf(stderr, "Invalid arguments. Usage:\n");
        printf("./morph sourceImage.png destinationImage.png outputpath steps linePath [p] [a] [b]\n");
        exit(1);
    }
    inputFileOrig = argv[1];
    inputFileDest = argv[2];
    outputFile = argv[3];
    steps = atoi(argv[4]);
    linePath = argv[5];

    imgRead(inputFileOrig, &hSrcImgMap, &imgWidthOrig, &imgHeightOrig);
    imgRead(inputFileDest, &hDstImgMap, &imgWidthDest, &imgHeightDest);

    if (argc == 9)
    {
        p = atof(argv[6]);
        a = atof(argv[7]);
        b = atof(argv[8]);
    }

    printf("\nUsing %d processes to perform %d steps\n", world_size, steps);
}

/**
 * Apply the kernel on this ranks slice
 */
void morphKernel(
    SimpleFeatureLine *hMorphLines, //
    pixel *hMorphMap,               //
    int numLines,                   //
    float t                         //
)
{
    for (int i = 0; i < mySliceHeight; i++)
    {
        for (int j = 0; j < imgWidthOrig; j++)
        {
            pixel interColor;
            SimplePoint dest;
            SimplePoint src;
            SimplePoint q = {.x = j, .y = i + myHeightOffset};

            // warping
            warp(&q, hMorphLines, hSrcLines, numLines, p, a, b, &src);
            warp(&q, hMorphLines, hDstLines, numLines, p, a, b, &dest);

            src.x = CLAMP(src.x, 0, imgWidthOrig - 1);
            src.y = CLAMP(src.y, 0, imgHeightOrig - 1);
            dest.x = CLAMP(dest.x, 0, imgWidthOrig - 1);
            dest.y = CLAMP(dest.y, 0, imgHeightOrig - 1);

            // color interpolation
            ColorInterPolate(&src, &dest, t, hSrcImgMap, hDstImgMap, &interColor);

            hMorphMap[i * imgWidthOrig + j].r = interColor.r;
            hMorphMap[i * imgWidthOrig + j].g = interColor.g;
            hMorphMap[i * imgWidthOrig + j].b = interColor.b;
            hMorphMap[i * imgWidthOrig + j].a = interColor.a;
        }
    }
}

/**
 * Perform morhping in all ranks then gather the slices and write this steps
 * image to file
 */
void doMorph(
    int numLines, //
    float t       //
)
{
    SimpleFeatureLine *hMorphLines = NULL;
    simpleLineInterpolate(&hMorphLines, numLines, t);

    ////////////////////////////////
    // PERFORM THE MORPHING STAGE //
    ////////////////////////////////

    morphKernel(hMorphLines, hMorphMap, numLines, t);

    ///////////////////////////////////
    // MERGE SLICES FOR OUTPUT IMAGE //
    ///////////////////////////////////

    // Gather all output slices in ROOT's hMorphMap
    MPI_Gather(
        hMorphMap,                                    //
        sizeof(pixel) * imgWidthOrig * mySliceHeight, //
        MPI_BYTE,                                     //
        hMorphMap,                                    //
        sizeof(pixel) * imgWidthOrig * mySliceHeight, //
        MPI_BYTE,                                     //
        ROOT,                                         //
        MPI_COMM_WORLD                                //
    );

    //////////////////////////////////
    // WRITE OUT THE FINISHED IMAGE //
    //////////////////////////////////

    if (world_rank == ROOT)
    {
        char rootFile[50] = {0};
        sprintf(rootFile, "%s%.5f.png", outputFile, t);
        imgWrite(rootFile, hMorphMap, imgWidthOrig, imgHeightOrig);
    }
    free(hMorphLines);
}

int main(int argc, char *argv[])
{
    //////////////////////////////////
    // MPI INITIALIZATION AND SETUP //
    //////////////////////////////////

    MPI_Init(&argc, &argv);

    // size and rank are saved globally instead of passing them as params everywhere.
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == ROOT)
    {
        // Parse argument and get vvalues for variables in rank 0, we later broadcast
        // the ones we need to the other ranks.
        parseArgs(argc, argv);
    }

    // Broadcasting arguments
    MPI_Bcast(&p, 1, MPI_FLOAT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&a, 1, MPI_FLOAT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&b, 1, MPI_FLOAT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&steps, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&imgWidthOrig, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&imgHeightOrig, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&imgWidthDest, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&imgHeightDest, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    /////////////////////////////////////////////////////
    // Load and allocate the feature lines and images  //
    /////////////////////////////////////////////////////
    int numLines;

    if (world_rank == ROOT)
    {
        SimpleFeatureLine **hLinePairs = loadLines(&numLines, linePath);
        hSrcLines = hLinePairs[0];
        hDstLines = hLinePairs[1];

        free(hLinePairs);
    }

    MPI_Bcast(&numLines, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    const size_t linePairSize = sizeof(SimpleFeatureLine) * numLines;
    const size_t imgSrcMapSize = sizeof(pixel) * imgHeightOrig * imgWidthOrig;
    const size_t imgDestMapSize = sizeof(pixel) * imgHeightDest * imgWidthDest;

    if (world_rank != ROOT)
    {
        // Allocate space for line pairs
        hSrcLines = malloc(linePairSize);
        hDstLines = malloc(linePairSize);
        // Allocate space for image maps
        hSrcImgMap = malloc(imgSrcMapSize);
        hDstImgMap = malloc(imgDestMapSize);
    }

    // Broadcast line pairs
    MPI_Bcast(hSrcLines, linePairSize, MPI_BYTE, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(hDstLines, linePairSize, MPI_BYTE, ROOT, MPI_COMM_WORLD);

    // Broadcast image maps
    MPI_Bcast(hSrcImgMap, imgSrcMapSize, MPI_BYTE, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(hDstImgMap, imgDestMapSize, MPI_BYTE, ROOT, MPI_COMM_WORLD);

    ///////////////////////////////////////
    // Prepae Slice and Image Morphing   //
    ///////////////////////////////////////

    mySliceHeight = imgHeightDest / world_size;
    myHeightOffset = world_rank * mySliceHeight;

    if (world_rank == ROOT)
    {
        // Root allocates space for entire output image (in order to havve space for it
        // when we gather the slices later before writing it to file).
        hMorphMap = malloc(sizeof(pixel) * imgWidthDest * imgHeightDest);
    }
    else
    {
        // All other ranks allocate enough space for the output of their slice
        hMorphMap = malloc(sizeof(pixel) * imgWidthDest * mySliceHeight);
    }
    if (hMorphMap == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    float stepSize = 1.0 / steps;

    //////////////////////////
    // Main Computation     //
    //////////////////////////

    double start = MPI_Wtime();
    for (int i = 0; i < steps + 1; i++)
    {
        if (world_rank == ROOT)
        {
            printProgress(i - 0.5, steps);
        }
        t = stepSize * i;
        doMorph(numLines, t);
        if (world_rank == ROOT)
        {
            printProgress(i + 0.5, steps);
        }
    }
    double end = MPI_Wtime();

    free(hSrcLines);
    free(hDstLines);
    free(hSrcImgMap);
    free(hDstImgMap);
    free(hMorphMap);

    if (world_rank == ROOT)
    {
        printf("%d Processes performed %d steps in %.2f seconds\n", world_size, steps, end - start);
    }

    MPI_Finalize();

    return 0;
}
