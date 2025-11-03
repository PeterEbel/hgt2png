/*
 * HGT2PNG - Professional Heightmap Converter v1.1.0
 *
 * Copyright (c) 2025 Peter Ebel <peter.ebel@outlook.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *********************************************************************************
 *** Project:            HGT to PNG Heightmap Converter with OpenMP Optimization
 *** Creation Date:      2016-01-01 (Original), 2025-10-27 (v1.1.0 Rewrite)
 *** Author:             Peter Ebel (peter.ebel@outlook.de)
 *** Objective:          Professional conversion of SRTM HGT files to PNG displacement maps
 *** Compile:            gcc hgt2png.c -o hgt2png -std=gnu99 $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp -mavx2 -O3
 *** Dependencies:       libpng-dev: sudo apt-get install libpng-dev pkg-config
 *** GitHub:             https://github.com/PeterEbel/hgt2png
 ***
 *** Version History:
 *** ------------------------------------------------------------------------------
 *** 1.0.0   2016-01-01  Ebel          Initial creation of the script
 *** 1.0.1   2023-10-23  Ebel          Link switch changed to -lpng
 *** 1.1.0   2025-10-27  Ebel          OpenMP optimization, 16-bit PNG, alpha transparency,
 ***                                   const-correctness, memory safety, professional features
 **********************************************************************************
 */

#define _POSIX_C_SOURCE 200112L // For aligned_alloc
#define _USE_MATH_DEFINES       // For M_PI on Windows/some systems
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h> // For SIZE_MAX and explicit integer types
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#ifdef _OPENMP
#include <omp.h> // OpenMP for SIMD vectorization and parallelization
#endif
#include <arpa/inet.h> // For ntohs/htons (network byte order)

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>

#define _MAX_PATH 255
#define _MAX_FILES 255
#define _MAX_HEIGHT 6000

// New constants for Procedural Detail Generation
#define ENABLE_PROCEDURAL_DETAIL 1

// Parameter structure for command line options
typedef enum
{
    CURVE_LINEAR = 0,
    CURVE_LOG = 1
} CurveType;

typedef enum
{
    METADATA_NONE = 0,
    METADATA_JSON = 1,
    METADATA_TXT = 2
} MetadataFormat;

// Vegetation Mask Parameters for different biomes
typedef struct
{
    int enabled;          // Enable vegetation mask generation
    float minElevation;   // Minimum elevation for vegetation (meters)
    float maxElevation;   // Maximum elevation for vegetation (meters)
    float maxSlope;       // Maximum slope angle for vegetation (degrees)
    float aspectModifier; // North/South face modifier (-1.0 to 1.0)
    float drainageBonus;  // Bonus for valleys/drainage areas
    float treeLine;       // Tree line elevation (meters)
    float bushLine;       // Bush line elevation (meters)
    float grassLine;      // Grass line elevation (meters)
} VegetationParams;

typedef enum
{
    BIOME_ALPINE = 0,
    BIOME_TEMPERATE = 1,
    BIOME_TROPICAL = 2,
    BIOME_DESERT = 3,
    BIOME_ARCTIC = 4
} BiomeType;

typedef struct
{
    int scaleFactor;
    float detailIntensity;
    int noiseSeed;
    int enableDetail;
    int verbose;
    int showHelp;
    int showVersion;
    int numThreads;                // Option for thread count
    int output16bit;               // Option for 16-bit PNG output
    float gamma;                   // Gamma correction (default: 1.0)
    CurveType curveType;           // Curve type (linear/log)
    int minHeight;                 // Minimum height for mapping (-1 = Auto)
    int maxHeight;                 // Maximum height for mapping (-1 = Auto)
    MetadataFormat metadataFormat; // Metadata format (none/json/txt)
    int alphaNoData;               // Option for transparent NoData pixels
    VegetationParams vegetation;   // Vegetation mask parameters
    BiomeType biome;               // Selected biome type
} ProgramOptions;

// Threading structures for parallelization
typedef struct
{
    int fileIndex;
    struct tag_FileInfoSRTM *fileInfo; // Use full structure name
    ProgramOptions *opts;
    int *globalResult;            // Shared result status
    pthread_mutex_t *outputMutex; // Mutex for thread-safe output
    int *filesProcessed;          // Shared counter for progress indication
    int totalFiles;               // Total number of files
} ThreadData;

// Default values
#define DEFAULT_SCALE_FACTOR 3
#define DEFAULT_DETAIL_INTENSITY 15.0f
#define DEFAULT_NOISE_SEED 12345
#define DEFAULT_NUM_THREADS 4

// Alpine Biome Configuration - Realistic European Alps Parameters
#define ALPINE_MIN_ELEVATION 700.0f  // Vegetation starts at ~700m (montane zone)
#define ALPINE_MAX_ELEVATION 2000.0f // Vegetation ends at ~2000m (alpine zone)
#define ALPINE_MAX_SLOPE 60.0f       // Maximum slope for vegetation (degrees)
#define ALPINE_TREE_LINE 1800.0f     // Tree line elevation (spruce/larch limit)
#define ALPINE_BUSH_LINE 2200.0f     // Shrub line (dwarf pine, rhododendron)
#define ALPINE_GRASS_LINE 2500.0f    // Grass line (alpine meadows)
#define ALPINE_ASPECT_MODIFIER 0.3f  // South faces drier, North faces moister
#define ALPINE_DRAINAGE_BONUS 0.4f   // Valley bottoms have more vegetation

// Vegetation density calculation functions
static float calculate_slope_angle(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, float pixel_pitch_meters);
static float calculate_aspect_angle(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, float pixel_pitch_meters);
static float calculate_drainage_factor(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, int radius);
static uint8_t calculate_vegetation_density_alpine(float elevation, float slope, float aspect, float drainage, const VegetationParams *params);
static void initialize_alpine_biome(VegetationParams *params);
static int generate_vegetation_mask(const struct tag_FileInfoSRTM *fi, const int16_t *elevation_data, const ProgramOptions *opts);

typedef int errno_t;

typedef struct tag_RGB
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RGB, *pRGB;

typedef struct tag_RGBA
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} RGBA, *pRGBA;

typedef struct tag_RGBA16
{
    uint16_t y; // 16-bit Grayscale/Luminance
    uint16_t a; // 16-bit Alpha
} RGBA16, *pRGBA16;

typedef struct tag_FileInfoSRTM
{
    char szFilename[_MAX_PATH];
    size_t iWidth;
    size_t iHeight;
    int16_t iMinElevation; // Explicitly signed 16-bit for elevation values
    int16_t iMaxElevation; // Explicitly signed 16-bit for elevation values
    size_t ulFilesize;
    int srtmType;           // Per-file SRTM type instead of global variable
    int noDataCount;       // Number of NoData pixels for statistics
} FileInfo, *pFileInfo;

FileInfo *fi;
RGB *PixelData;

void WritePNG(const char *, size_t, size_t);
static void WritePNGWithData(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGB *pixelData);
static void WritePNGWithAlpha(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGBA *pixelData);
static void WritePNG16BitGrayscale(const char *szFilename, size_t _iWidth, size_t _iHeight, const uint16_t *pixelData);
static void WritePNG16BitWithAlpha(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGBA16 *pixelData);

// Parameter management functions
static void initDefaultOptions(ProgramOptions *opts);
static void showHelp(const char *programName);
static void showVersion(void);
static int parseArguments(int argc, char *argv[], ProgramOptions *opts, char **inputFile);

// Safe multiplication with overflow detection
static int safe_multiply_size_t(size_t a, size_t b, size_t *result)
{
    if (a == 0 || b == 0)
    {
        *result = 0;
        return 1;
    }

    // Overflow check: if a * b > SIZE_MAX, then a > SIZE_MAX / b
    if (a > SIZE_MAX / b)
    {
        return 0; // Overflow detected
    }

    *result = a * b;
    return 1; // Success
}

// Safe malloc with overflow protection
static void *safe_malloc_pixels(size_t width, size_t height, size_t element_size)
{
    size_t pixel_count;
    size_t total_bytes;

    // First calculate width * height
    if (!safe_multiply_size_t(width, height, &pixel_count))
    {
        fprintf(stderr, "Error: Pixel count overflow at %zu x %zu\n", width, height);
        return NULL;
    }

    // Then pixel_count * element_size
    if (!safe_multiply_size_t(pixel_count, element_size, &total_bytes))
    {
        fprintf(stderr, "Error: Memory size overflow at %zu pixels × %zu bytes\n",
                pixel_count, element_size);
        return NULL;
    }

// Optimized memory alignment for SIMD vectorization
// 32-byte alignment for AVX2 (8 float values in parallel)
#ifdef _OPENMP
    const size_t alignment = 32; // AVX2-optimized
    size_t aligned_bytes = (total_bytes + alignment - 1) & ~(alignment - 1);

    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, aligned_bytes) != 0)
    {
        // Fallback to normal malloc if posix_memalign fails
        ptr = malloc(total_bytes);
    }
#else
    void *ptr = malloc(total_bytes);
#endif

    if (!ptr)
    {
        fprintf(stderr, "Error: malloc failed for %zu bytes (%zu×%zu×%zu)\n",
                total_bytes, width, height, element_size);
    }

    return ptr;
}

// New functions for Procedural Detail Generation
static float SimpleNoise(int x, int y, int seed);
static float FractalNoise(float x, float y, int octaves, float persistence, float scale, int seed);
static short int BilinearInterpolate(const short int *data, int width, int height, float x, float y);
static float GetPixelDistance(int srtmType);
static float CalculateLocalSlope(const short int *data, int width, int height, float x, float y, int srtmType);
static float GetHeightTypeFactor(short int height);
static float ApplyCurveMapping(float normalizedValue, CurveType curveType, float gamma);
static int extractGeoBounds(const char *filename, float *south, float *north, float *west, float *east);
static void writeMetadataFile(const char *pngFilename, const ProgramOptions *opts, const struct tag_FileInfoSRTM *fi, int effectiveMinHeight, int effectiveMaxHeight);
static short int *AddProceduralDetail(const short int *originalData, int originalWidth, int originalHeight, int scaleFactor, float detailIntensity, int seed, int srtmType);

