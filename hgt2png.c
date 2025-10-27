/*
 *********************************************************************************
 *** Project:            Heightmap Generator
 *** Creation Date:      2016-01-01
 *** Author:             Peter Ebel (peter.ebel@outlook.de)
 *** Objective:          Conversion of binary HGT files into PNG greyscale pictures
 *** Compile:            gcc hgt2png.c -o hgt2png -std=gnu99 $(pkg-config --cflags --libs libpng) -lm -pthread
 *** Dependencies:       libpng-dev: sudo apt-get install libpng-dev pkg-config
 *** Modification Log:  
 *** Version Date        Modified By   Modification Details
 *** ------------------------------------------------------------------------------
 *** 1.0.0   2016-01-01  Ebel          Initial creation of the script
 *** 1.0.1   2023-10-23  Ebel          Link switch changed to -lpng
 *** 1.1.0   2025-10-23  Ebel          Added Procedural Detail Generation and multithreading support
 **********************************************************************************
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
	
#include <png.h>

#define ChannelBlend_Multiply(A,B)   ((unsigned char)((A * B) / 255))

#define _MAX_PATH    255
#define _MAX_FILES   255
#define _MAX_HEIGHT 6000

// Neue Konstanten für Procedural Detail Generation
#define ENABLE_PROCEDURAL_DETAIL 1

// Parameter-Struktur für Kommandozeilen-Optionen
typedef enum {
  CURVE_LINEAR = 0,
  CURVE_LOG = 1
} CurveType;

typedef struct {
  int scaleFactor;
  float detailIntensity;
  int noiseSeed;
  int enableDetail;
  int verbose;
  int showHelp;
  int showVersion;
  int numThreads;  // Neue Option für Thread-Anzahl
  int output16bit;  // Neue Option für 16-Bit PNG Output
  float gamma;      // Gamma-Korrektur (Standard: 1.0)
  CurveType curveType;  // Kurventyp (linear/log)
  int minHeight;    // Minimale Höhe für Mapping (-1 = Auto)
  int maxHeight;    // Maximale Höhe für Mapping (-1 = Auto)
} ProgramOptions;

// Threading-Strukturen für Parallelisierung
typedef struct {
  int fileIndex;
  struct tag_FileInfoHGT* fileInfo;  // Verwende den vollständigen Strukturnamen
  ProgramOptions* opts;
  int* globalResult;        // Shared result status
  pthread_mutex_t* outputMutex;  // Mutex für thread-sichere Ausgabe
  int* filesProcessed;      // Shared counter für Fortschrittsanzeige
  int totalFiles;           // Gesamtanzahl Dateien
} ThreadData;

// Standard-Werte
#define DEFAULT_SCALE_FACTOR 3
#define DEFAULT_DETAIL_INTENSITY 15.0f
#define DEFAULT_NOISE_SEED 12345
#define DEFAULT_NUM_THREADS 4

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
  int hgtType;              // Pro-Datei HGT-Typ statt globaler Variable
  int noDataCount;          // Anzahl NoData-Pixel für Statistik
} FileInfo, *pFileInfo;

FileInfo *fi;
RGB *PixelData;

void WritePNG(const char *, short int, short int);
void WritePNGWithData(const char *szFilename, short int _iWidth, short int _iHeight, RGB* pixelData);
void WritePNG16BitGrayscale(const char *szFilename, short int _iWidth, short int _iHeight, unsigned short* pixelData);

// Parameter-Management Funktionen
void initDefaultOptions(ProgramOptions *opts);
void showHelp(const char *programName);
void showVersion(void);
int parseArguments(int argc, char *argv[], ProgramOptions *opts, char **inputFile);

// Neue Funktionen für Procedural Detail Generation
float SimpleNoise(int x, int y, int seed);
float FractalNoise(float x, float y, int octaves, float persistence, float scale, int seed);
short int BilinearInterpolate(short int* data, int width, int height, float x, float y);
float GetPixelDistance(int hgtType);
float CalculateLocalSlope(short int* data, int width, int height, float x, float y, int hgtType);
float GetHeightTypeFactor(short int height);
float ApplyCurveMapping(float normalizedValue, CurveType curveType, float gamma);
short int* AddProceduralDetail(short int* originalData, int originalWidth, int originalHeight, 
                               int scaleFactor, float detailIntensity, int seed, int hgtType);

// Neue Funktionen für Parallelisierung
void* processFileWorker(void* arg);
int processFilesParallel(struct tag_FileInfoHGT* fi, int iNumFilesToConvert, ProgramOptions opts);
int processFilesSequential(struct tag_FileInfoHGT* fi, int iNumFilesToConvert, ProgramOptions opts);

// Hilfsfunktion: Extrahiert Dateinamen aus Pfad und generiert PNG-Namen im aktuellen Verzeichnis
void generateOutputFilename(const char* inputPath, char* outputPath);

// NoData-Behandlung
int isNoDataValue(short int value, int hgtType);
short int processElevationValue(short int rawValue, int hgtType, int* noDataCount);

const int HGT_TYPE_30 = 1201;
const int HGT_TYPE_90 = 3601;
const int HGT_TYPE_UNKNOWN = -1;
const unsigned long HGT_TYPE_30_SIZE = 1201 * 1201 * sizeof(short int);
const unsigned long HGT_TYPE_90_SIZE = 3601 * 3601 * sizeof(short int);

// NoData Konstanten (SRTM Standard)
const short int NODATA_VALUE_BE = (short int)0x8000;  // Big Endian: -32768
const short int NODATA_VALUE_LE = (short int)0x0080;  // Little Endian: 128 (nach Byte-Swap)
const short int NODATA_REPLACEMENT = 0;                // Ersatzwert für NoData

// iHGTType globale Variable entfernt - jetzt pro-Datei in FileInfo.hgtType

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
      // sCurrentFilename[0] wurde in diesem Pfad nicht allokiert
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
              sCurrentFilename[cleanup] = NULL;
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
    // Cleanup bereits allokierter Filenames
    for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
      if (sCurrentFilename[cleanup] != NULL) {
        free(sCurrentFilename[cleanup]);
        sCurrentFilename[cleanup] = NULL;
      }
    }
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
      // Cleanup allocated memory
      for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
        if (sCurrentFilename[cleanup] != NULL) {
          free(sCurrentFilename[cleanup]);
          sCurrentFilename[cleanup] = NULL;
        }
      }
      free(fi);
      fi = NULL;
      return 1;
    }

    fstat(fileno(InFile), &buf);
    fi[i].ulFilesize = buf.st_size;

    if (fi[i].ulFilesize == HGT_TYPE_30_SIZE)
    {
      fi[i].hgtType = HGT_TYPE_30;  // Pro-Datei statt global
      fi[i].iWidth = 1201;
      fi[i].iHeight = 1201;
    } else if (fi[i].ulFilesize == HGT_TYPE_90_SIZE)
    {
      fi[i].hgtType = HGT_TYPE_90;  // Pro-Datei statt global
      fi[i].iWidth = 3601;
      fi[i].iHeight = 3601;
    }
    else
    {
      // Custom-Size-Parsing über Dateiname mit Fallback
      fi[i].iWidth = 0;
      fi[i].iHeight = 0;
      
      // Sicheres Parsing nur bei ausreichend langer Dateiname
      size_t nameLen = strlen(fi[i].szFilename);
      if (nameLen >= 15) {  // Mindestens "N00E000_1234x5678.hgt"
        memcpy(sTmp, &sCurrentFilename[i][5], 4);
        sTmp[4] = '\0';
        int parsedWidth = atoi(sTmp);
        
        memcpy(sTmp, &sCurrentFilename[i][10], 4);
        sTmp[4] = '\0';
        int parsedHeight = atoi(sTmp);
        
        // Plausibilitätsprüfung: Dimensionen müssen sinnvoll sein
        if (parsedWidth > 0 && parsedWidth <= 65536 && 
            parsedHeight > 0 && parsedHeight <= 65536) {
          fi[i].iWidth = parsedWidth;
          fi[i].iHeight = parsedHeight;
          fi[i].hgtType = fi[i].iWidth * fi[i].iHeight;
        }
      }
      
      // Fallback: Bei ungültigem Parsing als UNKNOWN markieren
      if (fi[i].iWidth == 0 || fi[i].iHeight == 0) {
        fi[i].hgtType = HGT_TYPE_UNKNOWN;
        if (opts.verbose) {
          fprintf(stderr, "WARNING: Could not parse dimensions from filename %s\n", fi[i].szFilename);
        }
      } else {
        // Zusätzliche Validation: Dateigröße muss mit Dimensionen übereinstimmen
        unsigned long expectedSize = (unsigned long)fi[i].iWidth * fi[i].iHeight * 2; // 2 bytes per pixel
        if (fi[i].ulFilesize != expectedSize) {
          fprintf(stderr, "ERROR: Filesize mismatch for %s: expected %lu bytes (%dx%d), got %lu bytes\n", 
                  fi[i].szFilename, expectedSize, fi[i].iWidth, fi[i].iHeight, fi[i].ulFilesize);
          fi[i].hgtType = HGT_TYPE_UNKNOWN;  // Als ungültig markieren
        }
      }
    }

    if (fi[i].hgtType == HGT_TYPE_UNKNOWN) {  // Pro-Datei Überprüfung
      fprintf(stderr, "Error: %s has an unknown HGT type\n", fi[i].szFilename);
      fclose(InFile);
      // Cleanup allocated memory
      for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
        if (sCurrentFilename[cleanup] != NULL) {
          free(sCurrentFilename[cleanup]);
          sCurrentFilename[cleanup] = NULL;
        }
      }
      free(fi);
      fi = NULL;
      return 1;
    }

    if ((iElevationData = (short *) malloc(fi[i].ulFilesize)) == NULL)
    {
      fprintf(stderr, "Error: Can't allocate memory to load %s\n", fi[i].szFilename);
      fclose(InFile);
      // Cleanup allocated memory
      for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
        if (sCurrentFilename[cleanup] != NULL) {
          free(sCurrentFilename[cleanup]);
          sCurrentFilename[cleanup] = NULL;
        }
      }
      free(fi);
      fi = NULL;
      return 1;
    }

    if ((iNumIntRead = fread(iElevationData, 1, fi[i].ulFilesize, InFile)) != (int)fi[i].ulFilesize)
    {
      fprintf(stderr, "Error: Can't load elevation data\n");
      free(iElevationData);
      iElevationData = NULL;
      fclose(InFile);
      // Cleanup allocated memory
      for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
        if (sCurrentFilename[cleanup] != NULL) {
          free(sCurrentFilename[cleanup]);
          sCurrentFilename[cleanup] = NULL;
        }
      }
      free(fi);
      fi = NULL;
      return 1;
    } 

    iCurrentMaxElevation = 0;
    iCurrentMinElevation = 9999;
    fi[i].noDataCount = 0;  // Initialisiere NoData-Zähler
    
    for (unsigned long j = 0; j < fi[i].ulFilesize / 2; j++)
    {
      // Korrekte NoData-Behandlung mit neuer Funktion
      iElevationData[j] = processElevationValue(iElevationData[j], fi[i].hgtType, &fi[i].noDataCount);
      
      // Min/Max nur für gültige Werte (nicht für NoData-Ersatzwerte)
      if (iElevationData[j] != NODATA_REPLACEMENT) {
        if (iElevationData[j] < iCurrentMinElevation) iCurrentMinElevation = iElevationData[j];
        if (iElevationData[j] > iCurrentMaxElevation) iCurrentMaxElevation = iElevationData[j];
      }
    }

    if (iCurrentMinElevation < iOverallMinElevation) iOverallMinElevation = iCurrentMinElevation;
    if (iCurrentMaxElevation > iOverallMaxElevation) iOverallMaxElevation = iCurrentMaxElevation;
    
    // NoData-Statistik ausgeben - Division durch 0 vermeiden
    if (fi[i].noDataCount > 0) {
        unsigned long totalPixels = fi[i].ulFilesize / 2;
        float noDataPercent = 0.0f;
        if (totalPixels > 0) {
            noDataPercent = (float)fi[i].noDataCount / totalPixels * 100.0f;
        }
        fprintf(stderr, "- MIN=%4d MAX=%4d, NoData=%d (%.1f%%)\n", 
                iCurrentMinElevation, iCurrentMaxElevation, fi[i].noDataCount, noDataPercent);
    } else {
        fprintf(stderr, "- MIN=%4d MAX=%4d\n", iCurrentMinElevation, iCurrentMaxElevation);
    }
    
    free(iElevationData);
    iElevationData = NULL;
    fclose(InFile);
  }

  // PARALLELISIERTE BATCH-VERARBEITUNG - Ersetzt die ursprüngliche Schleife
  int processingResult = processFilesParallel(fi, iNumFilesToConvert, opts);
  
  if (processingResult != 0) {
    // Cleanup bei Fehler
    for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++) {
      if (sCurrentFilename[cleanup] != NULL) {
        free(sCurrentFilename[cleanup]);
        sCurrentFilename[cleanup] = NULL;
      }
    }
    free(fi);
    fi = NULL;
    return processingResult;
  }

  // Cleanup allocated memory
  free(fi);
  fi = NULL;
  
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
  WritePNGWithData(szFilename, _iWidth, _iHeight, PixelData);
}

void WritePNGWithData(const char *szFilename, short int _iWidth, short int _iHeight, RGB* pixelData)
{
  png_image image; 

  memset(&image, 0, sizeof image);

  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_RGB;
  image.width = _iWidth;
  image.height = _iHeight;

  fprintf(stderr, "Info: Writing %s\n", szFilename);
  if (!png_image_write_to_file(&image, szFilename, 0/*convert_to_8bit*/, (png_bytep) pixelData, 0/*row_stride*/, NULL/*colormap*/))
  {
    fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
  }

  png_image_free(&image);
}

