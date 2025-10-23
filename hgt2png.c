/*
 *********************************************************************************
 *** Project:            Heightmap Generator
 *** Creation Date:      2016-01-01
 *** Author:             Peter Ebel (peter.ebel@outlook.de)
 *** Objective:          Conversion of binary HGT files into PNG greyscale pictures
 *** Compile:            gcc hgt2png.c -o hgt2png -std=gnu99 -lpng -lm
 *** Dependencies:       libpng: sudo apt-get install libpng-dev
 *** Modification Log:  
 *** Version Date        Modified By   Modification Details
 *** ------------------------------------------------------------------------------
 *** 1.0.0   2016-01-01  Ebel          Initial creation of the script
 *** 1.0.1   2023-10-23  Ebel          Link switch changed to -lpng
 *** 1.1.0   2025-10-23  Ebel          Added Procedural Detail Generation
 **********************************************************************************
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
	
#include "/usr/include/libpng/png.h"

#define ChannelBlend_Multiply(A,B)   ((unsigned char)((A * B) / 255))

#define _MAX_PATH    255
#define _MAX_FILES   255
#define _MAX_HEIGHT 6000

// Neue Konstanten für Procedural Detail Generation
#define ENABLE_PROCEDURAL_DETAIL 1

// Parameter-Struktur für Kommandozeilen-Optionen
typedef struct {
  int scaleFactor;
  float detailIntensity;
  int noiseSeed;
  int enableDetail;
  int verbose;
  int showHelp;
  int showVersion;
} ProgramOptions;

// Standard-Werte
#define DEFAULT_SCALE_FACTOR 3
#define DEFAULT_DETAIL_INTENSITY 15.0f
#define DEFAULT_NOISE_SEED 12345

typedef int errno_t;

typedef struct tag_RGB {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} RGB, *pRGB;

typedef struct tag_FileInfoHGT {
  char szFilename[_MAX_PATH];
  short int iWidth;
  short int iHeight;
  short int iMinElevation;
  short int iMaxElevation;
  unsigned long ulFilesize;
} FileInfo, *pFileInfo;

FileInfo *fi;
RGB *PixelData;

void WritePNG(const char *, short int, short int);

// Parameter-Management Funktionen
void initDefaultOptions(ProgramOptions *opts);
void showHelp(const char *programName);
void showVersion(void);
int parseArguments(int argc, char *argv[], ProgramOptions *opts, char **inputFile);

// Neue Funktionen für Procedural Detail Generation
float SimpleNoise(int x, int y, int seed);
float FractalNoise(float x, float y, int octaves, float persistence, float scale, int seed);
short int BilinearInterpolate(short int* data, int width, int height, float x, float y);
float CalculateLocalSlope(short int* data, int width, int height, float x, float y);
float GetHeightTypeFactor(short int height);
short int* AddProceduralDetail(short int* originalData, int originalWidth, int originalHeight, 
                               int scaleFactor, float detailIntensity, int seed);

const int HGT_TYPE_30 = 1201;
const int HGT_TYPE_90 = 3601;
const int HGT_TYPE_UNKNOWN = -1;
const unsigned long HGT_TYPE_30_SIZE = 1201 * 1201 * sizeof(short int);
const unsigned long HGT_TYPE_90_SIZE = 3601 * 3601 * sizeof(short int);

int iHGTType;

char OutputHeightmapFile[_MAX_PATH];

short int iOverallMinElevation;
short int iOverallMaxElevation;
short int iCurrentMinElevation;
short int iCurrentMaxElevation;

int main(int argc, char *argv[])
{
  ProgramOptions opts;
  char *inputFile = NULL;

  FILE *InFile;
  FILE *FileList;
  char *sCurrentFilename[255];
  char sBuffer[255];
  int f = 0;

  short int *iElevationData;
  int iNumIntRead;
  int iNumFilesToConvert = 0;

  // Initialisiere Standard-Optionen
  initDefaultOptions(&opts);

  // Parse Kommandozeilen-Argumente
  int parseResult = parseArguments(argc, argv, &opts, &inputFile);
  if (parseResult != 0) {
    return parseResult;
  }

  // Zeige Version falls angefordert
  if (opts.showVersion) {
    showVersion();
    return 0;
  }

  // Zeige Hilfe falls angefordert oder keine Input-Datei
  if (opts.showHelp || inputFile == NULL) {
    showHelp(argv[0]);
    return opts.showHelp ? 0 : 1;
  }

  if (opts.verbose) {
    fprintf(stderr, "\nhgt2png Converter v1.1.0 (C) 2025 - with Procedural Detail Generation\n");
    fprintf(stderr, "Scale Factor: %d, Detail Intensity: %.1f, Seed: %d\n", 
            opts.scaleFactor, opts.detailIntensity, opts.noiseSeed);
  }

  if ((strstr(inputFile, "HGT") != NULL) || strstr(inputFile, "hgt") != NULL)
  {
    if (opts.verbose) fprintf(stderr, "INFO: Single-File Mode\n");
    sCurrentFilename[0] = (char *) malloc(strlen(inputFile) + 1);
    if (sCurrentFilename[0] == NULL) {
      fprintf(stderr, "Error: Can't allocate memory for filename\n");
      return 1;
    }
    strcpy(sCurrentFilename[0], inputFile);
    iNumFilesToConvert = 1;
  }
  else
  {  
    if (opts.verbose) fprintf(stderr, "INFO: Filelist Mode\n");
    if ((FileList = fopen(inputFile, "rb")) == NULL) {
      fprintf(stderr, "Error: Can't open file list %s\n", inputFile);
      return 1;
    }

    while (!feof(FileList) && f < _MAX_FILES - 1)
    {
      if (fgets(sBuffer, sizeof(sBuffer), FileList) != NULL)
      {
        // Entferne Newline und prüfe auf leere Zeilen
        size_t len = strlen(sBuffer);
        if (len > 0 && sBuffer[len - 1] == '\n') {
          sBuffer[len - 1] = '\0';
          len--;
        }
        
        // Überspringe leere Zeilen
        if (len > 0) {
          sCurrentFilename[f] = (char *) malloc(len + 1);
          if (sCurrentFilename[f] == NULL) {
            fprintf(stderr, "Error: Can't allocate memory for filename %d\n", f);
            // Cleanup bereits allokierter Filenames
            for (int cleanup = 0; cleanup < f; cleanup++) {
              free(sCurrentFilename[cleanup]);
            }
            fclose(FileList);
            return 1;
          }
          strcpy(sCurrentFilename[f], sBuffer);
          f++;
        }
      }
    }
    
    if (f >= _MAX_FILES - 1) {
      fprintf(stderr, "WARNING: Maximum number of files (%d) reached. Some files may be skipped.\n", _MAX_FILES - 1);
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

    if ((iNumIntRead = fread(iElevationData, 1, fi[i].ulFilesize, InFile)) != (int)fi[i].ulFilesize)
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

    if ((iNumIntRead = fread(iElevationData, 1, fi[k].ulFilesize, InFile)) != (int)fi[k].ulFilesize)
    {
      fprintf(stderr, "Filename: %s\n", fi[k].szFilename);
      fprintf(stderr, "Filesize: %d\n", (unsigned int) fi[k].ulFilesize);
      fprintf(stderr, "Error: Can't load elevation data 2\n");
      fclose(InFile);
      return 1;
    } 

    fclose(InFile);

    // PROCEDURAL DETAIL GENERATION - NEU EINGEFÜGT
    if (opts.enableDetail) {
      if (opts.verbose) fprintf(stderr, "INFO: Adding procedural detail to %s...\n", fi[k].szFilename);
      
      // Byte-Swapping vor der Detail-Generation durchführen
      for (unsigned long pre = 0; pre < (fi[k].ulFilesize) / 2; pre++)
      {
        if (iHGTType == HGT_TYPE_30 || iHGTType == HGT_TYPE_90)
        {
          iElevationData[pre] = (short)(((iElevationData[pre] & 0xff) << 8) | ((iElevationData[pre] & 0xff00) >> 8));
        }
        if (iElevationData[pre] < 0) iElevationData[pre] = 0;
        if (iElevationData[pre] > _MAX_HEIGHT) iElevationData[pre] = _MAX_HEIGHT;
      }
      
      // Procedural Detail hinzufügen
      short int* detailedData = AddProceduralDetail(iElevationData, fi[k].iWidth, fi[k].iHeight, 
                                                    opts.scaleFactor, opts.detailIntensity, opts.noiseSeed);
      
      if (detailedData) {
        free(iElevationData);
        iElevationData = detailedData;
        fi[k].iWidth *= opts.scaleFactor;
        fi[k].iHeight *= opts.scaleFactor;
        fi[k].ulFilesize = fi[k].iWidth * fi[k].iHeight * sizeof(short int);
        
        if (opts.verbose) fprintf(stderr, "INFO: Enhanced resolution: %dx%d pixels\n", fi[k].iWidth, fi[k].iHeight);
      } else {
        fprintf(stderr, "WARNING: Could not add procedural detail, using original data\n");
      }
    }

    if ((PixelData = (RGB *) malloc(fi[k].ulFilesize * sizeof(struct tag_RGB))) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate pixel data block.\n");
      if (iElevationData != NULL) {
        free(iElevationData);
      }
      // Cleanup bereits verarbeiteter Dateien
      for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
        if (sCurrentFilename[cleanup] != NULL) {
          free(sCurrentFilename[cleanup]);
        }
      }
      free(fi);
      return 1;
    }
     
    for (unsigned long m = 0; m < (fi[k].ulFilesize) / 2; m++)
    {
      if (!opts.enableDetail) {
        // Byte-Swapping nur wenn keine Procedural Details verwendet werden
        if (iHGTType == HGT_TYPE_30 || iHGTType == HGT_TYPE_90)
        {
          iElevationData[m] = (short)(((iElevationData[m] & 0xff) << 8) | ((iElevationData[m] & 0xff00) >> 8));
        }
      }

      if (iOverallMaxElevation != 0)
      {
        PixelData[m].r = PixelData[m].g = PixelData[m].b = iElevationData[m] * 255 / iOverallMaxElevation;
      }
    }

    strcpy(OutputHeightmapFile, fi[k].szFilename);
    OutputHeightmapFile[strlen(OutputHeightmapFile) - 4] = '\0';
    strcat(OutputHeightmapFile, ".png");
    
    WritePNG(OutputHeightmapFile, fi[k].iWidth, fi[k].iHeight);

  //Free allocated memory  
    if (iElevationData != NULL)
    {
      free(iElevationData);
    }

    if (PixelData != NULL)
    {
      free(PixelData);
    }

  }

  // Cleanup allocated memory
  free(fi);
  
  // Free sCurrentFilename arrays (both Single-File and Filelist mode)
  for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
    if (sCurrentFilename[cleanup] != NULL) {
      free(sCurrentFilename[cleanup]);
    }
  }
  
  fprintf(stderr, "Info: Done\n");

}

void WritePNG(const char *szFilename, short int _iWidth, short int _iHeight)
{
  png_image image; 

  memset(&image, 0, sizeof image);

  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_RGB;
  image.width = _iWidth;
  image.height = _iHeight;

  fprintf(stderr, "Info: Writing %s\n", szFilename);
  if (!png_image_write_to_file(&image, szFilename, 0/*convert_to_8bit*/, (png_bytep) PixelData, 0/*row_stride*/, NULL/*colormap*/))
  {
    fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
  }

  png_image_free(&image);

}

