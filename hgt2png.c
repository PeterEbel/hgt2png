/*
 *********************************************************************************
 *** Project:            Heightmap Generator
 *** Creation Date:      2016-01-01
 *** Author:             Peter Ebel (peter.ebel@outlook.de)
 *** Objective:          Conversion of binary HGT files into PNG greyscale pictures
 *** Compile:            gcc hgt2png.c -o hgt2png -std=gnu99 /usr/lib/libpng.so
 *** Modification Log:  
 *** Version Date        Modified By   Modification Details
 *** ------------------------------------------------------------------------------
 *** 1.0.0   2016-01-01  Ebel          Initial creation of the script
 **********************************************************************************
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
	
#include "/usr/include/libpng/png.h"

#define ChannelBlend_Multiply(A,B)   ((unsigned char)((A * B) / 255))

#define _MAX_PATH    255
#define _MAX_FILES   255
#define _MAX_HEIGHT 6000

typedef int errno_t;

typedef struct tag_RGB {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} RGB, *pRGB;

typedef struct tag_Gradient {
  int iMinElevation;
  int iMaxElevation;
  RGB StartColor;
  RGB EndColor;
} Gradient, *pGradient;

typedef struct tag_Zoning {
  int iNumberOfGradients;
  Gradient gradient;
} Zoning, *pZoning;

typedef struct tag_FileInfoHGT {
  char szFilename[_MAX_PATH];
  char szZoningFilename[_MAX_PATH];
  short int iWidth;
  short int iHeight;
  short int iMinElevation;
  short int iMaxElevation;
  unsigned long ulFilesize;
} FileInfo, *pFileInfo;

FileInfo *fi;
RGB *PixelData;
RGB *ZoningData;

void WritePNG(const char *, short int, short int);
void WriteZoningPNG(const char *, short int, short int);
void CreateZoning(Zoning *, int);

const int HGT_TYPE_30 = 1201;
const int HGT_TYPE_90 = 3601;
const int HGT_TYPE_UNKNOWN = -1;
const int HGT_TYPE_30_SIZE = 1201 * 1201 * sizeof(short int);
const int HGT_TYPE_90_SIZE = 3601 * 3601 * sizeof(short int);

int iHGTType;

char OutputHeightmapFile[_MAX_PATH];
char OutputZoningFile[_MAX_PATH];

short int iOverallMinElevation;
short int iOverallMaxElevation;
short int iCurrentMinElevation;
short int iCurrentMaxElevation;

int main(int argc, const char *argv[])
{

  FILE *InFile;
  FILE *FileList;
  char *sCurrentFilename[255];
  char sBuffer[255];
  int f = 0;

  short int *iElevationData;
  int iNumIntRead;
  int iNumFilesToConvert = 0;
  int iNumberOfZonings = 3;

  Zoning *z;

  fprintf(stderr, "\nhgt2png Converter v1.0 (C) 2016\n");

  if (argc == 1)  
  {
    fprintf(stderr, "\nusage: hgt2png <filename>|<filelist>\n\n");
    return 1;
  }

  if ((strstr(argv[1], "HGT") != NULL) || strstr(argv[1], "hgt") != NULL)
  {
    fprintf(stderr, "INFO: Single-File Mode\n");
    sCurrentFilename[0] = (char *) malloc(strlen(argv[1]));
    strcpy(sCurrentFilename[0], argv[1]);
    iNumFilesToConvert = 1;
  }
  else
  {  
    fprintf(stderr, "INFO: Filelist Mode\n");
    if ((FileList = fopen(argv[1], "rb")) == NULL) {
      fprintf(stderr, "Error: Can't open file list %s\n", argv[1]);
      return 1;
    }

    while (!feof(FileList))
    {
      fgets(sBuffer,sizeof(sBuffer), FileList);
      sCurrentFilename[f] = (char *) malloc(strlen(sBuffer));
      strcpy(sCurrentFilename[f], sBuffer);
      sCurrentFilename[f][strlen(sCurrentFilename[f]) - 1] = '\0';
      f++;	
    }

    fclose(FileList);
    iNumFilesToConvert = f;
  }

  fprintf(stderr, "INFO: Number of files to convert: %d\n", iNumFilesToConvert);

  if ((fi = (FileInfo *) malloc(iNumFilesToConvert * sizeof(struct tag_FileInfoHGT))) == NULL)
  {
    fprintf(stderr, "Error: Can't allocate FileInfo array\n");
    return 1;
  }

  if ((z = (Zoning *) malloc(iNumberOfZonings * sizeof(struct tag_Zoning))) == NULL)
  {
    fprintf(stderr, "Error: Can't allocate Zoning array\n");
    return 1;
  }

  CreateZoning(z, iNumberOfZonings);

  struct stat buf;

  iOverallMinElevation = 9999;
  iOverallMaxElevation = 0;
  iCurrentMinElevation = 9999;
  iCurrentMaxElevation = 0;

  for (int i = 0; i < iNumFilesToConvert; i++)
  {
     
    iNumIntRead = 0;
    char sTmp[5];

    memset(&fi[i],0,sizeof(FileInfo));
 	strcpy(fi[i].szFilename, sCurrentFilename[i]);
    fprintf(stderr, "INFO: Pre-Processing: %s ", fi[i].szFilename);
    fi[i].iMinElevation = 0; 
    fi[i].iMaxElevation = 0;

    if ((InFile = fopen(fi[i].szFilename, "rb")) == NULL) {
      fprintf(stderr, "Error: Can't open input file %s\n", fi[i].szFilename);
      return 1;
    }

    fstat(fileno(InFile), &buf);
    fi[i].ulFilesize = buf.st_size;

    if (fi[i].ulFilesize == HGT_TYPE_30_SIZE)
    {
      iHGTType = HGT_TYPE_30;
      fi[i].iWidth = 1201;
      fi[i].iHeight = 1201;
    } else if (fi[i].ulFilesize == HGT_TYPE_90_SIZE)
    {
      iHGTType = HGT_TYPE_90;
      fi[i].iWidth = 3601;
      fi[i].iHeight = 3601;
    }
    else
    {
      memcpy(sTmp, &sCurrentFilename[i][5], 4);
      sTmp[4] = '\0';
      fi[i].iWidth = atoi(sTmp);
      memcpy(sTmp, &sCurrentFilename[i][10], 4);
      sTmp[4] = '\0';
      fi[i].iHeight = atoi(sTmp);
      iHGTType = fi[i].iWidth * fi[i].iHeight;
      free(sCurrentFilename[i]);
    }

    if (iHGTType == HGT_TYPE_UNKNOWN) {
      fprintf(stderr, "Error: %s has an unknown HGT type\n", fi[i].szFilename);
      fclose(InFile);
      return 1;
    }

    if ((iElevationData = (short *) malloc(fi[i].ulFilesize)) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate memory to load %s\n", fi[i].szFilename);
      fclose(InFile);
      return 1;
    }

    if ((iNumIntRead = fread(iElevationData, 1, fi[i].ulFilesize, InFile)) != fi[i].ulFilesize)
    {
      fprintf(stderr, "Error: Can't load elevation data\n");
      fclose(InFile);
      return 1;
    } 

    iCurrentMaxElevation = 0;
    iCurrentMinElevation = 9999;
    for (unsigned long j = 0; j < fi[i].ulFilesize / 2; j++)
    {
      if (iHGTType == HGT_TYPE_30 || iHGTType == HGT_TYPE_90)
      {
        iElevationData[j] = (short)(((iElevationData[j] & 0xff) << 8) | ((iElevationData[j] & 0xff00) >> 8));
      }
      if (iElevationData[j] < 0) iElevationData[j] = 0;
      if (iElevationData[j] > _MAX_HEIGHT) iElevationData[j] = _MAX_HEIGHT; //TO-DO
      if (iElevationData[j] < iCurrentMinElevation) iCurrentMinElevation = iElevationData[j];
      if (iElevationData[j] > iCurrentMaxElevation) iCurrentMaxElevation = iElevationData[j];
    }

    if (iCurrentMinElevation < iOverallMinElevation) iOverallMinElevation = iCurrentMinElevation;
    if (iCurrentMaxElevation > iOverallMaxElevation) iOverallMaxElevation = iCurrentMaxElevation;
    fprintf(stderr, "- MIN=%4d MAX=%4d\n", iOverallMinElevation, iOverallMaxElevation);
    
    free(iElevationData);
    fclose(InFile);
  }

  for (int k = 0; k < iNumFilesToConvert; k++)
  {
    if ((InFile = fopen(fi[k].szFilename, "rb")) == NULL) {
      fprintf(stderr, "Error: Can't open input file %s\n", fi[k].szFilename);
      return 1;
    }

    if ((iElevationData = (short *) malloc(fi[k].ulFilesize)) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate elevation data block %s\n", fi[k].szFilename);
      fclose(InFile);
      return 1;
    }

    if ((iNumIntRead = fread(iElevationData, 1, fi[k].ulFilesize, InFile)) != fi[k].ulFilesize)
    {
      fprintf(stderr, "Filename: %s\n", fi[k].szFilename);
      fprintf(stderr, "Filesize: %d\n", (unsigned int) fi[k].ulFilesize);
      fprintf(stderr, "Error: Can't load elevation data 2\n");
      fclose(InFile);
      return 1;
    } 

    fclose(InFile);

    if ((ZoningData = (RGB *) malloc(fi[k].ulFilesize * sizeof(struct tag_RGB))) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate zoning data block.\n");
      return 1;
    }

    if ((PixelData = (RGB *) malloc(fi[k].ulFilesize * sizeof(struct tag_RGB))) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate pixel data block.\n");
      return 1;
    }
     
    for (int m = 0; m < (fi[k].ulFilesize) / 2; m++)
    {
      if (iHGTType == HGT_TYPE_30 || iHGTType == HGT_TYPE_90)
      {
        iElevationData[m] = (short)(((iElevationData[m] & 0xff) << 8) | ((iElevationData[m] & 0xff00) >> 8));
      }

      if (iOverallMaxElevation != 0)
      {
        PixelData[m].r = PixelData[m].g = PixelData[m].b = iElevationData[m] * 255 / iOverallMaxElevation;
      }
       
      //Zoning
     
      ZoningData[m].r = PixelData[m].r;
      ZoningData[m].g = PixelData[m].g;
      ZoningData[m].b = PixelData[m].b;
     
      for (int g = 0; g < z->iNumberOfGradients; g++)
      {
        if ((iElevationData[m] >= z[g].gradient.iMinElevation) && (iElevationData[m] <= z[g].gradient.iMaxElevation))
        {
          switch (g)
          {
            case 0:
              ZoningData[m].r = z[0].gradient.StartColor.r;
              ZoningData[m].g = z[0].gradient.StartColor.g;
              ZoningData[m].b = z[0].gradient.StartColor.b;
              break;
            case 1:
              ZoningData[m].r = z[1].gradient.StartColor.r;
              ZoningData[m].g = z[1].gradient.StartColor.g;
              ZoningData[m].b = z[1].gradient.StartColor.b;
              break;
            case 2:
              ZoningData[m].r = z[2].gradient.StartColor.r;
              ZoningData[m].g = z[2].gradient.StartColor.g;
              ZoningData[m].b = z[2].gradient.StartColor.b;
              break;
            default:
              break;
          }
        }
      }
    }

    strcpy(OutputHeightmapFile, fi[k].szFilename);
    OutputHeightmapFile[strlen(OutputHeightmapFile) - 4] = '\0';
    strcat(OutputHeightmapFile, ".PNG");
    
    WritePNG(OutputHeightmapFile, fi[k].iWidth, fi[k].iHeight);

    strcpy(OutputZoningFile, fi[k].szFilename);
    OutputZoningFile[strlen(OutputZoningFile) - 4] = '\0';
    strcat(OutputZoningFile, "_ZON.PNG");
      
//  WriteZoningPNG(OutputZoningFile, fi[k].iWidth, fi[k].iHeight);

  //Free allocated memory  
    if (iElevationData != NULL)
    {
      free(iElevationData);
    }

    if (ZoningData != NULL)
    {
      free(ZoningData);
    }

    if (PixelData != NULL)
    {
      free(PixelData);
    }

  }

  free(fi);
  fprintf(stderr, "Info: Done\n");

}

void WritePNG(const char *szFilename, short int _iWidth, short int _iHeight)
{

  int result;
  png_image image; 

  memset(&image, 0, sizeof image);

  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_RGB;
  image.width = _iWidth;
  image.height = _iHeight;

  fprintf(stderr, "Info: Writing %s\n", szFilename);
//fprintf(stderr, "Info: Resolution %d x %d\n", image.width, image.height);
  if (png_image_write_to_file(&image, szFilename, 0/*convert_to_8bit*/, (png_bytep) PixelData, 0/*row_stride*/, NULL/*colormap*/))
  {
    result = 0;
  }
  else
    fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);

  png_image_free(&image);

}