void WritePNG16BitGrayscale(const char *szFilename, short int _iWidth, short int _iHeight, unsigned short* pixelData)
{
  png_image image; 

  memset(&image, 0, sizeof image);

  image.version = PNG_IMAGE_VERSION;
  image.format = PNG_FORMAT_LINEAR_Y;  // 16-bit Grayscale  
  image.width = _iWidth;
  image.height = _iHeight;

  fprintf(stderr, "Info: Writing 16-bit %s\n", szFilename);
  if (!png_image_write_to_file(&image, szFilename, 0/*convert_to_8bit*/, (png_bytep) pixelData, 0/*row_stride*/, NULL/*colormap*/))
  {
    fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
  }

  png_image_free(&image);
}

// PARALLELISIERUNG - Worker-Thread Funktion
void* processFileWorker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    struct tag_FileInfoHGT* fi = &data->fileInfo[data->fileIndex];
    ProgramOptions* opts = data->opts;
    
    FILE* InFile = NULL;
    short* iElevationData = NULL;
    RGB* PixelData = NULL;
    int iNumIntRead;
    char OutputHeightmapFile[256];
    
    // Thread-sichere Fortschrittsanzeige
    pthread_mutex_lock(data->outputMutex);
    if (opts->verbose) {
        fprintf(stderr, "INFO: Processing file %d/%d: %s\n", 
                data->fileIndex + 1, data->totalFiles, fi->szFilename);
    }
    pthread_mutex_unlock(data->outputMutex);
    
    // Datei öffnen
    if ((InFile = fopen(fi->szFilename, "rb")) == NULL) {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Can't open input file %s\n", fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        *(data->globalResult) = 1;
        return NULL; // Kein Speicher zu leaken hier
    }

    // Speicher für Elevation Data allokieren
    if ((iElevationData = (short*)malloc(fi->ulFilesize)) == NULL) {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Can't allocate elevation data block %s\n", fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        fclose(InFile);
        *(data->globalResult) = 1;
        return NULL; // File wurde geschlossen, kein Speicher allokiert
    }

    // Elevation Data lesen
    if ((iNumIntRead = fread(iElevationData, 1, fi->ulFilesize, InFile)) != (int)fi->ulFilesize) {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Can't load elevation data from %s\n", fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        fclose(InFile);
        free(iElevationData);
        iElevationData = NULL;
        *(data->globalResult) = 1;
        return NULL;
    }
    fclose(InFile);

    // PROCEDURAL DETAIL GENERATION
    if (opts->enableDetail) {
        // Korrekte NoData-Behandlung für Worker-Thread
        int threadNoDataCount = 0;
        for (unsigned long pre = 0; pre < (fi->ulFilesize) / 2; pre++) {
            iElevationData[pre] = processElevationValue(iElevationData[pre], fi->hgtType, &threadNoDataCount);
        }
        
        // NoData-Statistik thread-safe aktualisieren
        pthread_mutex_lock(data->outputMutex);
        fi->noDataCount += threadNoDataCount;
        if (opts->verbose && threadNoDataCount > 0) {
            fprintf(stderr, "INFO: Found %d NoData values in %s\n", threadNoDataCount, fi->szFilename);
        }
        pthread_mutex_unlock(data->outputMutex);
        
        // Procedural Detail hinzufügen
        short int* detailedData = AddProceduralDetail(iElevationData, fi->iWidth, fi->iHeight, 
                                                     opts->scaleFactor, opts->detailIntensity, opts->noiseSeed, fi->hgtType);
        
        if (detailedData) {
            free(iElevationData);
            iElevationData = detailedData;
            fi->iWidth *= opts->scaleFactor;
            fi->iHeight *= opts->scaleFactor;
            fi->ulFilesize = fi->iWidth * fi->iHeight * sizeof(short int);
            
            pthread_mutex_lock(data->outputMutex);
            if (opts->verbose) {
                fprintf(stderr, "INFO: Enhanced resolution: %dx%d pixels for %s\n", 
                        fi->iWidth, fi->iHeight, fi->szFilename);
            }
            pthread_mutex_unlock(data->outputMutex);
        } else {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "WARNING: Could not add procedural detail to %s, using original data\n", 
                    fi->szFilename);
            pthread_mutex_unlock(data->outputMutex);
        }
    }

    // Speicher für Pixel Data allokieren - abhängig vom Output-Format
    unsigned long pixelCount = fi->iWidth * fi->iHeight;
    unsigned short* PixelData16 = NULL;
    
    if (opts->output16bit) {
        // 16-Bit Grayscale-Daten
        if ((PixelData16 = (unsigned short*)malloc(pixelCount * sizeof(unsigned short))) == NULL) {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate 16-bit pixel data block for %s (%lu pixels)\n", 
                    fi->szFilename, pixelCount);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    } else {
        // 8-Bit RGB-Daten
        if ((PixelData = (RGB*)malloc(pixelCount * sizeof(struct tag_RGB))) == NULL) {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate RGB pixel data block for %s (%lu pixels)\n", 
                    fi->szFilename, pixelCount);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    }
    
    // Effektive Min/Max-Höhen berechnen (Custom Range oder Auto-Detection)
    int effectiveMinHeight = (opts->minHeight != -1) ? opts->minHeight : iOverallMinElevation;
    int effectiveMaxHeight = (opts->maxHeight != -1) ? opts->maxHeight : iOverallMaxElevation;
    
    // Sicherheitscheck: Min < Max
    if (effectiveMinHeight >= effectiveMaxHeight) {
        effectiveMinHeight = iOverallMinElevation;
        effectiveMaxHeight = iOverallMaxElevation;
    }
     
    // Elevation zu Pixel konvertieren - abhängig vom Output-Format
    for (unsigned long m = 0; m < pixelCount; m++) {
        if (!opts->enableDetail) {
            // Korrekte NoData-Behandlung ohne Detail-Generation
            int threadNoDataCount = 0;
            iElevationData[m] = processElevationValue(iElevationData[m], fi->hgtType, &threadNoDataCount);
            fi->noDataCount += threadNoDataCount;  // Thread-safe da sequentiell
        }

        // Normalisierung auf 0.0-1.0 mit Custom-Range
        float normalizedValue = 0.5f;  // Fallback
        if (effectiveMaxHeight > effectiveMinHeight) {
            // Clamp elevation zu effektivem Range
            int clampedElevation = iElevationData[m];
            if (clampedElevation < effectiveMinHeight) clampedElevation = effectiveMinHeight;
            if (clampedElevation > effectiveMaxHeight) clampedElevation = effectiveMaxHeight;
            
            normalizedValue = (float)(clampedElevation - effectiveMinHeight) / 
                             (float)(effectiveMaxHeight - effectiveMinHeight);
        }
        
        // Kurven-Mapping anwenden
        float curvedValue = ApplyCurveMapping(normalizedValue, opts->curveType, opts->gamma);
        
        if (opts->output16bit) {
            // 16-Bit Grayscale: Volle 16-Bit-Range nutzen (0-65535)
            PixelData16[m] = (unsigned short)(curvedValue * 65535.0f);
        } else {
            // 8-Bit RGB: Kurven-korrigierte Berechnung
            unsigned char pixelValue = (unsigned char)(curvedValue * 255.0f);
            PixelData[m].r = PixelData[m].g = PixelData[m].b = pixelValue;
        }
    }

    // PNG Output-Dateiname generieren (nur Dateiname, nicht kompletter Pfad)
    generateOutputFilename(fi->szFilename, OutputHeightmapFile);
    
    // PNG schreiben (thread-safe mit lokalen PixelData)
    pthread_mutex_lock(data->outputMutex);
    if (opts->output16bit) {
        WritePNG16BitGrayscale(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelData16);
    } else {
        WritePNGWithData(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelData);
    }
    pthread_mutex_unlock(data->outputMutex);

    // Cleanup für diesen Thread
    free(iElevationData);
    iElevationData = NULL;
    if (opts->output16bit) {
        free(PixelData16);
        PixelData16 = NULL;
    } else {
        free(PixelData);  // Thread-lokale PixelData freigeben
        PixelData = NULL;
    }
    
    // Fortschrittszähler aktualisieren
    pthread_mutex_lock(data->outputMutex);
    (*(data->filesProcessed))++;
    if (opts->verbose) {
        fprintf(stderr, "INFO: Completed %d/%d files\n", *(data->filesProcessed), data->totalFiles);
    }
    pthread_mutex_unlock(data->outputMutex);
    
    return NULL;
}