// NEUE FUNKTIONEN FÜR PROCEDURAL DETAIL GENERATION

// Simple Noise-Funktion (Perlin-ähnlich)
float SimpleNoise(int x, int y, int seed) {
    int n = x + y * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

// Fraktales Noise für verschiedene Detailgrade
float FractalNoise(float x, float y, int octaves, float persistence, float scale, int seed) {
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        total += SimpleNoise((int)(x * frequency), (int)(y * frequency), seed + i) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    
    return total / maxValue;
}

// Bilineare Interpolation
short int BilinearInterpolate(short int* data, int width, int height, float x, float y) {
    int x1 = (int)x;
    int y1 = (int)y;
    int x2 = x1 + 1;
    int y2 = y1 + 1;
    
    // Grenzen prüfen
    if (x2 >= width) x2 = width - 1;
    if (y2 >= height) y2 = height - 1;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    
    float fx = x - x1;
    float fy = y - y1;
    
    short int p1 = data[y1 * width + x1];  // Top-left
    short int p2 = data[y1 * width + x2];  // Top-right
    short int p3 = data[y2 * width + x1];  // Bottom-left
    short int p4 = data[y2 * width + x2];  // Bottom-right
    
    // Interpolation in X-Richtung
    float i1 = p1 * (1 - fx) + p2 * fx;
    float i2 = p3 * (1 - fx) + p4 * fx;
    
    // Interpolation in Y-Richtung
    return (short int)(i1 * (1 - fy) + i2 * fy);
}

// Lokale Steigung berechnen
float CalculateLocalSlope(short int* data, int width, int height, float x, float y) {
    int ix = (int)x;
    int iy = (int)y;
    
    if (ix <= 0 || ix >= width-1 || iy <= 0 || iy >= height-1) return 0.0f;
    
    short int left = data[iy * width + (ix-1)];
    short int right = data[iy * width + (ix+1)];
    short int top = data[(iy-1) * width + ix];
    short int bottom = data[(iy+1) * width + ix];
    
    float dx = (right - left) / 60.0f; // 60m = 2 * 30m SRTM-1 Auflösung
    float dy = (bottom - top) / 60.0f;
    
    float slope = sqrt(dx*dx + dy*dy) / 100.0f; // Normalisiert auf 0-1
    if (slope > 1.0f) slope = 1.0f;
    return slope;
}

// Terrain-typ-spezifische Faktoren
float GetHeightTypeFactor(short int height) {
    if (height < 100) return 0.5f;      // Küstenebenen - wenig Detail
    else if (height < 500) return 0.7f;  // Hügelland - mittleres Detail
    else if (height < 1500) return 1.0f; // Berge - volles Detail
    else if (height < 3000) return 0.8f; // Hochgebirge - weniger Detail (Fels)
    else return 0.3f;                    // Extreme Höhen - minimales Detail
}

// Hauptfunktion für Procedural Detail Generation
short int* AddProceduralDetail(short int* originalData, int originalWidth, int originalHeight, 
                               int scaleFactor, float detailIntensity, int seed) {
    
    int newWidth = originalWidth * scaleFactor;
    int newHeight = originalHeight * scaleFactor;
    
    short int* detailedData = malloc(newWidth * newHeight * sizeof(short int));
    if (!detailedData) {
        fprintf(stderr, "ERROR: Cannot allocate memory for detailed heightmap\n");
        return NULL;
    }
    
    fprintf(stderr, "INFO: Generating %dx%d detailed heightmap (intensity: %.1f)\n", 
            newWidth, newHeight, detailIntensity);
    
    for (int y = 0; y < newHeight; y++) {
        for (int x = 0; x < newWidth; x++) {
            // Basis-Höhe durch bilineare Interpolation
            float srcX = (float)x / scaleFactor;
            float srcY = (float)y / scaleFactor;
            
            if (srcX >= originalWidth - 1) srcX = originalWidth - 1.001f;
            if (srcY >= originalHeight - 1) srcY = originalHeight - 1.001f;
            
            short int baseHeight = BilinearInterpolate(originalData, originalWidth, originalHeight, srcX, srcY);
            
            // Procedural Detail hinzufügen
            // Verschiedene Noise-Oktaven für unterschiedliche Detailgrößen
            float detailNoise1 = FractalNoise(x * 0.005f, y * 0.005f, 3, 0.5f, 1.0f, seed);      // Große Features
            float detailNoise2 = FractalNoise(x * 0.02f, y * 0.02f, 4, 0.6f, 1.0f, seed + 100);  // Mittlere Features  
            float detailNoise3 = FractalNoise(x * 0.08f, y * 0.08f, 2, 0.4f, 1.0f, seed + 200);  // Feine Details
            
            float combinedNoise = detailNoise1 * 0.5f + detailNoise2 * 0.3f + detailNoise3 * 0.2f;
            
            // Höhenabhängige Variation (flache Bereiche = weniger Detail, steile = mehr Detail)
            float localSlope = CalculateLocalSlope(originalData, originalWidth, originalHeight, srcX, srcY);
            float slopeMultiplier = 0.3f + (localSlope * 0.7f); // 0.3 bis 1.0
            
            // Detail basierend auf Terrain-Typ anpassen
            float heightFactor = GetHeightTypeFactor(baseHeight);
            
            // Finales Detail berechnen
            float detailVariation = combinedNoise * detailIntensity * slopeMultiplier * heightFactor;
            
            short int finalHeight = baseHeight + (short int)(detailVariation);
            
            // Sicherstellen, dass Werte im gültigen Bereich bleiben
            if (finalHeight < 0) finalHeight = 0;
            if (finalHeight > _MAX_HEIGHT) finalHeight = _MAX_HEIGHT;
            
            detailedData[y * newWidth + x] = finalHeight;
        }
        
        // Fortschrittsanzeige
        if (y % (newHeight / 10) == 0) {
            fprintf(stderr, "INFO: Progress: %d%%\n", (y * 100) / newHeight);
        }
    }
    
    return detailedData;
}

// PARAMETER-MANAGEMENT FUNKTIONEN

// Initialisiere Standard-Optionen
void initDefaultOptions(ProgramOptions *opts) {
    opts->scaleFactor = DEFAULT_SCALE_FACTOR;
    opts->detailIntensity = DEFAULT_DETAIL_INTENSITY;
    opts->noiseSeed = DEFAULT_NOISE_SEED;
    opts->enableDetail = ENABLE_PROCEDURAL_DETAIL;
    opts->verbose = 1;  // Standard: Verbose Output
    opts->showHelp = 0;
    opts->showVersion = 0;
}

// Zeige Hilfe-Text
void showHelp(const char *programName) {
    printf("hgt2png v1.1.0 - HGT to PNG Heightmap Converter with Procedural Detail Generation\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] <input.hgt|filelist.txt>\n\n", programName);
    
    printf("OPTIONS:\n");
    printf("  -s, --scale <factor>     Scale factor for resolution enhancement (default: %d)\n", DEFAULT_SCALE_FACTOR);
    printf("                           1=original, 2=double, 3=triple resolution\n");
    printf("  -i, --intensity <value>  Detail intensity in meters (default: %.1f)\n", DEFAULT_DETAIL_INTENSITY);
    printf("                           Higher values = more pronounced details\n");
    printf("  -r, --seed <value>       Random seed for procedural generation (default: %d)\n", DEFAULT_NOISE_SEED);
    printf("  -d, --disable-detail     Disable procedural detail generation\n");
    printf("  -q, --quiet              Suppress verbose output\n");
    printf("  -h, --help               Show this help message\n");
    printf("  -v, --version            Show version information\n\n");
    
    printf("INPUT:\n");
    printf("  Single HGT file:         %s terrain.hgt\n", programName);
    printf("  Multiple files (list):   %s filelist.txt\n\n", programName);
    
    printf("EXAMPLES:\n");
    printf("  %s N48E011.hgt                          # Standard processing\n", programName);
    printf("  %s -s 2 -i 25.0 N48E011.hgt             # 2x scale, high detail\n", programName);
    printf("  %s -d -q terrain_files.txt              # No detail, quiet mode\n", programName);
    printf("  %s --scale 4 --intensity 10 --seed 999  # Custom parameters\n\n", programName);
    
    printf("OUTPUT:\n");
    printf("  Creates PNG files with same basename as input HGT files.\n");
    printf("  Example: N48E011.hgt → N48E011.png\n\n");
}

// Zeige Version
void showVersion(void) {
    printf("hgt2png v1.1.0\n");
    printf("HGT to PNG Heightmap Converter with Procedural Detail Generation\n");
    printf("(C) 2025 Peter Ebel\n");
}

// Parse Kommandozeilen-Argumente
int parseArguments(int argc, char *argv[], ProgramOptions *opts, char **inputFile) {
    int c;
    static struct option long_options[] = {
        {"scale-factor",     required_argument, 0, 's'},
        {"detail-intensity", required_argument, 0, 'i'},
        {"noise-seed",       required_argument, 0, 'r'},
        {"disable-detail",   no_argument,       0, 'd'},
        {"quiet",            no_argument,       0, 'q'},
        {"help",             no_argument,       0, 'h'},
        {"version",          no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "s:i:r:dqhv", long_options, NULL)) != -1) {
        switch (c) {
            case 's':
                opts->scaleFactor = atoi(optarg);
                if (opts->scaleFactor < 1 || opts->scaleFactor > 10) {
                    fprintf(stderr, "Error: Scale factor must be between 1 and 10\n");
                    return 1;
                }
                break;
                
            case 'i':
                opts->detailIntensity = atof(optarg);
                if (opts->detailIntensity < 0.0f || opts->detailIntensity > 100.0f) {
                    fprintf(stderr, "Error: Detail intensity must be between 0.0 and 100.0\n");
                    return 1;
                }
                break;
                
            case 'r':
                opts->noiseSeed = atoi(optarg);
                break;
                
            case 'd':
                opts->enableDetail = 0;
                break;
                
            case 'q':
                opts->verbose = 0;
                break;
                
            case 'h':
                opts->showHelp = 1;
                return 0;
                
            case 'v':
                opts->showVersion = 1;
                return 0;
                
            case '?':
                fprintf(stderr, "Use '%s --help' for usage information.\n", argv[0]);
                return 1;
                
            default:
                return 1;
        }
    }

    // Input-Datei aus den verbleibenden Argumenten
    if (optind < argc) {
        *inputFile = argv[optind];
    }

    return 0;
}
