/******************************************************************************************
The "morph" program is to do the morph
*******************************************************************************************/

#ifndef MORPH_H
#define MORPH_H

#include <math.h>

// Default Width
const int WIDTH = 600;
// Default Height
const int HEIGHT = 600;

// Start image width
int imgWidthOrig = 0;
// Start image height
int imgHeightOrig = 0;
// End image width
int imgWidthDest = 0;
// End image height
int imgHeightDest = 0;

//the pixel
typedef struct pix{
  unsigned char r, g, b, a;
} pixel;

typedef struct SimplePoint_struct {
        double x, y;
} SimplePoint;

typedef struct SimpleFeatureLine_struct {
      SimplePoint startPoint;
      SimplePoint endPoint;
} SimpleFeatureLine;

// Start Image
pixel * hSrcImgMap;
// End Image
pixel * hDstImgMap;
// Output Pixels (the morphed image) 
pixel * hMorphMap;

// Start Input file
const char *inputFileOrig;
// End input file
const char *inputFileDest;
// Output file
const char *outputFile;

// Temp File
const char *tempFile;
// Line Path
const char *linePath;

//the parameter of the weight
const char* pStr, *aStr, *bStr, *tStr;
float p = 0;
float a = 1;
float b = 2;
float t = 0.5;


/********************************************************************************
This function would allocate the memory space for the pixmap
********************************************************************************/

void allocPixmap(int w, int h, pixel ** map);

/********************************************************************************
This function would read from a image file of various types and store the RGBA
info into the "pixmap".
********************************************************************************/

pixel ** setup2DPixelMap(unsigned char *pixelMap, int w, int h, int channels);

#endif