// PARALLELISIERTE BATCH-VERARBEITUNG
int processFilesParallel(struct tag_FileInfoHGT* fi, int iNumFilesToConvert, ProgramOptions opts) {
    if (iNumFilesToConvert <= 1 || opts.numThreads <= 1) {
        // Fallback auf sequentielle Verarbeitung bei wenigen Dateien oder 1 Thread
        return processFilesSequential(fi, iNumFilesToConvert, opts);
    }
    
    pthread_t* threads;
    ThreadData* threadData;
    pthread_mutex_t outputMutex = PTHREAD_MUTEX_INITIALIZER;
    int globalResult = 0;
    int filesProcessed = 0;
    
    // Anzahl Threads an verfügbare Dateien anpassen
    int actualThreads = (opts.numThreads > iNumFilesToConvert) ? iNumFilesToConvert : opts.numThreads;
    
    if (opts.verbose) {
        fprintf(stderr, "INFO: Starting parallel processing with %d threads for %d files\n", 
                actualThreads, iNumFilesToConvert);
    }
    
    // Thread-Arrays allokieren
    threads = (pthread_t*)malloc(actualThreads * sizeof(pthread_t));
    threadData = (ThreadData*)malloc(iNumFilesToConvert * sizeof(ThreadData));
    
    if (!threads || !threadData) {
        fprintf(stderr, "Error: Cannot allocate memory for threading\n");
        if (threads) free(threads);
        if (threadData) free(threadData);
        return 1;
    }
    
    // Thread-Daten für alle Dateien vorbereiten
    for (int i = 0; i < iNumFilesToConvert; i++) {
        threadData[i].fileIndex = i;
        threadData[i].fileInfo = fi;
        threadData[i].opts = &opts;
        threadData[i].globalResult = &globalResult;
        threadData[i].outputMutex = &outputMutex;
        threadData[i].filesProcessed = &filesProcessed;
        threadData[i].totalFiles = iNumFilesToConvert;
    }
    
    // Threads in Batches starten (Thread-Pool Simulation)
    int fileIndex = 0;
    while (fileIndex < iNumFilesToConvert && globalResult == 0) {
        int threadsToStart = (iNumFilesToConvert - fileIndex > actualThreads) ? 
                           actualThreads : (iNumFilesToConvert - fileIndex);
        
        // Threads starten
        for (int t = 0; t < threadsToStart; t++) {
            if (pthread_create(&threads[t], NULL, processFileWorker, &threadData[fileIndex + t]) != 0) {
                fprintf(stderr, "Error: Cannot create thread %d\n", t);
                globalResult = 1;
                break;
            }
        }
        
        // Auf alle gestarteten Threads warten
        for (int t = 0; t < threadsToStart; t++) {
            pthread_join(threads[t], NULL);
        }
        
        fileIndex += threadsToStart;
    }
    
    // Cleanup
    free(threads);
    free(threadData);
    pthread_mutex_destroy(&outputMutex);
    
    if (opts.verbose) {
        fprintf(stderr, "INFO: Parallel processing completed. Result: %s\n", 
                globalResult == 0 ? "SUCCESS" : "ERROR");
    }
    
    return globalResult;
}