void WriteZoningPNG(const char *szFilename, short int _iWidth, short int _iHeight)
{

  int result;
  png_image image; 

  memset(&image, 0, sizeof image);

  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_RGB;
  image.width = _iWidth;
  image.height = _iHeight;

  fprintf(stderr, "Info: Writing %s\n", szFilename);
  
  if (png_image_write_to_file(&image, szFilename, 0/*convert_to_8bit*/, (png_bytep) ZoningData, 0/*row_stride*/, NULL/*colormap*/))
  {
    result = 0;
  }
  else
    fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);

//free(ZoningData);
  png_image_free(&image);

}

void CreateZoning(Zoning *z, int _iNumberOfZonings)
{
  z[0].iNumberOfGradients = _iNumberOfZonings;

  z[0].gradient.iMinElevation = 0;
  z[0].gradient.iMaxElevation = 1000;
  z[0].gradient.StartColor.r = (char) 255;
  z[0].gradient.StartColor.g = 0;
  z[0].gradient.StartColor.b = 0;
  z[0].gradient.EndColor.r = 0;
  z[0].gradient.EndColor.g = (char) 255;
  z[0].gradient.EndColor.b = 0;
 
  z[1].gradient.iMinElevation = 1001;
  z[1].gradient.iMaxElevation = 2000;
  z[1].gradient.StartColor.r = 0;
  z[1].gradient.StartColor.g = (char) 255;
  z[1].gradient.StartColor.b = 0;
  z[1].gradient.EndColor.r = 0;
  z[1].gradient.EndColor.g = 0;
  z[1].gradient.EndColor.b = (char) 255;

  z[2].gradient.iMinElevation = 2001;
  z[2].gradient.iMaxElevation = 5000;
  z[2].gradient.StartColor.r = 0;
  z[2].gradient.StartColor.g = 0;
  z[2].gradient.StartColor.b = (char) 255;
  z[2].gradient.EndColor.r = 0;
  z[2].gradient.EndColor.g = 0;
  z[2].gradient.EndColor.b = 0;
}