// Functions for parallelization
static void *processFileWorker(void *arg);
static int processFilesParallel(struct tag_FileInfoSRTM *fi, int iNumFilesToConvert, ProgramOptions opts);
static int processFilesSequential(struct tag_FileInfoSRTM *fi, int iNumFilesToConvert, ProgramOptions opts);

// Helper function: Extracts filename from path and generates PNG name in current directory
static void generateOutputFilename(const char *inputPath, char *outputPath);

// NoData handling
static int isNoDataValue(int16_t value, int srtmType);
static int16_t processElevationValue(int16_t rawValue, int srtmType, int *noDataCount);

const int SRTM3 = 1201;
const int SRTM1 = 3601;
const int SRTM_UNKNOWN = -1;
const size_t SRTM3_SIZE = 1201 * 1201 * sizeof(int16_t);
const size_t SRTM1_SIZE = 3601 * 3601 * sizeof(int16_t);

// NoData constants (SRTM standard) - Big Endian format
const int16_t NODATA_VALUE_BE = (int16_t)0x8000; // Big Endian: -32768
const int16_t NODATA_REPLACEMENT = 0;            // Replacement value for NoData

char OutputHeightmapFile[_MAX_PATH];

int16_t iOverallMinElevation;
int16_t iOverallMaxElevation;
int16_t iCurrentMinElevation;
int16_t iCurrentMaxElevation;

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

    // Initialize default options
    initDefaultOptions(&opts);

    // Parse command line arguments
    int parseResult = parseArguments(argc, argv, &opts, &inputFile);
    if (parseResult != 0)
    {
        return parseResult;
    }

    // Show version if requested
    if (opts.showVersion)
    {
        showVersion();
        return 0;
    }

    // Show help if requested or no input file
    if (opts.showHelp || inputFile == NULL)
    {
        showHelp(argv[0]);
        return opts.showHelp ? 0 : 1;
    }

    if (opts.verbose)
    {
        fprintf(stderr, "\nhgt2png Converter v1.1.0 (C) 2025 - with Procedural Detail Generation\n");
        fprintf(stderr, "Scale Factor: %d, Detail Intensity: %.1f, Seed: %d\n",
                opts.scaleFactor, opts.detailIntensity, opts.noiseSeed);
    }

    if ((strstr(inputFile, "HGT") != NULL) || strstr(inputFile, "hgt") != NULL)
    {
        if (opts.verbose)
            fprintf(stderr, "INFO: Single-File Mode\n");
        sCurrentFilename[0] = (char *)malloc(strlen(inputFile) + 1);
        if (sCurrentFilename[0] == NULL)
        {
            fprintf(stderr, "Error: Can't allocate memory for filename\n");
            return 1;
        }
        strcpy(sCurrentFilename[0], inputFile);
        iNumFilesToConvert = 1;
    }
    else
    {
        if (opts.verbose)
            fprintf(stderr, "INFO: Filelist Mode\n");
        if ((FileList = fopen(inputFile, "rb")) == NULL)
        {
            fprintf(stderr, "Error: Can't open file list %s\n", inputFile);
            // Cleanup falls bereits Single-File allokiert wurde
            if (iNumFilesToConvert > 0 && sCurrentFilename[0] != NULL)
            {
                free(sCurrentFilename[0]);
                sCurrentFilename[0] = NULL;
            }
            return 1;
        }

        while (!feof(FileList) && f < _MAX_FILES - 1)
        {
            if (fgets(sBuffer, sizeof(sBuffer), FileList) != NULL)
            {
                // Remove newline and check for empty lines
                size_t len = strlen(sBuffer);
                if (len > 0 && sBuffer[len - 1] == '\n')
                {
                    sBuffer[len - 1] = '\0';
                    len--;
                }

                // Skip empty lines
                if (len > 0)
                {
                    sCurrentFilename[f] = (char *)malloc(len + 1);
                    if (sCurrentFilename[f] == NULL)
                    {
                        fprintf(stderr, "Error: Can't allocate memory for filename %d\n", f);
                        // Cleanup already allocated filenames
                        for (int cleanup = 0; cleanup < f; cleanup++)
                        {
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

        if (f >= _MAX_FILES - 1)
        {
            fprintf(stderr, "WARNING: Maximum number of files (%d) reached. Some files may be skipped.\n", _MAX_FILES - 1);
        }

        fclose(FileList);
        iNumFilesToConvert = f;
    }

    fprintf(stderr, "INFO: Number of files to convert: %d\n", iNumFilesToConvert);

    if ((fi = (FileInfo *)malloc(iNumFilesToConvert * sizeof(struct tag_FileInfoSRTM))) == NULL)
    {
        fprintf(stderr, "Error: Can't allocate FileInfo array\n");
        // Cleanup already allocated filenames
        for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
        {
            if (sCurrentFilename[cleanup] != NULL)
            {
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

        memset(&fi[i], 0, sizeof(FileInfo));
        strcpy(fi[i].szFilename, sCurrentFilename[i]);
        fprintf(stderr, "INFO: Pre-Processing: %s ", fi[i].szFilename);
        fi[i].iMinElevation = 0;
        fi[i].iMaxElevation = 0;

        if ((InFile = fopen(fi[i].szFilename, "rb")) == NULL)
        {
            fprintf(stderr, "Error: Can't open input file %s\n", fi[i].szFilename);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
            {
                if (sCurrentFilename[cleanup] != NULL)
                {
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

        if (fi[i].ulFilesize == SRTM3_SIZE)
        {
            fi[i].srtmType = SRTM3; // Pro-Datei statt global
            fi[i].iWidth = 1201;
            fi[i].iHeight = 1201;
        }
        else if (fi[i].ulFilesize == SRTM1_SIZE)
        {
            fi[i].srtmType = SRTM1; // Per-file instead of global
            fi[i].iWidth = 3601;
            fi[i].iHeight = 3601;
        }
        else
        {
            // Custom size parsing via filename with fallback
            fi[i].iWidth = 0;
            fi[i].iHeight = 0;

            // Safe parsing only for sufficiently long filename
            size_t nameLen = strlen(fi[i].szFilename);
            if (nameLen >= 15)
            { // At least "N00E000_1234x5678.hgt"
                memcpy(sTmp, &sCurrentFilename[i][5], 4);
                sTmp[4] = '\0';
                int parsedWidth = atoi(sTmp);

                memcpy(sTmp, &sCurrentFilename[i][10], 4);
                sTmp[4] = '\0';
                int parsedHeight = atoi(sTmp);

                // Plausibility check: dimensions must be reasonable
                if (parsedWidth > 0 && parsedWidth <= 65536 &&
                    parsedHeight > 0 && parsedHeight <= 65536)
                {
                    fi[i].iWidth = parsedWidth;
                    fi[i].iHeight = parsedHeight;
                    fi[i].srtmType = fi[i].iWidth * fi[i].iHeight;
                }
            }

            // Fallback: Mark as UNKNOWN for invalid parsing
            if (fi[i].iWidth == 0 || fi[i].iHeight == 0)
            {
                fi[i].srtmType = SRTM_UNKNOWN;
                if (opts.verbose)
                {
                    fprintf(stderr, "WARNING: Could not parse dimensions from filename %s\n", fi[i].szFilename);
                }
            }
            else
            {
                // Additional validation: file size must match dimensions (overflow-protected)
                size_t expectedPixels, expectedSize;
                if (!safe_multiply_size_t(fi[i].iWidth, fi[i].iHeight, &expectedPixels) ||
                    !safe_multiply_size_t(expectedPixels, 2, &expectedSize))
                {
                    fprintf(stderr, "ERROR: Dimension overflow for %s: %zu×%zu pixels\n",
                            fi[i].szFilename, fi[i].iWidth, fi[i].iHeight);
                    fi[i].srtmType = SRTM_UNKNOWN; // Mark as invalid
                }
                else if (fi[i].ulFilesize != expectedSize)
                {
                    fprintf(stderr, "ERROR: Filesize mismatch for %s: expected %zu bytes (%zu×%zu), got %zu bytes\n",
                            fi[i].szFilename, expectedSize, fi[i].iWidth, fi[i].iHeight, fi[i].ulFilesize);
                    fi[i].srtmType = SRTM_UNKNOWN; // Mark as invalid
                }
            }
        }

        if (fi[i].srtmType == SRTM_UNKNOWN)
        { // Per-file check
            fprintf(stderr, "Error: %s has an unknown SRTM type\n", fi[i].szFilename);
            fclose(InFile);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
            {
                if (sCurrentFilename[cleanup] != NULL)
                {
                    free(sCurrentFilename[cleanup]);
                    sCurrentFilename[cleanup] = NULL;
                }
            }
            free(fi);
            fi = NULL;
            return 1;
        }

        if ((iElevationData = (short *)malloc(fi[i].ulFilesize)) == NULL)
        {
            fprintf(stderr, "Error: Can't allocate memory to load %s\n", fi[i].szFilename);
            fclose(InFile);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
            {
                if (sCurrentFilename[cleanup] != NULL)
                {
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
            for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
            {
                if (sCurrentFilename[cleanup] != NULL)
                {
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
        fi[i].noDataCount = 0; // Initialize NoData counter

        for (unsigned long j = 0; j < fi[i].ulFilesize / 2; j++)
        {
            // NoData handling
            iElevationData[j] = processElevationValue(iElevationData[j], fi[i].srtmType, &fi[i].noDataCount);

            // Min/Max only for valid values (not for NoData replacement values)
            if (iElevationData[j] != NODATA_REPLACEMENT)
            {
                if (iElevationData[j] < iCurrentMinElevation)
                    iCurrentMinElevation = iElevationData[j];
                if (iElevationData[j] > iCurrentMaxElevation)
                    iCurrentMaxElevation = iElevationData[j];
            }
        }

        if (iCurrentMinElevation < iOverallMinElevation)
            iOverallMinElevation = iCurrentMinElevation;
        if (iCurrentMaxElevation > iOverallMaxElevation)
            iOverallMaxElevation = iCurrentMaxElevation;

        // Output NoData statistics - avoid division by 0
        if (fi[i].noDataCount > 0)
        {
            unsigned long totalPixels = fi[i].ulFilesize / 2;
            float noDataPercent = 0.0f;
            if (totalPixels > 0)
            {
                noDataPercent = (float)fi[i].noDataCount / totalPixels * 100.0f;
            }
            fprintf(stderr, "- MIN=%4d MAX=%4d, NoData=%d (%.1f%%)\n",
                    iCurrentMinElevation, iCurrentMaxElevation, fi[i].noDataCount, noDataPercent);
        }
        else
        {
            fprintf(stderr, "- MIN=%4d MAX=%4d\n", iCurrentMinElevation, iCurrentMaxElevation);
        }

        free(iElevationData);
        iElevationData = NULL;
        fclose(InFile);
    }

    // PARALLELIZED BATCH PROCESSING
    int processingResult = processFilesParallel(fi, iNumFilesToConvert, opts);

    if (processingResult != 0)
    {
        // Cleanup on error
        for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
        {
            if (sCurrentFilename[cleanup] != NULL)
            {
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

    for (int cleanup = 0; cleanup < iNumFilesToConvert; cleanup++)
    {
        if (sCurrentFilename[cleanup] != NULL)
        {
            free(sCurrentFilename[cleanup]);
        }
    }

    fprintf(stderr, "Info: Done\n");
}

void WritePNG(const char *szFilename, size_t _iWidth, size_t _iHeight)
{
    WritePNGWithData(szFilename, _iWidth, _iHeight, PixelData);
}

static void WritePNGWithData(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGB *pixelData)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_RGB;
    image.width = _iWidth;
    image.height = _iHeight;

    fprintf(stderr, "Info: Writing %s\n", szFilename);
    if (!png_image_write_to_file(&image, szFilename, 0 /*convert_to_8bit*/, (png_bytep)pixelData, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
    }

    png_image_free(&image);
}

static void WritePNGWithAlpha(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGBA *pixelData)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_RGBA; // RGBA mit Alpha-Kanal
    image.width = _iWidth;
    image.height = _iHeight;

    fprintf(stderr, "Info: Writing RGBA %s\n", szFilename);
    if (!png_image_write_to_file(&image, szFilename, 0 /*convert_to_8bit*/, (png_bytep)pixelData, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
    }

    png_image_free(&image);
}

static void WritePNG16BitGrayscale(const char *szFilename, size_t _iWidth, size_t _iHeight, const uint16_t *pixelData)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_LINEAR_Y; // 16-bit Grayscale
    image.width = _iWidth;
    image.height = _iHeight;

    fprintf(stderr, "Info: Writing 16-bit %s\n", szFilename);
    if (!png_image_write_to_file(&image, szFilename, 0 /*convert_to_8bit*/, (png_bytep)pixelData, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
    }

    png_image_free(&image);
}

static void WritePNG16BitWithAlpha(const char *szFilename, size_t _iWidth, size_t _iHeight, const RGBA16 *pixelData)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_LINEAR_Y_ALPHA; // 16-bit Grayscale + Alpha
    image.width = _iWidth;
    image.height = _iHeight;

    fprintf(stderr, "Info: Writing 16-bit RGBA %s\n", szFilename);
    if (!png_image_write_to_file(&image, szFilename, 0 /*convert_to_8bit*/, (png_bytep)pixelData, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", szFilename, image.message);
    }

    png_image_free(&image);
}

// PARALLELIZATION - Worker thread function
static void *processFileWorker(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
    struct tag_FileInfoSRTM *fi = &data->fileInfo[data->fileIndex];
    ProgramOptions *opts = data->opts;

    FILE *InFile = NULL;
    short *iElevationData = NULL;
    RGB *PixelData = NULL;
    RGBA *PixelDataRGBA = NULL; // New RGBA data for alpha support
    int iNumIntRead;
    char OutputHeightmapFile[256];

    // Thread-safe progress indication
    pthread_mutex_lock(data->outputMutex);
    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Processing file %d/%d: %s\n",
                data->fileIndex + 1, data->totalFiles, fi->szFilename);
    }
    pthread_mutex_unlock(data->outputMutex);

    // Open file
    if ((InFile = fopen(fi->szFilename, "rb")) == NULL)
    {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Can't open input file %s\n", fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        *(data->globalResult) = 1;
        return NULL; // No memory to leak here
    }

    // Allocate memory for elevation data
    if ((iElevationData = (short *)malloc(fi->ulFilesize)) == NULL)
    {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Can't allocate elevation data block %s\n", fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        fclose(InFile);
        *(data->globalResult) = 1;
        return NULL; // File was closed, no memory allocated
    }

    // Read elevation data
    if ((iNumIntRead = fread(iElevationData, 1, fi->ulFilesize, InFile)) != (int)fi->ulFilesize)
    {
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
    if (opts->enableDetail)
    {
        // NoData handling for worker thread
        int threadNoDataCount = 0;
        for (unsigned long pre = 0; pre < (fi->ulFilesize) / 2; pre++)
        {
            iElevationData[pre] = processElevationValue(iElevationData[pre], fi->srtmType, &threadNoDataCount);
        }

        // Update NoData statistics thread-safely
        pthread_mutex_lock(data->outputMutex);
        fi->noDataCount += threadNoDataCount;
        if (opts->verbose && threadNoDataCount > 0)
        {
            fprintf(stderr, "INFO: Found %d NoData values in %s\n", threadNoDataCount, fi->szFilename);
        }
        pthread_mutex_unlock(data->outputMutex);

        // Add procedural detail
        short int *detailedData = AddProceduralDetail(iElevationData, fi->iWidth, fi->iHeight,
                                                      opts->scaleFactor, opts->detailIntensity, opts->noiseSeed, fi->srtmType);

        if (detailedData)
        {
            free(iElevationData);
            iElevationData = detailedData;
            fi->iWidth *= opts->scaleFactor;
            fi->iHeight *= opts->scaleFactor;

            // Safe calculation of new file size
            size_t newPixels, newFilesize;
            if (!safe_multiply_size_t(fi->iWidth, fi->iHeight, &newPixels) ||
                !safe_multiply_size_t(newPixels, sizeof(short int), &newFilesize))
            {
                pthread_mutex_lock(data->outputMutex);
                fprintf(stderr, "ERROR: File size overflow after enhancement: %zu×%zu for %s\n",
                        fi->iWidth, fi->iHeight, fi->szFilename);
                pthread_mutex_unlock(data->outputMutex);
                free(iElevationData);
                iElevationData = NULL;
                *(data->globalResult) = 1;
                return NULL;
            }
            fi->ulFilesize = newFilesize;

            pthread_mutex_lock(data->outputMutex);
            if (opts->verbose)
            {
                fprintf(stderr, "INFO: Enhanced resolution: %zu×%zu pixels for %s\n",
                        fi->iWidth, fi->iHeight, fi->szFilename);
            }
            pthread_mutex_unlock(data->outputMutex);
        }
        else
        {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "WARNING: Could not add procedural detail to %s, using original data\n",
                    fi->szFilename);
            pthread_mutex_unlock(data->outputMutex);
        }
    }

    // Allocate memory for pixel data - depending on output format
    size_t pixelCount;
    if (!safe_multiply_size_t(fi->iWidth, fi->iHeight, &pixelCount))
    {
        pthread_mutex_lock(data->outputMutex);
        fprintf(stderr, "Error: Pixel count overflow at %zu×%zu for %s\n",
                fi->iWidth, fi->iHeight, fi->szFilename);
        pthread_mutex_unlock(data->outputMutex);
        free(iElevationData);
        iElevationData = NULL;
        *(data->globalResult) = 1;
        return NULL;
    }

    unsigned short *PixelData16 = NULL;
    RGBA16 *PixelData16Alpha = NULL;

    if (opts->output16bit && opts->alphaNoData)
    {
        // 16-bit grayscale + alpha for transparent NoData
        PixelData16Alpha = (RGBA16 *)safe_malloc_pixels(fi->iWidth, fi->iHeight, sizeof(RGBA16));
        if (PixelData16Alpha == NULL)
        {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate 16-bit RGBA pixel data block for %s (%zu×%zu pixels)\n",
                    fi->szFilename, fi->iWidth, fi->iHeight);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    }
    else if (opts->output16bit)
    {
        // 16-bit grayscale data with overflow-protected allocation
        PixelData16 = (unsigned short *)safe_malloc_pixels(fi->iWidth, fi->iHeight, sizeof(unsigned short));
        if (PixelData16 == NULL)
        {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate 16-bit pixel data block for %s (%zu×%zu pixels)\n",
                    fi->szFilename, fi->iWidth, fi->iHeight);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    }
    else if (opts->alphaNoData)
    {
        // 8-bit RGBA data for alpha support with overflow-protected allocation
        PixelDataRGBA = (RGBA *)safe_malloc_pixels(fi->iWidth, fi->iHeight, sizeof(struct tag_RGBA));
        if (PixelDataRGBA == NULL)
        {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate RGBA pixel data block for %s (%zu×%zu pixels)\n",
                    fi->szFilename, fi->iWidth, fi->iHeight);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    }
    else
    {
        // 8-bit RGB data with overflow-protected allocation
        PixelData = (RGB *)safe_malloc_pixels(fi->iWidth, fi->iHeight, sizeof(struct tag_RGB));
        if (PixelData == NULL)
        {
            pthread_mutex_lock(data->outputMutex);
            fprintf(stderr, "Error: Can't allocate RGB pixel data block for %s (%zu×%zu pixels)\n",
                    fi->szFilename, fi->iWidth, fi->iHeight);
            pthread_mutex_unlock(data->outputMutex);
            free(iElevationData);
            iElevationData = NULL;
            *(data->globalResult) = 1;
            return NULL;
        }
    }

    // Calculate effective min/max heights (custom range or auto-detection)
    int effectiveMinHeight = (opts->minHeight != -1) ? opts->minHeight : iOverallMinElevation;
    int effectiveMaxHeight = (opts->maxHeight != -1) ? opts->maxHeight : iOverallMaxElevation;

    // Safety check: Min < Max
    if (effectiveMinHeight >= effectiveMaxHeight)
    {
        effectiveMinHeight = iOverallMinElevation;
        effectiveMaxHeight = iOverallMaxElevation;
    }

    // Convert elevation to pixels - depending on output format
    // OpenMP: SIMD vectorization for optimal performance with large images
    int totalNoDataCount = 0; // Thread-safe aggregation for NoData counting
#pragma omp parallel for simd aligned(iElevationData : 32) reduction(+ : totalNoDataCount) if (pixelCount > 10000)
    for (unsigned long m = 0; m < pixelCount; m++)
    {
        if (!opts->enableDetail)
        {
            // Correct NoData handling without detail generation
            int threadNoDataCount = 0;
            iElevationData[m] = processElevationValue(iElevationData[m], fi->srtmType, &threadNoDataCount);
            totalNoDataCount += threadNoDataCount; // Thread-safe reduction
        }

        // Check for NoData pixels (for alpha transparency)
        int isNoDataPixel = (iElevationData[m] == NODATA_REPLACEMENT);

        // Normalization to 0.0-1.0 with custom range
        float normalizedValue = 0.5f; // Fallback
        if (effectiveMaxHeight > effectiveMinHeight)
        {
            // Clamp elevation to effective range
            int clampedElevation = iElevationData[m];
            if (clampedElevation < effectiveMinHeight)
                clampedElevation = effectiveMinHeight;
            if (clampedElevation > effectiveMaxHeight)
                clampedElevation = effectiveMaxHeight;

            normalizedValue = (float)(clampedElevation - effectiveMinHeight) /
                              (float)(effectiveMaxHeight - effectiveMinHeight);
        }

        // Apply curve mapping
        float curvedValue = ApplyCurveMapping(normalizedValue, opts->curveType, opts->gamma);

        if (opts->output16bit && opts->alphaNoData)
        {
            // 16-bit grayscale + alpha: make NoData pixels transparent
            uint16_t pixelValue = (uint16_t)(curvedValue * 65535.0f);
            PixelData16Alpha[m].y = pixelValue;
            PixelData16Alpha[m].a = isNoDataPixel ? 0 : 65535; // Transparent for NoData, opaque for valid data
        }
        else if (opts->output16bit)
        {
            // 16-bit grayscale: use full 16-bit range (0-65535)
            PixelData16[m] = (unsigned short)(curvedValue * 65535.0f);
        }
        else if (opts->alphaNoData)
        {
            // 8-bit RGBA: with alpha channel for NoData transparency
            unsigned char pixelValue = (unsigned char)(curvedValue * 255.0f);
            PixelDataRGBA[m].r = pixelValue;
            PixelDataRGBA[m].g = pixelValue;
            PixelDataRGBA[m].b = pixelValue;
            PixelDataRGBA[m].a = isNoDataPixel ? 0 : 255; // Transparent for NoData, opaque for valid data
        }
        else
        {
            // 8-bit RGB: curve-corrected calculation
            unsigned char pixelValue = (unsigned char)(curvedValue * 255.0f);
            PixelData[m].r = PixelData[m].g = PixelData[m].b = pixelValue;
        }
    }

    // Update NoData count after parallel processing
    fi->noDataCount += totalNoDataCount;

    // Generate PNG output filename (filename only, not full path)
    generateOutputFilename(fi->szFilename, OutputHeightmapFile);

    // Write PNG (thread-safe with local PixelData)
    pthread_mutex_lock(data->outputMutex);
    if (opts->output16bit && opts->alphaNoData)
    {
        WritePNG16BitWithAlpha(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelData16Alpha);
    }
    else if (opts->output16bit)
    {
        WritePNG16BitGrayscale(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelData16);
    }
    else if (opts->alphaNoData)
    {
        WritePNGWithAlpha(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelDataRGBA);
    }
    else
    {
        WritePNGWithData(OutputHeightmapFile, fi->iWidth, fi->iHeight, PixelData);
    }

    // Write metadata file (if enabled)
    writeMetadataFile(OutputHeightmapFile, opts, fi, effectiveMinHeight, effectiveMaxHeight);

    // Generate vegetation mask (if enabled)
    if (opts->vegetation.enabled)
    {
        // We need to reload the elevation data for vegetation mask since we may have modified it
        // Use the original elevation data stored in iElevationData before any processing
        generate_vegetation_mask(fi, iElevationData, opts);
    }

    pthread_mutex_unlock(data->outputMutex);

    // Cleanup for this thread
    free(iElevationData);
    iElevationData = NULL;
    if (opts->output16bit && opts->alphaNoData)
    {
        free(PixelData16Alpha);
        PixelData16Alpha = NULL;
    }
    else if (opts->output16bit)
    {
        free(PixelData16);
        PixelData16 = NULL;
    }
    else if (opts->alphaNoData)
    {
        free(PixelDataRGBA); // Free thread-local RGBA data
        PixelDataRGBA = NULL;
    }
    else
    {
        free(PixelData); // Free thread-local PixelData
        PixelData = NULL;
    }

    // Update progress counter
    pthread_mutex_lock(data->outputMutex);
    (*(data->filesProcessed))++;
    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Completed %d/%d files\n", *(data->filesProcessed), data->totalFiles);
    }
    pthread_mutex_unlock(data->outputMutex);

    return NULL;
}

// PARALLELIZED BATCH PROCESSING
static int processFilesParallel(struct tag_FileInfoSRTM *fi, int iNumFilesToConvert, ProgramOptions opts)
{
    if (iNumFilesToConvert <= 1 || opts.numThreads <= 1)
    {
        // Fallback to sequential processing for few files or 1 thread
        return processFilesSequential(fi, iNumFilesToConvert, opts);
    }

    pthread_t *threads;
    ThreadData *threadData;
    pthread_mutex_t outputMutex = PTHREAD_MUTEX_INITIALIZER;
    int globalResult = 0;
    int filesProcessed = 0;

    // Adapt number of threads to available files
    int actualThreads = (opts.numThreads > iNumFilesToConvert) ? iNumFilesToConvert : opts.numThreads;

    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Starting parallel processing with %d threads for %d files\n",
                actualThreads, iNumFilesToConvert);
    }

    // Allocate thread arrays
    threads = (pthread_t *)malloc(actualThreads * sizeof(pthread_t));
    threadData = (ThreadData *)malloc(iNumFilesToConvert * sizeof(ThreadData));

    if (!threads || !threadData)
    {
        fprintf(stderr, "Error: Cannot allocate memory for threading\n");
        if (threads)
            free(threads);
        if (threadData)
            free(threadData);
        return 1;
    }

    // Prepare thread data for all files
    for (int i = 0; i < iNumFilesToConvert; i++)
    {
        threadData[i].fileIndex = i;
        threadData[i].fileInfo = fi;
        threadData[i].opts = &opts;
        threadData[i].globalResult = &globalResult;
        threadData[i].outputMutex = &outputMutex;
        threadData[i].filesProcessed = &filesProcessed;
        threadData[i].totalFiles = iNumFilesToConvert;
    }

    // Start threads in batches (thread pool simulation)
    int fileIndex = 0;
    while (fileIndex < iNumFilesToConvert && globalResult == 0)
    {
        int threadsToStart = (iNumFilesToConvert - fileIndex > actualThreads) ? actualThreads : (iNumFilesToConvert - fileIndex);

        // Start threads
        for (int t = 0; t < threadsToStart; t++)
        {
            if (pthread_create(&threads[t], NULL, processFileWorker, &threadData[fileIndex + t]) != 0)
            {
                fprintf(stderr, "Error: Cannot create thread %d\n", t);
                globalResult = 1;
                break;
            }
        }

        // Wait for all started threads
        for (int t = 0; t < threadsToStart; t++)
        {
            pthread_join(threads[t], NULL);
        }

        fileIndex += threadsToStart;
    }

    // Cleanup
    free(threads);
    free(threadData);
    pthread_mutex_destroy(&outputMutex);

    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Parallel processing completed. Result: %s\n",
                globalResult == 0 ? "SUCCESS" : "ERROR");
    }

    return globalResult;
}

// SEQUENTIAL PROCESSING (Fallback)
static int processFilesSequential(struct tag_FileInfoSRTM *fi, int iNumFilesToConvert, ProgramOptions opts)
{
    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Using sequential processing for %d files\n", iNumFilesToConvert);
    }

    // The original loop as a separate function
    for (int k = 0; k < iNumFilesToConvert; k++)
    {
        // Use worker logic, but without threading
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

        if (result != 0)
        {
            pthread_mutex_destroy(&dummyMutex);
            return result;
        }
        pthread_mutex_destroy(&dummyMutex);
    }

    return 0;
}

// PROCEDURAL DETAIL GENERATION

// Simple noise function (Perlin-like)
static float SimpleNoise(int x, int y, int seed)
{
    int n = x + y * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

// Bilinear interpolation for noise sampling (eliminates quantization)
static float BilinearNoiseInterpolate(float x, float y, int seed)
{
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    float fx = x - x0;
    float fy = y - y0;

    float v00 = SimpleNoise(x0, y0, seed);
    float v10 = SimpleNoise(x0 + 1, y0, seed);
    float v01 = SimpleNoise(x0, y0 + 1, seed);
    float v11 = SimpleNoise(x0 + 1, y0 + 1, seed);

    float v0 = v00 * (1.0f - fx) + v10 * fx;
    float v1 = v01 * (1.0f - fx) + v11 * fx;

    return v0 * (1.0f - fy) + v1 * fy;
}

// Fractal noise for different detail levels (with sub-pixel precision)
static float FractalNoise(float x, float y, int octaves, float persistence, float scale, int seed)
{
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++)
    {
        // Bilinear interpolation instead of integer casting eliminates quantization
        total += BilinearNoiseInterpolate(x * frequency, y * frequency, seed + i) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

// Bilinear interpolation
static short int BilinearInterpolate(const short int *data, int width, int height, float x, float y)
{
    int x1 = (int)x;
    int y1 = (int)y;
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    // Check boundaries
    if (x2 >= width)
        x2 = width - 1;
    if (y2 >= height)
        y2 = height - 1;
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;

    float fx = x - x1;
    float fy = y - y1;

    short int p1 = data[y1 * width + x1]; // Top-left
    short int p2 = data[y1 * width + x2]; // Top-right
    short int p3 = data[y2 * width + x1]; // Bottom-left
    short int p4 = data[y2 * width + x2]; // Bottom-right

    // Interpolation in X direction
    float i1 = p1 * (1 - fx) + p2 * fx;
    float i2 = p3 * (1 - fx) + p4 * fx;

    // Interpolation in Y direction
    return (short int)(i1 * (1 - fy) + i2 * fy);
}

// Calculate local slope
// Calculate pixel distance based on SRTM type
static float GetPixelDistance(int srtmType)
{
    if (srtmType == SRTM3)
    {
        return 60.0f; // 2 * 30m for SRTM-1 (1201x1201)
    }
    else if (srtmType == SRTM1)
    {
        return 180.0f; // 2 * 90m for SRTM-3 (3601x3601)
    }
    else
    {
        // Custom size: assume medium resolution
        return 60.0f; // Fallback to SRTM-1 resolution
    }
}

static float CalculateLocalSlope(const short int *data, int width, int height, float x, float y, int srtmType)
{
    int ix = (int)x;
    int iy = (int)y;

    if (ix <= 0 || ix >= width - 1 || iy <= 0 || iy >= height - 1)
        return 0.0f;

    short int left = data[iy * width + (ix - 1)];
    short int right = data[iy * width + (ix + 1)];
    short int top = data[(iy - 1) * width + ix];
    short int bottom = data[(iy + 1) * width + ix];

    float pixelDistance = GetPixelDistance(srtmType);
    float dx = (right - left) / pixelDistance;
    float dy = (bottom - top) / pixelDistance;

    float slope = sqrt(dx * dx + dy * dy) / 100.0f; // Normalized to 0-1
    if (slope > 1.0f)
        slope = 1.0f;
    return slope;
}

// Terrain-type-specific factors
static float GetHeightTypeFactor(short int height)
{
    if (height < 100)
        return 0.5f; // Coastal plains - little detail
    else if (height < 500)
        return 0.7f; // Hills - medium detail
    else if (height < 1500)
        return 1.0f; // Mountains - full detail
    else if (height < 3000)
        return 0.8f; // High mountains - less detail (rock)
    else
        return 0.3f; // Extreme heights - minimal detail
}

// Curve mapping for various elevation-to-pixel transformations
static float ApplyCurveMapping(float normalizedValue, CurveType curveType, float gamma)
{
    // Input clipping
    if (normalizedValue < 0.0f)
        normalizedValue = 0.0f;
    if (normalizedValue > 1.0f)
        normalizedValue = 1.0f;

    float result = normalizedValue;

    switch (curveType)
    {
    case CURVE_LINEAR:
        // Linear curve - no change
        result = normalizedValue;
        break;

    case CURVE_LOG:
        // Logarithmic curve - emphasizes low values
        if (normalizedValue > 0.0f)
        {
            result = logf(1.0f + normalizedValue * 9.0f) / logf(10.0f); // log10(1 + x*9)
        }
        else
        {
            result = 0.0f;
        }
        break;
    }

    // Apply gamma correction
    if (gamma != 1.0f)
    {
        result = powf(result, 1.0f / gamma);
    }

    // Output clipping
    if (result < 0.0f)
        result = 0.0f;
    if (result > 1.0f)
        result = 1.0f;

    return result;
}

// Extracts geographic bounds from HGT filenames (e.g. "N49E004.hgt" -> lat 49-50, lon 4-5)
static int extractGeoBounds(const char *filename, float *south, float *north, float *west, float *east)
{
    // Extract filename without path
    const char *basename = strrchr(filename, '/');
    if (basename)
        basename++;
    else
        basename = filename;

    // Expect format: [N|S]DDEEEE[E|W].hgt (e.g. N49E004.hgt)
    if (strlen(basename) < 7)
        return 0;

    // Parse latitude
    char latHemisphere = basename[0];
    if (latHemisphere != 'N' && latHemisphere != 'S')
        return 0;

    int latDegrees = (basename[1] - '0') * 10 + (basename[2] - '0');
    if (latDegrees < 0 || latDegrees > 90)
        return 0;

    *south = (latHemisphere == 'N') ? (float)latDegrees : -(float)(latDegrees + 1);
    *north = (latHemisphere == 'N') ? (float)(latDegrees + 1) : -(float)latDegrees;

    // Parse longitude
    char lonHemisphere = basename[3];
    if (lonHemisphere != 'E' && lonHemisphere != 'W')
        return 0;

    int lonDegrees = (basename[4] - '0') * 100 + (basename[5] - '0') * 10 + (basename[6] - '0');
    if (lonDegrees < 0 || lonDegrees > 180)
        return 0;

    *west = (lonHemisphere == 'E') ? (float)lonDegrees : -(float)(lonDegrees + 1);
    *east = (lonHemisphere == 'E') ? (float)(lonDegrees + 1) : -(float)lonDegrees;

    return 1;
}

// Main function for Procedural Detail Generation
static short int *AddProceduralDetail(const short int *originalData, int originalWidth, int originalHeight,
                                      int scaleFactor, float detailIntensity, int seed, int srtmType)
{

    size_t newWidth, newHeight;

    // Safe calculation of new dimensions with overflow protection
    if (!safe_multiply_size_t((size_t)originalWidth, (size_t)scaleFactor, &newWidth) ||
        !safe_multiply_size_t((size_t)originalHeight, (size_t)scaleFactor, &newHeight))
    {
        fprintf(stderr, "ERROR: Dimension overflow in procedural detail generation: %d×%d scale %d\n",
                originalWidth, originalHeight, scaleFactor);
        return NULL;
    }

    // Safe memory allocation with overflow protection
    short int *detailedData = (short int *)safe_malloc_pixels(newWidth, newHeight, sizeof(short int));
    if (!detailedData)
    {
        fprintf(stderr, "ERROR: Cannot allocate memory for detailed heightmap (%zu×%zu pixels)\n",
                newWidth, newHeight);
        return NULL;
    }

    fprintf(stderr, "INFO: Generating %zu×%zu detailed heightmap (intensity: %.1f)\n",
            newWidth, newHeight, detailIntensity);

// OpenMP: Parallel processing + SIMD vectorization for optimal performance
// Distribute outer loop across multiple CPU cores
#pragma omp parallel for schedule(dynamic, 64) if (newHeight > 1000)
    for (size_t y = 0; y < newHeight; y++)
    {
        // Progress reporting (thread-safe, reduced frequency)
        if (y % (newHeight / 10) == 0)
        {
#pragma omp critical
            {
                fprintf(stderr, "INFO: Progress: %zu%%\n", (y * 100) / newHeight);
            }
        }

// Optimize inner loop for SIMD vectorization
#pragma omp simd aligned(detailedData : 32) safelen(16)
        for (size_t x = 0; x < newWidth; x++)
        {
            // Base height through bilinear interpolation
            float srcX = (float)x / scaleFactor;
            float srcY = (float)y / scaleFactor;

            if (srcX >= originalWidth - 1)
                srcX = originalWidth - 1.001f;
            if (srcY >= originalHeight - 1)
                srcY = originalHeight - 1.001f;

            short int baseHeight = BilinearInterpolate(originalData, originalWidth, originalHeight, srcX, srcY);

            // Add procedural detail
            // Different noise octaves for different detail sizes
            float detailNoise1 = FractalNoise(x * 0.005f, y * 0.005f, 3, 0.5f, 1.0f, seed);     // Large features
            float detailNoise2 = FractalNoise(x * 0.02f, y * 0.02f, 4, 0.6f, 1.0f, seed + 100); // Medium features
            float detailNoise3 = FractalNoise(x * 0.08f, y * 0.08f, 2, 0.4f, 1.0f, seed + 200); // Fine details

            float combinedNoise = detailNoise1 * 0.5f + detailNoise2 * 0.3f + detailNoise3 * 0.2f;

            // Height-dependent variation (flat areas = less detail, steep areas = more detail)
            float localSlope = CalculateLocalSlope(originalData, originalWidth, originalHeight, srcX, srcY, srtmType);
            float slopeMultiplier = 0.3f + (localSlope * 0.7f); // 0.3 to 1.0

            // Adjust detail based on terrain type
            float heightFactor = GetHeightTypeFactor(baseHeight);

            // Calculate final detail with sub-pixel precision
            float detailVariation = combinedNoise * detailIntensity * slopeMultiplier * heightFactor;

            // Maintain float precision for final height (against terracing)
            float finalHeight = (float)baseHeight + detailVariation;

            // Ensure values stay within valid range
            if (finalHeight < 0.0f)
                finalHeight = 0.0f;
            if (finalHeight > (float)_MAX_HEIGHT)
                finalHeight = (float)_MAX_HEIGHT;

            // Rounding instead of truncation for better sub-pixel distribution
            detailedData[y * newWidth + x] = (short int)(finalHeight + 0.5f);
        }
    }

    return detailedData;
}

// HELPER FUNCTIONS

// Extracts filename from path and generates PNG name in current directory
static void generateOutputFilename(const char *inputPath, char *outputPath)
{
    const char *filename = inputPath;
    const char *lastSlash = strrchr(inputPath, '/');

    // If a path separator was found, take the part after it
    if (lastSlash != NULL)
    {
        filename = lastSlash + 1;
    }

    // Copy the filename and replace the extension
    strcpy(outputPath, filename);

    // Find the last .hgt extension and replace it with .png
    char *extension = strrchr(outputPath, '.');
    if (extension != NULL && (strcmp(extension, ".hgt") == 0 || strcmp(extension, ".HGT") == 0))
    {
        strcpy(extension, ".png");
    }
    else
    {
        // If no .hgt extension was found, append .png
        strcat(outputPath, ".png");
    }
}

// NODATA HANDLING

// Checks if a raw value is a NoData value (after endian conversion)
static int isNoDataValue(int16_t value, int srtmType)
{
    // Only for SRTM data (30m/90m) NoData detection
    if (srtmType != SRTM3 && srtmType != SRTM1)
    {
        return 0;
    }

    // NoData value: -32768 (0x8000 in Big Endian)
    return (value == NODATA_VALUE_BE);
}

// Processes an elevation value: Network-to-Host Byte Order + NoData handling + Clamping
static int16_t processElevationValue(int16_t rawValue, int srtmType, int *noDataCount)
{
    // 1. Endian conversion for SRTM data (Big Endian -> Host Endian)
    int16_t hostValue = rawValue;
    if (srtmType == SRTM3 || srtmType == SRTM1)
    {
        hostValue = (int16_t)ntohs((uint16_t)rawValue); // Network (Big Endian) to Host
    }

    // 2. NoData detection after endian conversion
    if (isNoDataValue(hostValue, srtmType))
    {
        if (noDataCount != NULL)
        {
            (*noDataCount)++;
        }
        return NODATA_REPLACEMENT; // Replacement value for NoData
    }

    // 3. Clamping to valid elevation values
    if (hostValue < 0)
        hostValue = 0;
    if (hostValue > _MAX_HEIGHT)
        hostValue = _MAX_HEIGHT;

    return hostValue;
}

// PARAMETER MANAGEMENT FUNCTIONS

// Initialize default options
static void initDefaultOptions(ProgramOptions *opts)
{
    opts->scaleFactor = DEFAULT_SCALE_FACTOR;
    opts->detailIntensity = DEFAULT_DETAIL_INTENSITY;
    opts->noiseSeed = DEFAULT_NOISE_SEED;
    opts->enableDetail = ENABLE_PROCEDURAL_DETAIL;
    opts->verbose = 1; // Default: Verbose output
    opts->showHelp = 0;
    opts->showVersion = 0;
    opts->numThreads = DEFAULT_NUM_THREADS; // New option for thread count
    opts->output16bit = 0;                  // Default: 8-bit RGB output
    opts->gamma = 1.0f;                     // Default: No gamma correction
    opts->curveType = CURVE_LINEAR;         // Default: Linear curve
    opts->minHeight = -1;                   // Auto-detection
    opts->maxHeight = -1;                   // Auto-detection
    opts->metadataFormat = METADATA_NONE;   // Default: No metadata
    opts->alphaNoData = 0;                  // Default: No transparent NoData pixels

    // Initialize vegetation mask parameters (disabled by default)
    opts->vegetation.enabled = 0;
    opts->biome = BIOME_ALPINE; // Default biome

    // Initialize Alpine biome parameters (will be set if vegetation masks are enabled)
    initialize_alpine_biome(&opts->vegetation);
    opts->vegetation.enabled = 0; // Override to disabled by default
}

// Show help text
static void showHelp(const char *programName)
{
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
    printf("      --alpha-nodata       Generate RGBA PNG with transparent NoData pixels\n");
    printf("  -g, --gamma <f>          Gamma correction curve (default: 1.0, range: 0.1-10.0)\n");
    printf("  -c, --curve <type>       Mapping curve: linear|log (default: linear)\n");
    printf("  -m, --min-height <n>     Minimum elevation for mapping (default: auto)\n");
    printf("  -M, --max-height <n>     Maximum elevation for mapping (default: auto)\n");
    printf("  -x, --metadata <format>  Generate sidecar metadata: json|txt|none (default: none)\n");
    printf("                           Contains elevation range, pixel pitch, and geo coordinates\n");
    printf("  -V, --vegetation-mask    Generate vegetation density mask PNG (grayscale 0-255)\n");
    printf("  -B, --biome <type>       Select biome type: alpine|temperate|tropical|desert|arctic\n");
    printf("                           (default: alpine, only used with --vegetation-mask)\n");
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
    printf("  %s --alpha-nodata --metadata json terrain.hgt  # Transparent NoData with metadata\n", programName);
    printf("  %s --threads 4 --scale-factor 3 --detail-intensity 10  # Parallel + custom\n", programName);
    printf("  %s --vegetation-mask --biome alpine terrain.hgt  # Generate Alpine vegetation mask\n\n", programName);

    printf("OUTPUT:\n");
    printf("  Creates PNG files with same basename as input HGT files.\n");
    printf("  Example: N48E011.hgt → N48E011.png\n");
    printf("  With vegetation masks: N48E011.hgt → N48E011.png + N48E011_vegetation_alpine.png\n\n");
}

// Show version
static void showVersion(void)
{
    printf("hgt2png v1.1.0\n");
    printf("HGT to PNG Heightmap Converter with Procedural Detail Generation\n");
    printf("(C) 2025 Peter Ebel\n");
}

// Parse command line arguments
static int parseArguments(int argc, char *argv[], ProgramOptions *opts, char **inputFile)
{
    int c;
    static struct option long_options[] = {
        {"scale-factor", required_argument, 0, 's'},
        {"detail-intensity", required_argument, 0, 'i'},
        {"noise-seed", required_argument, 0, 'r'},
        {"threads", required_argument, 0, 't'},
        {"disable-detail", no_argument, 0, 'd'},
        {"quiet", no_argument, 0, 'q'},
        {"16bit", no_argument, 0, '6'},
        {"alpha-nodata", no_argument, 0, 'a'},
        {"gamma", required_argument, 0, 'g'},
        {"curve", required_argument, 0, 'c'},
        {"min-height", required_argument, 0, 'm'},
        {"max-height", required_argument, 0, 'M'},
        {"metadata", required_argument, 0, 'x'},
        {"vegetation-mask", no_argument, 0, 'V'},
        {"biome", required_argument, 0, 'B'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    while ((c = getopt_long(argc, argv, "s:i:r:t:dq6ag:c:m:M:x:VB:hv", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 's':
            opts->scaleFactor = atoi(optarg);
            if (opts->scaleFactor < 1 || opts->scaleFactor > 10)
            {
                fprintf(stderr, "Error: Scale factor must be between 1 and 10\n");
                return 1;
            }
            break;

        case 'i':
            opts->detailIntensity = atof(optarg);
            if (opts->detailIntensity < 0.0f || opts->detailIntensity > 100.0f)
            {
                fprintf(stderr, "Error: Detail intensity must be between 0.0 and 100.0\n");
                return 1;
            }
            break;

        case 'r':
            opts->noiseSeed = atoi(optarg);
            break;

        case 't':
            opts->numThreads = atoi(optarg);
            if (opts->numThreads < 1 || opts->numThreads > 16)
            {
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

        case 'a':
            opts->alphaNoData = 1;
            break;

        case 'g':
            opts->gamma = atof(optarg);
            if (opts->gamma <= 0.1f || opts->gamma > 10.0f)
            {
                fprintf(stderr, "Error: Gamma must be between 0.1 and 10.0\n");
                return 1;
            }
            break;

        case 'c':
            if (strcmp(optarg, "linear") == 0)
            {
                opts->curveType = CURVE_LINEAR;
            }
            else if (strcmp(optarg, "log") == 0)
            {
                opts->curveType = CURVE_LOG;
            }
            else
            {
                fprintf(stderr, "Error: Curve type must be 'linear' or 'log'\n");
                return 1;
            }
            break;

        case 'm':
            opts->minHeight = atoi(optarg);
            break;

        case 'M':
            opts->maxHeight = atoi(optarg);
            if (opts->minHeight != -1 && opts->maxHeight != -1 && opts->maxHeight <= opts->minHeight)
            {
                fprintf(stderr, "Error: max-height must be greater than min-height\n");
                return 1;
            }
            break;

        case 'x':
            if (strcmp(optarg, "json") == 0)
            {
                opts->metadataFormat = METADATA_JSON;
            }
            else if (strcmp(optarg, "txt") == 0)
            {
                opts->metadataFormat = METADATA_TXT;
            }
            else if (strcmp(optarg, "none") == 0)
            {
                opts->metadataFormat = METADATA_NONE;
            }
            else
            {
                fprintf(stderr, "Error: Metadata format must be 'json', 'txt', or 'none'\n");
                return 1;
            }
            break;

        case 'V':
            opts->vegetation.enabled = 1;
            break;

        case 'B':
            if (strcmp(optarg, "alpine") == 0)
            {
                opts->biome = BIOME_ALPINE;
            }
            else if (strcmp(optarg, "temperate") == 0)
            {
                opts->biome = BIOME_TEMPERATE;
            }
            else if (strcmp(optarg, "tropical") == 0)
            {
                opts->biome = BIOME_TROPICAL;
            }
            else if (strcmp(optarg, "desert") == 0)
            {
                opts->biome = BIOME_DESERT;
            }
            else if (strcmp(optarg, "arctic") == 0)
            {
                opts->biome = BIOME_ARCTIC;
            }
            else
            {
                fprintf(stderr, "Error: Biome must be 'alpine', 'temperate', 'tropical', 'desert', or 'arctic'\n");
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
    if (optind < argc)
    {
        *inputFile = argv[optind];
    }

    return 0;
}

// Writes metadata sidecar file for precise Blender scaling
static void writeMetadataFile(const char *pngFilename, const ProgramOptions *opts, const struct tag_FileInfoSRTM *fi,
                              int effectiveMinHeight, int effectiveMaxHeight)
{

    if (opts->metadataFormat == METADATA_NONE)
        return;

    // Generate metadata filename
    char metadataFilename[512];
    strcpy(metadataFilename, pngFilename);

    // Replace PNG extension
    char *dot = strrchr(metadataFilename, '.');
    if (dot)
    {
        if (opts->metadataFormat == METADATA_JSON)
        {
            strcpy(dot, ".json");
        }
        else
        {
            strcpy(dot, ".txt");
        }
    }
    else
    {
        if (opts->metadataFormat == METADATA_JSON)
        {
            strcat(metadataFilename, ".json");
        }
        else
        {
            strcat(metadataFilename, ".txt");
        }
    }

    // Extract geographic bounds
    float south, north, west, east;
    int hasGeoBounds = extractGeoBounds(fi->szFilename, &south, &north, &west, &east);

    // Calculate pixel pitch (meters per pixel)
    float pixelPitchMeters = 30.0f; // Default SRTM-1
    if (fi->srtmType == SRTM1)
    {
        pixelPitchMeters = 90.0f; // SRTM-3
    }
    else if (fi->srtmType == SRTM3)
    {
        pixelPitchMeters = 30.0f; // SRTM-1
    }

    // With procedural detail: divide pixel pitch by scale factor
    if (opts->enableDetail && opts->scaleFactor > 1)
    {
        pixelPitchMeters /= (float)opts->scaleFactor;
    }

    FILE *metadataFile = fopen(metadataFilename, "w");
    if (!metadataFile)
    {
        fprintf(stderr, "WARNING: Could not write metadata file %s\n", metadataFilename);
        return;
    }

    if (opts->metadataFormat == METADATA_JSON)
    {
        // JSON format for machine processing
        fprintf(metadataFile, "{\n");
        fprintf(metadataFile, "  \"source_file\": \"%s\",\n", fi->szFilename);
        fprintf(metadataFile, "  \"png_file\": \"%s\",\n", pngFilename);
        fprintf(metadataFile, "  \"dimensions\": {\n");
        fprintf(metadataFile, "    \"width\": %zu,\n", fi->iWidth);
        fprintf(metadataFile, "    \"height\": %zu\n", fi->iHeight);
        fprintf(metadataFile, "  },\n");
        fprintf(metadataFile, "  \"elevation\": {\n");
        fprintf(metadataFile, "    \"min_meters\": %d,\n", effectiveMinHeight);
        fprintf(metadataFile, "    \"max_meters\": %d,\n", effectiveMaxHeight);
        fprintf(metadataFile, "    \"range_meters\": %d,\n", effectiveMaxHeight - effectiveMinHeight);
        fprintf(metadataFile, "    \"original_min\": %d,\n", fi->iMinElevation);
        fprintf(metadataFile, "    \"original_max\": %d\n", fi->iMaxElevation);
        fprintf(metadataFile, "  },\n");
        fprintf(metadataFile, "  \"scaling\": {\n");
        fprintf(metadataFile, "    \"pixel_pitch_meters\": %.6f,\n", pixelPitchMeters);
        fprintf(metadataFile, "    \"scale_factor\": %d,\n", opts->scaleFactor);
        fprintf(metadataFile, "    \"world_size_meters\": {\n");
        fprintf(metadataFile, "      \"width\": %.2f,\n", fi->iWidth * pixelPitchMeters);
        fprintf(metadataFile, "      \"height\": %.2f\n", fi->iHeight * pixelPitchMeters);
        fprintf(metadataFile, "    }\n");
        fprintf(metadataFile, "  }");

        if (hasGeoBounds)
        {
            fprintf(metadataFile, ",\n");
            fprintf(metadataFile, "  \"geographic\": {\n");
            fprintf(metadataFile, "    \"bounds\": {\n");
            fprintf(metadataFile, "      \"south\": %.6f,\n", south);
            fprintf(metadataFile, "      \"north\": %.6f,\n", north);
            fprintf(metadataFile, "      \"west\": %.6f,\n", west);
            fprintf(metadataFile, "      \"east\": %.6f\n", east);
            fprintf(metadataFile, "    },\n");
            fprintf(metadataFile, "    \"center\": {\n");
            fprintf(metadataFile, "      \"latitude\": %.6f,\n", (south + north) / 2.0f);
            fprintf(metadataFile, "      \"longitude\": %.6f\n", (west + east) / 2.0f);
            fprintf(metadataFile, "    }\n");
            fprintf(metadataFile, "  }\n");
        }
        else
        {
            fprintf(metadataFile, "\n");
        }

        fprintf(metadataFile, "}\n");
    }
    else
    {
        // TXT format for human readability
        fprintf(metadataFile, "HGT2PNG Metadata\n");
        fprintf(metadataFile, "================\n\n");
        fprintf(metadataFile, "Source File: %s\n", fi->szFilename);
        fprintf(metadataFile, "PNG File: %s\n", pngFilename);
        fprintf(metadataFile, "\nImage Dimensions:\n");
        fprintf(metadataFile, "  Width:  %zu pixels\n", fi->iWidth);
        fprintf(metadataFile, "  Height: %zu pixels\n", fi->iHeight);
        fprintf(metadataFile, "\nElevation Data:\n");
        fprintf(metadataFile, "  Effective Range: %d - %d meters\n", effectiveMinHeight, effectiveMaxHeight);
        fprintf(metadataFile, "  Original Range:  %d - %d meters\n", fi->iMinElevation, fi->iMaxElevation);
        fprintf(metadataFile, "  Total Range:     %d meters\n", effectiveMaxHeight - effectiveMinHeight);
        fprintf(metadataFile, "\nBlender Scaling (Displacement Setup):\n");
        fprintf(metadataFile, "  Pixel Pitch: %.6f meters/pixel\n", pixelPitchMeters);
        fprintf(metadataFile, "  World Size:  %.2f x %.2f meters\n",
                fi->iWidth * pixelPitchMeters, fi->iHeight * pixelPitchMeters);
        fprintf(metadataFile, "  Scale Factor: %d\n", opts->scaleFactor);

        if (hasGeoBounds)
        {
            fprintf(metadataFile, "\nGeographic Coordinates:\n");
            fprintf(metadataFile, "  Bounds: %.6f°N to %.6f°N, %.6f°E to %.6f°E\n", south, north, west, east);
            fprintf(metadataFile, "  Center: %.6f°N, %.6f°E\n", (south + north) / 2.0f, (west + east) / 2.0f);
        }
    }

    fclose(metadataFile);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Wrote metadata file %s\n", metadataFilename);
    }
}

// ================================================================================
// VEGETATION MASK GENERATION SYSTEM
// Alpine Biome Implementation - Realistic European Alps Parameters
// ================================================================================

/**
 * Initialize Alpine biome parameters with realistic values
 * Based on European Alps vegetation zones and ecological data
 */
static void initialize_alpine_biome(VegetationParams *params)
{
    params->enabled = 1;
    params->minElevation = ALPINE_MIN_ELEVATION;     // 700m - montane forest zone
    params->maxElevation = ALPINE_MAX_ELEVATION;     // 2000m - above tree line
    params->maxSlope = ALPINE_MAX_SLOPE;             // 60° maximum for vegetation
    params->treeLine = ALPINE_TREE_LINE;             // 1800m - spruce/larch limit
    params->bushLine = ALPINE_BUSH_LINE;             // 2200m - dwarf pine/rhododendron
    params->grassLine = ALPINE_GRASS_LINE;           // 2500m - alpine meadows
    params->aspectModifier = ALPINE_ASPECT_MODIFIER; // 0.3 - moderate aspect influence
    params->drainageBonus = ALPINE_DRAINAGE_BONUS;   // 0.4 - valleys have more vegetation
}

/**
 * Calculate slope angle in degrees from elevation data
 * Uses Sobel operator for gradient calculation
 */
static float calculate_slope_angle(const int16_t *elevation_data, size_t width, size_t height,
                                   size_t x, size_t y, float pixel_pitch_meters)
{
    if (x == 0 || y == 0 || x >= width - 1 || y >= height - 1)
    {
        return 0.0f; // Boundary pixels have 0 slope
    }

    // Sobel operator for gradient calculation
    float dx = (elevation_data[(y - 1) * width + (x + 1)] + 2 * elevation_data[y * width + (x + 1)] + elevation_data[(y + 1) * width + (x + 1)] -
                elevation_data[(y - 1) * width + (x - 1)] - 2 * elevation_data[y * width + (x - 1)] - elevation_data[(y + 1) * width + (x - 1)]) /
               8.0f;

    float dy = (elevation_data[(y + 1) * width + (x - 1)] + 2 * elevation_data[(y + 1) * width + x] + elevation_data[(y + 1) * width + (x + 1)] -
                elevation_data[(y - 1) * width + (x - 1)] - 2 * elevation_data[(y - 1) * width + x] - elevation_data[(y - 1) * width + (x + 1)]) /
               8.0f;

    // Convert to slope angle in degrees
    float rise = sqrtf(dx * dx + dy * dy);
    float run = pixel_pitch_meters;
    float slope_radians = atanf(rise / run);

    return slope_radians * 180.0f / M_PI;
}

/**
 * Calculate aspect angle (0° = North, 90° = East, 180° = South, 270° = West)
 */
static float calculate_aspect_angle(const int16_t *elevation_data, size_t width, size_t height,
                                    size_t x, size_t y, float pixel_pitch_meters)
{
    (void)pixel_pitch_meters; // Parameter not used in this implementation
    if (x == 0 || y == 0 || x >= width - 1 || y >= height - 1)
    {
        return 0.0f; // Boundary pixels face North
    }

    // Calculate gradient components
    float dx = elevation_data[y * width + (x + 1)] - elevation_data[y * width + (x - 1)];
    float dy = elevation_data[(y + 1) * width + x] - elevation_data[(y - 1) * width + x];

    if (dx == 0.0f && dy == 0.0f)
    {
        return 0.0f; // Flat areas face North
    }

    // Calculate aspect (direction of steepest descent)
    float aspect_radians = atan2f(-dx, dy); // Note: -dx for proper orientation
    float aspect_degrees = aspect_radians * 180.0f / M_PI;

    // Normalize to 0-360°
    if (aspect_degrees < 0.0f)
    {
        aspect_degrees += 360.0f;
    }

    return aspect_degrees;
}

/**
 * Calculate drainage factor - higher in valleys, lower on ridges
 * Uses local elevation variance as proxy for drainage
 */
static float calculate_drainage_factor(const int16_t *elevation_data, size_t width, size_t height,
                                       size_t x, size_t y, int radius)
{
    if (radius <= 0)
        radius = 2;

    int16_t center_elevation = elevation_data[y * width + x];
    float sum_diff = 0.0f;
    int count = 0;

    // Sample neighborhood
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            int nx = (int)x + dx;
            int ny = (int)y + dy;

            if (nx >= 0 && ny >= 0 && nx < (int)width && ny < (int)height)
            {
                int16_t neighbor_elevation = elevation_data[ny * width + nx];
                sum_diff += (float)(center_elevation - neighbor_elevation);
                count++;
            }
        }
    }

    if (count == 0)
        return 0.5f;

    float avg_diff = sum_diff / (float)count;

    // Negative values = valley (center lower than surroundings) = high drainage
    // Positive values = ridge (center higher than surroundings) = low drainage
    float drainage_factor = 0.5f - (avg_diff / 200.0f); // Normalize around ±100m differences

    // Clamp to 0-1 range
    if (drainage_factor < 0.0f)
        drainage_factor = 0.0f;
    if (drainage_factor > 1.0f)
        drainage_factor = 1.0f;

    return drainage_factor;
}

/**
 * Calculate vegetation density for Alpine biome (0-255 grayscale)
 * Implements realistic Alpine vegetation distribution patterns
 */
static uint8_t calculate_vegetation_density_alpine(float elevation, float slope, float aspect,
                                                   float drainage, const VegetationParams *params)
{
    if (!params->enabled)
        return 0;

    // Base elevation factor
    float elevation_factor = 0.0f;

    if (elevation < params->minElevation)
    {
        elevation_factor = 0.0f; // Below vegetation zone
    }
    else if (elevation <= params->treeLine)
    {
        // Montane forest zone (700-1800m) - dense vegetation
        float range = params->treeLine - params->minElevation;
        elevation_factor = 1.0f - ((elevation - params->minElevation) / range) * 0.3f; // 70-100% density
    }
    else if (elevation <= params->bushLine)
    {
        // Subalpine zone (1800-2200m) - shrubs and dwarf trees
        float range = params->bushLine - params->treeLine;
        float position = (elevation - params->treeLine) / range;
        elevation_factor = 0.7f - position * 0.4f; // 30-70% density
    }
    else if (elevation <= params->grassLine)
    {
        // Alpine zone (2200-2500m) - alpine meadows and cushion plants
        float range = params->grassLine - params->bushLine;
        float position = (elevation - params->bushLine) / range;
        elevation_factor = 0.3f - position * 0.2f; // 10-30% density
    }
    else
    {
        elevation_factor = 0.0f; // Above vegetation limit (nival zone)
    }

    // Slope factor - steeper slopes have less vegetation
    float slope_factor = 1.0f;
    if (slope > params->maxSlope)
    {
        slope_factor = 0.0f; // Too steep for vegetation
    }
    else if (slope > 30.0f)
    {
        slope_factor = 1.0f - ((slope - 30.0f) / (params->maxSlope - 30.0f)) * 0.8f; // Gradual reduction
    }

    // Aspect factor - South faces are drier, North faces moister
    float aspect_factor = 1.0f;
    if (aspect >= 135.0f && aspect <= 225.0f)
    {
        // South-facing slopes (135°-225°) - drier
        aspect_factor = 1.0f - params->aspectModifier;
    }
    else if (aspect >= 315.0f || aspect <= 45.0f)
    {
        // North-facing slopes (315°-45°) - moister
        aspect_factor = 1.0f + params->aspectModifier;
    }

    // Drainage factor bonus for valleys
    float drainage_factor = 1.0f + (drainage * params->drainageBonus);

    // Combine all factors
    float final_density = elevation_factor * slope_factor * aspect_factor * drainage_factor;

    // Clamp to 0-1 range
    if (final_density < 0.0f)
        final_density = 0.0f;
    if (final_density > 1.0f)
        final_density = 1.0f;

    // Convert to 8-bit grayscale (0-255)
    return (uint8_t)(final_density * 255.0f);
}

/**
 * Generate vegetation mask PNG from heightmap data
 * Creates grayscale PNG where brightness represents vegetation density
 */
static int generate_vegetation_mask(const struct tag_FileInfoSRTM *fi, const int16_t *elevation_data,
                                    const ProgramOptions *opts)
{
    if (!opts->vegetation.enabled)
    {
        return 0; // Vegetation masks disabled
    }

    // Create vegetation mask filename
    char mask_filename[_MAX_PATH];
    char base_name[_MAX_PATH];

    // Extract base filename without extension
    const char *last_dot = strrchr(fi->szFilename, '.');
    if (last_dot)
    {
        size_t base_len = last_dot - fi->szFilename;
        strncpy(base_name, fi->szFilename, base_len);
        base_name[base_len] = '\0';
    }
    else
    {
        strcpy(base_name, fi->szFilename);
    }

    // Create mask filename with biome suffix
    const char *biome_names[] = {"alpine", "temperate", "tropical", "desert", "arctic"};
    snprintf(mask_filename, sizeof(mask_filename), "%s_vegetation_%s.png",
             base_name, biome_names[opts->biome]);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Generating vegetation mask: %s\n", mask_filename);
        fprintf(stderr, "INFO: Biome: %s, Elevation range: %.0f-%.0f meters\n",
                biome_names[opts->biome], opts->vegetation.minElevation, opts->vegetation.maxElevation);
    }

    // Determine pixel pitch for slope calculations
    float pixel_pitch_meters = 90.0f; // Default SRTM-3
    if (fi->srtmType == SRTM3)
    {
        pixel_pitch_meters = 30.0f; // SRTM-1
    }

    // Create PNG file
    FILE *fp = fopen(mask_filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "ERROR: Cannot create vegetation mask file %s\n", mask_filename);
        return -1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        fprintf(stderr, "ERROR: Cannot create PNG write structure\n");
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        fprintf(stderr, "ERROR: Cannot create PNG info structure\n");
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "ERROR: PNG write error\n");
        return -1;
    }

    png_init_io(png_ptr, fp);

    // Set PNG headers for grayscale
    png_set_IHDR(png_ptr, info_ptr, fi->iWidth, fi->iHeight, 8,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Allocate row buffer
    png_bytep row = (png_bytep)malloc(fi->iWidth * sizeof(png_byte));
    if (!row)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        fprintf(stderr, "ERROR: Cannot allocate PNG row buffer\n");
        return -1;
    }

    // Progress tracking
    size_t progress_step = fi->iHeight / 20; // Update every 5%
    if (progress_step == 0)
        progress_step = 1;

// Process each row
#pragma omp parallel for schedule(dynamic, 1) if (opts->numThreads > 1)
    for (size_t y = 0; y < fi->iHeight; y++)
    {
        // Progress indicator
        if (opts->verbose && (y % progress_step == 0))
        {
#pragma omp critical
            {
                fprintf(stderr, "\rProgress: %zu%% ", (y * 100) / fi->iHeight);
                fflush(stderr);
            }
        }

        // Process each pixel in row
        for (size_t x = 0; x < fi->iWidth; x++)
        {
            size_t index = y * fi->iWidth + x;
            int16_t elevation = elevation_data[index];

            uint8_t vegetation_density = 0;

            // Skip NoData pixels
            if (elevation != NODATA_REPLACEMENT)
            {
                // Calculate terrain analysis factors
                float slope = calculate_slope_angle(elevation_data, fi->iWidth, fi->iHeight,
                                                    x, y, pixel_pitch_meters);
                float aspect = calculate_aspect_angle(elevation_data, fi->iWidth, fi->iHeight,
                                                      x, y, pixel_pitch_meters);
                float drainage = calculate_drainage_factor(elevation_data, fi->iWidth, fi->iHeight,
                                                           x, y, 2);

                // Calculate vegetation density based on biome
                switch (opts->biome)
                {
                case BIOME_ALPINE:
                    vegetation_density = calculate_vegetation_density_alpine(
                        (float)elevation, slope, aspect, drainage, &opts->vegetation);
                    break;
                // TODO: Add other biomes (temperate, tropical, desert, arctic)
                default:
                    vegetation_density = calculate_vegetation_density_alpine(
                        (float)elevation, slope, aspect, drainage, &opts->vegetation);
                    break;
                }
            }

            row[x] = vegetation_density;
        }

// Write row to PNG (thread-safe)
#pragma omp critical
        {
            png_write_row(png_ptr, row);
        }
    }

    if (opts->verbose)
    {
        fprintf(stderr, "\rProgress: 100%% \n");
    }

    // Finalize PNG
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(fp);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Vegetation mask saved: %s\n", mask_filename);
    }

    return 0;
}