// SEQUENTIELLE VERARBEITUNG (Fallback)
int processFilesSequential(struct tag_FileInfoHGT* fi, int iNumFilesToConvert, ProgramOptions opts) {
    if (opts.verbose) {
        fprintf(stderr, "INFO: Using sequential processing for %d files\n", iNumFilesToConvert);
    }
    
    // Die ursprüngliche Schleife als separate Funktion
    for (int k = 0; k < iNumFilesToConvert; k++) {
        // Verwende die Worker-Logik, aber ohne Threading
        ThreadData data;
        data.fileIndex = k;
        data.fileInfo = fi;
        data.opts = &opts;
        int result = 0;
        data.globalResult = &result;
        pthread_mutex_t dummyMutex = PTHREAD_MUTEX_INITIALIZER;
        data.outputMutex = &dummyMutex;
        int processed = k;
        data.filesProcessed = &processed;
        data.totalFiles = iNumFilesToConvert;
        
        processFileWorker(&data);
        
        if (result != 0) {
            pthread_mutex_destroy(&dummyMutex);
            return result;
        }
        pthread_mutex_destroy(&dummyMutex);
    }
    
    return 0;
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
// Pixelabstand basierend auf HGT-Typ berechnen
float GetPixelDistance(int hgtType) {
    if (hgtType == HGT_TYPE_30) {
        return 60.0f; // 2 * 30m für SRTM-1 (1201x1201)
    } else if (hgtType == HGT_TYPE_90) {
        return 180.0f; // 2 * 90m für SRTM-3 (3601x3601)  
    } else {
        // Custom-Size: Annahme mittlere Auflösung
        return 60.0f; // Fallback auf SRTM-1 Auflösung
    }
}

float CalculateLocalSlope(short int* data, int width, int height, float x, float y, int hgtType) {
    int ix = (int)x;
    int iy = (int)y;
    
    if (ix <= 0 || ix >= width-1 || iy <= 0 || iy >= height-1) return 0.0f;
    
    short int left = data[iy * width + (ix-1)];
    short int right = data[iy * width + (ix+1)];
    short int top = data[(iy-1) * width + ix];
    short int bottom = data[(iy+1) * width + ix];
    
    float pixelDistance = GetPixelDistance(hgtType);
    float dx = (right - left) / pixelDistance;
    float dy = (bottom - top) / pixelDistance;
    
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

// Kurven-Mapping für verschiedene Elevation-zu-Pixel-Transformationen
float ApplyCurveMapping(float normalizedValue, CurveType curveType, float gamma) {
    // Eingabe-Clipping
    if (normalizedValue < 0.0f) normalizedValue = 0.0f;
    if (normalizedValue > 1.0f) normalizedValue = 1.0f;
    
    float result = normalizedValue;
    
    switch (curveType) {
        case CURVE_LINEAR:
            // Lineare Kurve - keine Änderung
            result = normalizedValue;
            break;
            
        case CURVE_LOG:
            // Logarithmische Kurve - betont niedrige Werte
            if (normalizedValue > 0.0f) {
                result = logf(1.0f + normalizedValue * 9.0f) / logf(10.0f);  // log10(1 + x*9)
            } else {
                result = 0.0f;
            }
            break;
    }
    
    // Gamma-Korrektur anwenden
    if (gamma != 1.0f) {
        result = powf(result, 1.0f / gamma);
    }
    
    // Ausgabe-Clipping
    if (result < 0.0f) result = 0.0f;
    if (result > 1.0f) result = 1.0f;
    
    return result;
}

// Hauptfunktion für Procedural Detail Generation
short int* AddProceduralDetail(short int* originalData, int originalWidth, int originalHeight, 
                               int scaleFactor, float detailIntensity, int seed, int hgtType) {
    
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
            float localSlope = CalculateLocalSlope(originalData, originalWidth, originalHeight, srcX, srcY, hgtType);
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
        
        // Intelligente Fortschrittsanzeige - Division durch 0 vermeiden
        static int lastProgress = -1;  // Verhindert doppelte Ausgaben
        int currentProgress = (y * 100) / newHeight;
        
        // Progress-Intervall abhängig von Bildgröße
        int progressInterval;
        if (newHeight >= 10000) {
            progressInterval = 10;  // Alle 10% bei großen Bildern
        } else if (newHeight >= 1000) {
            progressInterval = 20;  // Alle 20% bei mittleren Bildern  
        } else if (newHeight >= 100) {
            progressInterval = 50;  // Alle 50% bei kleinen Bildern
        } else {
            progressInterval = 100; // Nur am Ende bei sehr kleinen Bildern
        }
        
        // Ausgabe nur bei Intervall-Änderung
        if (currentProgress >= lastProgress + progressInterval || currentProgress == 100) {
            fprintf(stderr, "INFO: Progress: %d%%\n", currentProgress);
            lastProgress = currentProgress;
        }
    }
    
    return detailedData;
}

// HILFSFUNKTIONEN

// Extrahiert Dateinamen aus Pfad und generiert PNG-Namen im aktuellen Verzeichnis
void generateOutputFilename(const char* inputPath, char* outputPath) {
    const char* filename = inputPath;
    const char* lastSlash = strrchr(inputPath, '/');
    
    // Wenn ein Pfad-Separator gefunden wurde, nimm den Teil danach
    if (lastSlash != NULL) {
        filename = lastSlash + 1;
    }
    
    // Kopiere den Dateinamen und ersetze die Endung
    strcpy(outputPath, filename);
    
    // Finde die letzte .hgt Endung und ersetze sie durch .png
    char* extension = strrchr(outputPath, '.');
    if (extension != NULL && (strcmp(extension, ".hgt") == 0 || strcmp(extension, ".HGT") == 0)) {
        strcpy(extension, ".png");
    } else {
        // Falls keine .hgt Endung gefunden wurde, hänge .png an
        strcat(outputPath, ".png");
    }
}

// NODATA-BEHANDLUNG

// Prüft ob ein Rohwert ein NoData-Wert ist (vor oder nach Byte-Swap)
int isNoDataValue(short int value, int hgtType) {
    // Nur für SRTM-Daten (30m/90m) NoData-Erkennung
    if (hgtType != HGT_TYPE_30 && hgtType != HGT_TYPE_90) {
        return 0;
    }
    
    // NoData-Wert in beiden Byte-Orders erkennen
    return (value == NODATA_VALUE_BE || value == NODATA_VALUE_LE);
}

// Verarbeitet einen Höhenwert: Byte-Swap + NoData-Behandlung + Clamping
short int processElevationValue(short int rawValue, int hgtType, int* noDataCount) {
    // 1. NoData-Erkennung VOR Byte-Swap (kritisch!)
    if (isNoDataValue(rawValue, hgtType)) {
        if (noDataCount != NULL) {
            (*noDataCount)++;
        }
        return NODATA_REPLACEMENT;  // Ersatzwert für NoData
    }
    
    // 2. Byte-Swapping für SRTM-Daten
    short int swappedValue = rawValue;
    if (hgtType == HGT_TYPE_30 || hgtType == HGT_TYPE_90) {
        swappedValue = (short)(((rawValue & 0xff) << 8) | ((rawValue & 0xff00) >> 8));
    }
    
    // 3. Nochmalige NoData-Prüfung nach Byte-Swap (Sicherheit)
    if (isNoDataValue(swappedValue, hgtType)) {
        if (noDataCount != NULL) {
            (*noDataCount)++;
        }
        return NODATA_REPLACEMENT;
    }
    
    // 4. Clamping auf gültige Höhenwerte
    if (swappedValue < 0) swappedValue = 0;
    if (swappedValue > _MAX_HEIGHT) swappedValue = _MAX_HEIGHT;
    
    return swappedValue;
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
    opts->numThreads = DEFAULT_NUM_THREADS;  // Neue Option für Thread-Anzahl
    opts->output16bit = 0;  // Standard: 8-Bit RGB Output
    opts->gamma = 1.0f;     // Standard: Keine Gamma-Korrektur
    opts->curveType = CURVE_LINEAR;  // Standard: Lineare Kurve
    opts->minHeight = -1;   // Auto-Erkennung
    opts->maxHeight = -1;   // Auto-Erkennung
}

// Zeige Hilfe-Text
void showHelp(const char *programName) {
    printf("hgt2png v1.1.0 - HGT to PNG Heightmap Converter with Procedural Detail Generation\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] <input.hgt|filelist.txt>\n\n", programName);
    
    printf("OPTIONS:\n");
    printf("  -s, --scale-factor <n>   Scale factor for resolution enhancement (default: %d)\n", DEFAULT_SCALE_FACTOR);
    printf("                           1=original, 2=double, 3=triple resolution\n");
    printf("  -i, --detail-intensity <f> Detail intensity in meters (default: %.1f)\n", DEFAULT_DETAIL_INTENSITY);
    printf("                           Higher values = more pronounced details\n");
    printf("  -r, --noise-seed <n>     Random seed for procedural generation (default: %d)\n", DEFAULT_NOISE_SEED);
    printf("  -t, --threads <n>        Number of parallel threads (default: %d)\n", DEFAULT_NUM_THREADS);
    printf("                           1=sequential, 2-16=parallel processing\n");
    printf("  -d, --disable-detail     Disable procedural detail generation\n");
    printf("  -q, --quiet              Suppress verbose output\n");
    printf("      --16bit              Generate 16-bit grayscale PNG (better for displacement maps)\n");
    printf("  -g, --gamma <f>          Gamma correction curve (default: 1.0, range: 0.1-10.0)\n");
    printf("  -c, --curve <type>       Mapping curve: linear|log (default: linear)\n");
    printf("  -m, --min-height <n>     Minimum elevation for mapping (default: auto)\n");
    printf("  -M, --max-height <n>     Maximum elevation for mapping (default: auto)\n");
    printf("  -h, --help               Show this help message\n");
    printf("  -v, --version            Show version information\n\n");
    
    printf("INPUT:\n");
    printf("  Single HGT file:         %s terrain.hgt\n", programName);
    printf("  Multiple files (list):   %s filelist.txt\n\n", programName);
    
    printf("EXAMPLES:\n");
    printf("  %s N48E011.hgt                          # Standard processing\n", programName);
    printf("  %s -s 2 -i 25.0 N48E011.hgt             # 2x scale, high detail\n", programName);
    printf("  %s -t 8 terrain_files.txt               # 8 parallel threads\n", programName);
    printf("  %s -d -q terrain_files.txt              # No detail, quiet mode\n", programName);
    printf("  %s --threads 4 --scale-factor 3 --detail-intensity 10  # Parallel + custom\n\n", programName);
    
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
        {"threads",          required_argument, 0, 't'},
        {"disable-detail",   no_argument,       0, 'd'},
        {"quiet",            no_argument,       0, 'q'},
        {"16bit",            no_argument,       0, '6'},
        {"gamma",            required_argument, 0, 'g'},
        {"curve",            required_argument, 0, 'c'},
        {"min-height",       required_argument, 0, 'm'},
        {"max-height",       required_argument, 0, 'M'},
        {"help",             no_argument,       0, 'h'},
        {"version",          no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "s:i:r:t:dq6g:c:m:M:hv", long_options, NULL)) != -1) {
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
                
            case 't':
                opts->numThreads = atoi(optarg);
                if (opts->numThreads < 1 || opts->numThreads > 16) {
                    fprintf(stderr, "Error: Number of threads must be between 1 and 16\n");
                    return 1;
                }
                break;
                
            case 'd':
                opts->enableDetail = 0;
                break;
                
            case 'q':
                opts->verbose = 0;
                break;
                
            case '6':
                opts->output16bit = 1;
                break;
                
            case 'g':
                opts->gamma = atof(optarg);
                if (opts->gamma <= 0.1f || opts->gamma > 10.0f) {
                    fprintf(stderr, "Error: Gamma must be between 0.1 and 10.0\n");
                    return 1;
                }
                break;
                
            case 'c':
                if (strcmp(optarg, "linear") == 0) {
                    opts->curveType = CURVE_LINEAR;
                } else if (strcmp(optarg, "log") == 0) {
                    opts->curveType = CURVE_LOG;
                } else {
                    fprintf(stderr, "Error: Curve type must be 'linear' or 'log'\n");
                    return 1;
                }
                break;
                
            case 'm':
                opts->minHeight = atoi(optarg);
                break;
                
            case 'M':
                opts->maxHeight = atoi(optarg);
                if (opts->minHeight != -1 && opts->maxHeight != -1 && opts->maxHeight <= opts->minHeight) {
                    fprintf(stderr, "Error: max-height must be greater than min-height\n");
                    return 1;
                }
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
