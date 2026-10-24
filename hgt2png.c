/*
 * HGT2PNG - Professional Heightmap Converter v1.5.1
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
 *** Project:            HGT to PNG Heightmap Converter with multi-biome vegetation masks
 *** Creation Date:      2016-01-01 (Original), 2025-10-27 (v1.1.0 Rewrite), 2025-11-05 (v1.2.0 Vegetation), 2025-11-21 (v1.3.0 Multi-biome), 2026-10-22 (v1.4.0 Island removal), 2026-10-23 (v1.5.0 Latitude aridity)
 *** Author:             Peter Ebel (peter.ebel@outlook.de)
 *** Objective:          Professional conversion of SRTM HGT files to PNG displacement maps
 ***                     with multi-biome vegetation masks (Alpine + Temperate) for realistic 3D landscapes
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
 *** 1.2.0   2025-11-05  Ebel          Alpine vegetation mask generation system with scientific
 ***                                   biome modeling, elevation/slope/aspect analysis
 *** 1.3.0   2025-11-21  Ebel          Multi-biome system: Added Temperate biome for lowland/mid-elevation
 ***                                   terrain (0-1500m), parabolic density distribution, deciduous forests
 *** 1.4.0   2026-10-22  Ebel          --remove-islands: removes isolated land patches in the sea,
 ***                                   elevation normalization now runs before detail generation
 *** 1.4.1   2026-10-22  Ebel          --remove-islands also keeps open sea flat during detail generation
 ***                                   (procedural noise created phantom islands in the sea)
 *** 1.5.0   2026-10-23  Ebel          --lat-aridity / --aridity: latitude-dependent vegetation masks
 ***                                   (tree/bush/grass lines and density by latitude from the tile name);
 ***                                   fixed corrupted vegetation masks with more than one thread
 *** 1.5.1   2026-10-23  Ebel          Aridity is applied before the density is clamped (keeps relief structure),
 ***                                   density reduction raised to 0.7
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
} curve_type_t;

typedef enum
{
    METADATA_NONE = 0,
    METADATA_JSON = 1,
    METADATA_TXT = 2
} metadata_format_t;

// Vegetation Mask Parameters for different biomes
typedef struct
{
    int enabled;          // Enable vegetation mask generation
    float min_elevation;   // Minimum elevation for vegetation (meters)
    float max_elevation;   // Maximum elevation for vegetation (meters)
    float max_slope;       // Maximum slope angle for vegetation (degrees)
    float aspect_modifier; // North/South face modifier (-1.0 to 1.0)
    float drainage_bonus;  // Bonus for valleys/drainage areas
    float tree_line;       // Tree line elevation (meters)
    float bush_line;       // Bush line elevation (meters)
    float grass_line;      // Grass line elevation (meters)
    float density_scale;   // Final density multiplier applied before clamping (1.0 = none)
} vegetation_params_t;

typedef enum
{
    BIOME_ALPINE = 0,
    BIOME_TEMPERATE = 1,
    BIOME_TROPICAL = 2,
    BIOME_DESERT = 3,
    BIOME_ARCTIC = 4
} biome_type_t;

#define MAX_ARIDITY_POINTS 16

typedef struct
{
    float lat;   // Latitude in degrees
    float value; // Aridity 0 (humid) to 1 (arid)
} aridity_point_t;

typedef struct
{
    int scale_factor;
    float detail_intensity;
    int noise_seed;
    int enable_detail;
    int verbose;
    int show_help;
    int show_version;
    int num_threads;                // Option for thread count
    int output_16bit;               // Option for 16-bit PNG output
    float gamma;                   // Gamma correction (default: 1.0)
    curve_type_t curve_type;           // Curve type (linear/log)
    int min_height;                 // Minimum height for mapping (-1 = Auto)
    int max_height;                 // Maximum height for mapping (-1 = Auto)
    metadata_format_t metadata_format; // Metadata format (none/json/txt)
    int alpha_nodata;               // Option for transparent NoData pixels
    vegetation_params_t vegetation;   // Vegetation mask parameters
    biome_type_t biome;               // Selected biome type
    size_t remove_islands;          // Min. island size in source pixels (0 = disabled)
    aridity_point_t aridity_points[MAX_ARIDITY_POINTS]; // Control points sorted by latitude
    int aridity_point_count;         // 0 = latitude aridity disabled
} program_options_t;

// Threading structures for parallelization
typedef struct
{
    int file_index;
    struct tag_file_info_srtm *file_info; // Use full structure name
    program_options_t *opts;
    int *global_result;            // Shared result status
    pthread_mutex_t *output_mutex; // Mutex for thread-safe output
    int *files_processed;          // Shared counter for progress indication
    int total_files;               // Total number of files
} thread_data_t;

// Default values
#define DEFAULT_SCALE_FACTOR 3
#define DEFAULT_DETAIL_INTENSITY 15.0f
#define DEFAULT_NOISE_SEED 12345
#define DEFAULT_NUM_THREADS 4

// Alpine Biome Configuration - Realistic European Alps Parameters
#define ALPINE_MIN_ELEVATION 700.0f  // Vegetation starts at ~700m (montane zone)
#define ALPINE_MAX_ELEVATION 2400.0f // Vegetation ends at ~2400m (high alpine zone)
#define ALPINE_MAX_SLOPE 45.0f       // Maximum slope for vegetation (degrees) - more realistic
#define ALPINE_TREE_LINE 1800.0f     // Tree line elevation (spruce/larch limit)
#define ALPINE_BUSH_LINE 2200.0f     // Shrub line (dwarf pine, rhododendron)
#define ALPINE_GRASS_LINE 2400.0f    // Grass line (alpine meadows) - consistent with max
#define ALPINE_ASPECT_MODIFIER 0.2f  // South faces drier, North faces moister - reduced
#define ALPINE_DRAINAGE_BONUS 0.3f   // Valley bottoms have more vegetation - reduced

// Temperate Biome Configuration - Central European Lowlands/Mid-elevation
#define TEMPERATE_MIN_ELEVATION 0.0f    // Vegetation starts at sea level
#define TEMPERATE_MAX_ELEVATION 1500.0f // Vegetation typical up to ~1500m
#define TEMPERATE_MAX_SLOPE 50.0f       // More tolerant of slopes than alpine
#define TEMPERATE_TREE_LINE 1200.0f     // Deciduous/mixed forest limit (~1200m)
#define TEMPERATE_BUSH_LINE 1400.0f     // Shrub/bush zone (1200-1400m)
#define TEMPERATE_GRASS_LINE 1500.0f    // Grassland/meadow zone up to 1500m
#define TEMPERATE_ASPECT_MODIFIER 0.15f // Less aspect influence in temperate zones
#define TEMPERATE_DRAINAGE_BONUS 0.4f   // Strong drainage influence (wet lowlands)

// Aridity effect at aridity 1.0 (initial values, to be calibrated visually)
#define ARIDITY_DENSITY_REDUCTION 0.7f // Density multiplier is 1 - 0.7 * aridity
#define ARIDITY_LINE_SHIFT 400.0f      // Tree/bush/grass line drop in meters

// Vegetation density calculation functions
static float calculate_slope_angle(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, float pixel_pitch_meters);
static float calculate_aspect_angle(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, float pixel_pitch_meters);
static float calculate_drainage_factor(const int16_t *elevation_data, size_t width, size_t height, size_t x, size_t y, int radius);
static uint8_t calculate_vegetation_density_alpine(float elevation, float slope, float aspect, float drainage, const vegetation_params_t *params);
static uint8_t calculate_vegetation_density_temperate(float elevation, float slope, float aspect, float drainage, const vegetation_params_t *params);
static void initialize_alpine_biome(vegetation_params_t *params);
static void initialize_temperate_biome(vegetation_params_t *params);
static int generate_vegetation_mask(const struct tag_file_info_srtm *fi, const int16_t *elevation_data, const program_options_t *opts);

typedef int errno_t;

typedef struct tag_rgb
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} rgb_t, *p_rgb_t;

typedef struct tag_rgba
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} rgba_t, *p_rgba_t;

typedef struct tag_rgba16
{
    uint16_t y; // 16-bit Grayscale/Luminance
    uint16_t a; // 16-bit Alpha
} rgba16_t, *p_rgba16_t;

typedef struct tag_file_info_srtm
{
    char filename[_MAX_PATH];
    size_t width;
    size_t height;
    int16_t min_elevation; // Explicitly signed 16-bit for elevation values
    int16_t max_elevation; // Explicitly signed 16-bit for elevation values
    size_t filesize;
    int srtm_type;           // Per-file SRTM type instead of global variable
    int nodata_count;       // Number of NoData pixels for statistics
} file_info_t, *p_file_info_t;

file_info_t *fi;
rgb_t *pixel_data;

void write_png(const char *, size_t, size_t);
static void write_png_with_data(const char *filename, size_t width, size_t height, const rgb_t *pixel_data);
static void write_png_with_alpha(const char *filename, size_t width, size_t height, const rgba_t *pixel_data);
static void write_png_16bit_grayscale(const char *filename, size_t width, size_t height, const uint16_t *pixel_data);
static void write_png_16bit_with_alpha(const char *filename, size_t width, size_t height, const rgba16_t *pixel_data);

// Parameter management functions
static void init_default_options(program_options_t *opts);
static void show_help(const char *program_name);
static void show_version(void);
static int parse_arguments(int argc, char *argv[], program_options_t *opts, char **input_file);

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
static float simple_noise(int x, int y, int seed);
static float fractal_noise(float x, float y, int octaves, float persistence, float scale, int seed);
static short int bilinear_interpolate(const short int *data, int width, int height, float x, float y);
static float get_pixel_distance(int srtm_type);
static float calculate_local_slope(const short int *data, int width, int height, float x, float y, int srtm_type);
static float get_height_type_factor(short int height);
static float apply_curve_mapping(float normalized_value, curve_type_t curve_type, float gamma);
static int extract_geo_bounds(const char *filename, float *south, float *north, float *west, float *east);
static float latitude_at_row(float south, float north, size_t row, size_t height);
static float aridity_at_lat(const aridity_point_t *points, int count, float lat);
static void apply_aridity(vegetation_params_t *params, float aridity);
static void write_metadata_file(const char *png_filename, const program_options_t *opts, const struct tag_file_info_srtm *fi, int effective_min_height, int effective_max_height);
static short int *add_procedural_detail(const short int *originalData, int originalWidth, int originalHeight, int scale_factor, float detail_intensity, int seed, int srtm_type, int flat_sea);
static int remove_small_islands(int16_t *data, size_t width, size_t height, size_t min_pixels, size_t *removed_islands);

// Functions for parallelization
static void *process_file_worker(void *arg);
static int process_files_parallel(struct tag_file_info_srtm *fi, int num_files_to_convert, program_options_t opts);
static int process_files_sequential(struct tag_file_info_srtm *fi, int num_files_to_convert, program_options_t opts);

// Helper function: Extracts filename from path and generates PNG name in current directory
static void generate_output_filename(const char *input_path, char *output_path);

// NoData handling
static int is_nodata_value(int16_t value, int srtm_type);
static int16_t process_elevation_value(int16_t raw_value, int srtm_type, int *nodata_count);

const int SRTM3 = 1201;
const int SRTM1 = 3601;
const int SRTM_UNKNOWN = -1;
const size_t SRTM3_SIZE = 1201 * 1201 * sizeof(int16_t);
const size_t SRTM1_SIZE = 3601 * 3601 * sizeof(int16_t);

// NoData constants (SRTM standard) - Big Endian format
const int16_t NODATA_VALUE_BE = (int16_t)0x8000; // Big Endian: -32768
const int16_t NODATA_REPLACEMENT = 0;            // Replacement value for NoData

char output_heightmap_file[_MAX_PATH];

int16_t overall_min_elevation;
int16_t overall_max_elevation;
int16_t current_min_elevation;
int16_t current_max_elevation;

int main(int argc, char *argv[])
{
    program_options_t opts;
    char *input_file = NULL;

    FILE *in_file;
    FILE *file_list;
    char *current_filename[255];
    char buffer[255];
    int f = 0;

    short int *elevation_data;
    int num_int_read;
    int num_files_to_convert = 0;

    // Initialize default options
    init_default_options(&opts);

    // Parse command line arguments
    int parse_result = parse_arguments(argc, argv, &opts, &input_file);
    if (parse_result != 0)
    {
        return parse_result;
    }

    // Show version if requested
    if (opts.show_version)
    {
        show_version();
        return 0;
    }

    // Show help if requested or no input file
    if (opts.show_help || input_file == NULL)
    {
        show_help(argv[0]);
        return opts.show_help ? 0 : 1;
    }

    if (opts.verbose)
    {
        fprintf(stderr, "\nhgt2png Converter v1.5.1 (C) 2025 - with Procedural Detail Generation and Multi-Biome Vegetation Masks\n");
        fprintf(stderr, "Scale Factor: %d, Detail Intensity: %.1f, Seed: %d\n",
                opts.scale_factor, opts.detail_intensity, opts.noise_seed);
    }

    if ((strstr(input_file, "HGT") != NULL) || strstr(input_file, "hgt") != NULL)
    {
        if (opts.verbose)
            fprintf(stderr, "INFO: Single-File Mode\n");
        current_filename[0] = (char *)malloc(strlen(input_file) + 1);
        if (current_filename[0] == NULL)
        {
            fprintf(stderr, "Error: Can't allocate memory for filename\n");
            return 1;
        }
        strcpy(current_filename[0], input_file);
        num_files_to_convert = 1;
    }
    else
    {
        if (opts.verbose)
            fprintf(stderr, "INFO: Filelist Mode\n");
        if ((file_list = fopen(input_file, "rb")) == NULL)
        {
            fprintf(stderr, "Error: Can't open file list %s\n", input_file);
            // Cleanup falls bereits Single-File allokiert wurde
            if (num_files_to_convert > 0 && current_filename[0] != NULL)
            {
                free(current_filename[0]);
                current_filename[0] = NULL;
            }
            return 1;
        }

        while (!feof(file_list) && f < _MAX_FILES - 1)
        {
            if (fgets(buffer, sizeof(buffer), file_list) != NULL)
            {
                // Remove newline and check for empty lines
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n')
                {
                    buffer[len - 1] = '\0';
                    len--;
                }

                // Skip empty lines
                if (len > 0)
                {
                    current_filename[f] = (char *)malloc(len + 1);
                    if (current_filename[f] == NULL)
                    {
                        fprintf(stderr, "Error: Can't allocate memory for filename %d\n", f);
                        // Cleanup already allocated filenames
                        for (int cleanup = 0; cleanup < f; cleanup++)
                        {
                            free(current_filename[cleanup]);
                            current_filename[cleanup] = NULL;
                        }
                        fclose(file_list);
                        return 1;
                    }
                    strcpy(current_filename[f], buffer);
                    f++;
                }
            }
        }

        if (f >= _MAX_FILES - 1)
        {
            fprintf(stderr, "WARNING: Maximum number of files (%d) reached. Some files may be skipped.\n", _MAX_FILES - 1);
        }

        fclose(file_list);
        num_files_to_convert = f;
    }

    fprintf(stderr, "INFO: Number of files to convert: %d\n", num_files_to_convert);

    if ((fi = (file_info_t *)malloc(num_files_to_convert * sizeof(struct tag_file_info_srtm))) == NULL)
    {
        fprintf(stderr, "Error: Can't allocate FileInfo array\n");
        // Cleanup already allocated filenames
        for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
        {
            if (current_filename[cleanup] != NULL)
            {
                free(current_filename[cleanup]);
                current_filename[cleanup] = NULL;
            }
        }
        return 1;
    }

    struct stat buf;

    overall_min_elevation = 9999;
    overall_max_elevation = 0;
    current_min_elevation = 9999;
    current_max_elevation = 0;

    for (int i = 0; i < num_files_to_convert; i++)
    {

        num_int_read = 0;
        char tmp[5];

        memset(&fi[i], 0, sizeof(file_info_t));
        strcpy(fi[i].filename, current_filename[i]);
        fprintf(stderr, "INFO: Pre-Processing: %s ", fi[i].filename);
        fi[i].min_elevation = 0;
        fi[i].max_elevation = 0;

        if ((in_file = fopen(fi[i].filename, "rb")) == NULL)
        {
            fprintf(stderr, "Error: Can't open input file %s\n", fi[i].filename);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
            {
                if (current_filename[cleanup] != NULL)
                {
                    free(current_filename[cleanup]);
                    current_filename[cleanup] = NULL;
                }
            }
            free(fi);
            fi = NULL;
            return 1;
        }

        fstat(fileno(in_file), &buf);
        fi[i].filesize = buf.st_size;

        if (fi[i].filesize == SRTM3_SIZE)
        {
            fi[i].srtm_type = SRTM3; // Pro-Datei statt global
            fi[i].width = 1201;
            fi[i].height = 1201;
        }
        else if (fi[i].filesize == SRTM1_SIZE)
        {
            fi[i].srtm_type = SRTM1; // Per-file instead of global
            fi[i].width = 3601;
            fi[i].height = 3601;
        }
        else
        {
            // Custom size parsing via filename with fallback
            fi[i].width = 0;
            fi[i].height = 0;

            // Safe parsing only for sufficiently long filename
            size_t name_len = strlen(fi[i].filename);
            if (name_len >= 15)
            { // At least "N00E000_1234x5678.hgt"
                memcpy(tmp, &current_filename[i][5], 4);
                tmp[4] = '\0';
                int parsed_width = atoi(tmp);

                memcpy(tmp, &current_filename[i][10], 4);
                tmp[4] = '\0';
                int parsed_height = atoi(tmp);

                // Plausibility check: dimensions must be reasonable
                if (parsed_width > 0 && parsed_width <= 65536 &&
                    parsed_height > 0 && parsed_height <= 65536)
                {
                    fi[i].width = parsed_width;
                    fi[i].height = parsed_height;
                    fi[i].srtm_type = fi[i].width * fi[i].height;
                }
            }

            // Fallback: Mark as UNKNOWN for invalid parsing
            if (fi[i].width == 0 || fi[i].height == 0)
            {
                fi[i].srtm_type = SRTM_UNKNOWN;
                if (opts.verbose)
                {
                    fprintf(stderr, "WARNING: Could not parse dimensions from filename %s\n", fi[i].filename);
                }
            }
            else
            {
                // Additional validation: file size must match dimensions (overflow-protected)
                size_t expected_pixels, expected_size;
                if (!safe_multiply_size_t(fi[i].width, fi[i].height, &expected_pixels) ||
                    !safe_multiply_size_t(expected_pixels, 2, &expected_size))
                {
                    fprintf(stderr, "ERROR: Dimension overflow for %s: %zu×%zu pixels\n",
                            fi[i].filename, fi[i].width, fi[i].height);
                    fi[i].srtm_type = SRTM_UNKNOWN; // Mark as invalid
                }
                else if (fi[i].filesize != expected_size)
                {
                    fprintf(stderr, "ERROR: Filesize mismatch for %s: expected %zu bytes (%zu×%zu), got %zu bytes\n",
                            fi[i].filename, expected_size, fi[i].width, fi[i].height, fi[i].filesize);
                    fi[i].srtm_type = SRTM_UNKNOWN; // Mark as invalid
                }
            }
        }

        if (fi[i].srtm_type == SRTM_UNKNOWN)
        { // Per-file check
            fprintf(stderr, "Error: %s has an unknown SRTM type\n", fi[i].filename);
            fclose(in_file);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
            {
                if (current_filename[cleanup] != NULL)
                {
                    free(current_filename[cleanup]);
                    current_filename[cleanup] = NULL;
                }
            }
            free(fi);
            fi = NULL;
            return 1;
        }

        if ((elevation_data = (short *)malloc(fi[i].filesize)) == NULL)
        {
            fprintf(stderr, "Error: Can't allocate memory to load %s\n", fi[i].filename);
            fclose(in_file);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
            {
                if (current_filename[cleanup] != NULL)
                {
                    free(current_filename[cleanup]);
                    current_filename[cleanup] = NULL;
                }
            }
            free(fi);
            fi = NULL;
            return 1;
        }

        if ((num_int_read = fread(elevation_data, 1, fi[i].filesize, in_file)) != (int)fi[i].filesize)
        {
            fprintf(stderr, "Error: Can't load elevation data\n");
            free(elevation_data);
            elevation_data = NULL;
            fclose(in_file);
            // Cleanup allocated memory
            for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
            {
                if (current_filename[cleanup] != NULL)
                {
                    free(current_filename[cleanup]);
                    current_filename[cleanup] = NULL;
                }
            }
            free(fi);
            fi = NULL;
            return 1;
        }

        current_max_elevation = 0;
        current_min_elevation = 9999;
        fi[i].nodata_count = 0; // Initialize NoData counter

        for (unsigned long j = 0; j < fi[i].filesize / 2; j++)
        {
            // NoData handling
            elevation_data[j] = process_elevation_value(elevation_data[j], fi[i].srtm_type, &fi[i].nodata_count);

            // Min/Max only for valid values (not for NoData replacement values)
            if (elevation_data[j] != NODATA_REPLACEMENT)
            {
                if (elevation_data[j] < current_min_elevation)
                    current_min_elevation = elevation_data[j];
                if (elevation_data[j] > current_max_elevation)
                    current_max_elevation = elevation_data[j];
            }
        }

        if (current_min_elevation < overall_min_elevation)
            overall_min_elevation = current_min_elevation;
        if (current_max_elevation > overall_max_elevation)
            overall_max_elevation = current_max_elevation;

        // Output NoData statistics - avoid division by 0
        if (fi[i].nodata_count > 0)
        {
            unsigned long total_pixels = fi[i].filesize / 2;
            float nodata_percent = 0.0f;
            if (total_pixels > 0)
            {
                nodata_percent = (float)fi[i].nodata_count / total_pixels * 100.0f;
            }
            fprintf(stderr, "- MIN=%4d MAX=%4d, NoData=%d (%.1f%%)\n",
                    current_min_elevation, current_max_elevation, fi[i].nodata_count, nodata_percent);
        }
        else
        {
            fprintf(stderr, "- MIN=%4d MAX=%4d\n", current_min_elevation, current_max_elevation);
        }

        free(elevation_data);
        elevation_data = NULL;
        fclose(in_file);
    }

    // PARALLELIZED BATCH PROCESSING
    int processing_result = process_files_parallel(fi, num_files_to_convert, opts);

    if (processing_result != 0)
    {
        // Cleanup on error
        for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
        {
            if (current_filename[cleanup] != NULL)
            {
                free(current_filename[cleanup]);
                current_filename[cleanup] = NULL;
            }
        }
        free(fi);
        fi = NULL;
        return processing_result;
    }

    // Cleanup allocated memory
    free(fi);
    fi = NULL;

    for (int cleanup = 0; cleanup < num_files_to_convert; cleanup++)
    {
        if (current_filename[cleanup] != NULL)
        {
            free(current_filename[cleanup]);
        }
    }

    fprintf(stderr, "Info: Done\n");
}

void write_png(const char *filename, size_t width, size_t height)
{
    write_png_with_data(filename, width, height, pixel_data);
}

static void write_png_with_data(const char *filename, size_t width, size_t height, const rgb_t *pixel_data)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_RGB;
    image.width = width;
    image.height = height;

    fprintf(stderr, "Info: Writing %s\n", filename);
    if (!png_image_write_to_file(&image, filename, 0 /*convert_to_8bit*/, (png_bytep)pixel_data, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", filename, image.message);
    }

    png_image_free(&image);
}

static void write_png_with_alpha(const char *filename, size_t width, size_t height, const rgba_t *pixel_data)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_RGBA; // RGBA mit Alpha-Kanal
    image.width = width;
    image.height = height;

    fprintf(stderr, "Info: Writing RGBA %s\n", filename);
    if (!png_image_write_to_file(&image, filename, 0 /*convert_to_8bit*/, (png_bytep)pixel_data, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", filename, image.message);
    }

    png_image_free(&image);
}

static void write_png_16bit_grayscale(const char *filename, size_t width, size_t height, const uint16_t *pixel_data)
{
    png_image image;

    memset(&image, 0, sizeof image);

    image.version = PNG_IMAGE_VERSION;
    image.format = PNG_FORMAT_LINEAR_Y; // 16-bit Grayscale
    image.width = width;
    image.height = height;

    fprintf(stderr, "Info: Writing 16-bit %s\n", filename);
    if (!png_image_write_to_file(&image, filename, 0 /*convert_to_8bit*/, (png_bytep)pixel_data, 0 /*row_stride*/, NULL /*colormap*/))
    {
        fprintf(stderr, "Error: Writing %s: %s\n", filename, image.message);
    }

    png_image_free(&image);
}

static void write_png_16bit_with_alpha(const char *filename, size_t width, size_t height, const rgba16_t *pixel_data)
{
    // The simplified libpng API treats linear gray+alpha as premultiplied and writes 65535 under alpha 0
    fprintf(stderr, "Info: Writing 16-bit RGBA %s\n", filename);

    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Error: Writing %s: cannot open file\n", filename);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_ptr ? png_create_info_struct(png_ptr) : NULL;
    if (!png_ptr || !info_ptr)
    {
        fprintf(stderr, "Error: Writing %s: cannot create PNG structures\n", filename);
        if (png_ptr)
            png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        fprintf(stderr, "Error: Writing %s: PNG write error\n", filename);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, (png_uint_32)width, (png_uint_32)height, 16,
                 PNG_COLOR_TYPE_GRAY_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    // Same gAMA/cHRM chunks as the simplified API writes for the other 16-bit outputs
    png_set_gAMA_fixed(png_ptr, info_ptr, 100000);
    png_set_cHRM_fixed(png_ptr, info_ptr, 31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000);
    png_write_info(png_ptr, info_ptr);
    png_set_swap(png_ptr); // PNG samples are big-endian

    for (size_t y = 0; y < height; y++)
    {
        png_write_row(png_ptr, (png_bytep)(pixel_data + y * width));
    }

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

// PARALLELIZATION - Worker thread function
static void *process_file_worker(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    struct tag_file_info_srtm *fi = &data->file_info[data->file_index];
    program_options_t *opts = data->opts;

    FILE *in_file = NULL;
    short *elevation_data = NULL;
    rgb_t *pixel_data = NULL;
    rgba_t *pixel_data_rgba = NULL; // New RGBA data for alpha support
    int num_int_read;
    char output_heightmap_file[256];

    // Thread-safe progress indication
    pthread_mutex_lock(data->output_mutex);
    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Processing file %d/%d: %s\n",
                data->file_index + 1, data->total_files, fi->filename);
    }
    pthread_mutex_unlock(data->output_mutex);

    // Open file
    if ((in_file = fopen(fi->filename, "rb")) == NULL)
    {
        pthread_mutex_lock(data->output_mutex);
        fprintf(stderr, "Error: Can't open input file %s\n", fi->filename);
        pthread_mutex_unlock(data->output_mutex);
        *(data->global_result) = 1;
        return NULL; // No memory to leak here
    }

    // Allocate memory for elevation data
    if ((elevation_data = (short *)malloc(fi->filesize)) == NULL)
    {
        pthread_mutex_lock(data->output_mutex);
        fprintf(stderr, "Error: Can't allocate elevation data block %s\n", fi->filename);
        pthread_mutex_unlock(data->output_mutex);
        fclose(in_file);
        *(data->global_result) = 1;
        return NULL; // File was closed, no memory allocated
    }

    // Read elevation data
    if ((num_int_read = fread(elevation_data, 1, fi->filesize, in_file)) != (int)fi->filesize)
    {
        pthread_mutex_lock(data->output_mutex);
        fprintf(stderr, "Error: Can't load elevation data from %s\n", fi->filename);
        pthread_mutex_unlock(data->output_mutex);
        fclose(in_file);
        free(elevation_data);
        elevation_data = NULL;
        *(data->global_result) = 1;
        return NULL;
    }
    fclose(in_file);

    // Endian conversion, NoData replacement and clamping must precede island removal and detail generation
    int thread_nodata_count = 0;
    for (unsigned long pre = 0; pre < (fi->filesize) / 2; pre++)
    {
        elevation_data[pre] = process_elevation_value(elevation_data[pre], fi->srtm_type, &thread_nodata_count);
    }

    // Update NoData statistics thread-safely
    pthread_mutex_lock(data->output_mutex);
    fi->nodata_count += thread_nodata_count;
    if (opts->verbose && thread_nodata_count > 0)
    {
        fprintf(stderr, "INFO: Found %d NoData values in %s\n", thread_nodata_count, fi->filename);
    }
    pthread_mutex_unlock(data->output_mutex);

    if (opts->remove_islands > 0)
    {
        size_t removed_islands = 0;
        int island_result = remove_small_islands((int16_t *)elevation_data, fi->width, fi->height,
                                              opts->remove_islands, &removed_islands);

        pthread_mutex_lock(data->output_mutex);
        if (island_result != 0)
        {
            fprintf(stderr, "WARNING: Island removal skipped for %s (allocation failed)\n", fi->filename);
        }
        else if (opts->verbose)
        {
            fprintf(stderr, "INFO: Removed %zu islands smaller than %zu pixels in %s\n",
                    removed_islands, opts->remove_islands, fi->filename);
        }
        pthread_mutex_unlock(data->output_mutex);
    }

    // PROCEDURAL DETAIL GENERATION
    if (opts->enable_detail)
    {
        // Add procedural detail
        short int *detailed_data = add_procedural_detail(elevation_data, fi->width, fi->height,
                                                      opts->scale_factor, opts->detail_intensity, opts->noise_seed, fi->srtm_type,
                                                      opts->remove_islands > 0);

        if (detailed_data)
        {
            free(elevation_data);
            elevation_data = detailed_data;
            fi->width *= opts->scale_factor;
            fi->height *= opts->scale_factor;

            // Safe calculation of new file size
            size_t new_pixels, new_filesize;
            if (!safe_multiply_size_t(fi->width, fi->height, &new_pixels) ||
                !safe_multiply_size_t(new_pixels, sizeof(short int), &new_filesize))
            {
                pthread_mutex_lock(data->output_mutex);
                fprintf(stderr, "ERROR: File size overflow after enhancement: %zu×%zu for %s\n",
                        fi->width, fi->height, fi->filename);
                pthread_mutex_unlock(data->output_mutex);
                free(elevation_data);
                elevation_data = NULL;
                *(data->global_result) = 1;
                return NULL;
            }
            fi->filesize = new_filesize;

            pthread_mutex_lock(data->output_mutex);
            if (opts->verbose)
            {
                fprintf(stderr, "INFO: Enhanced resolution: %zu×%zu pixels for %s\n",
                        fi->width, fi->height, fi->filename);
            }
            pthread_mutex_unlock(data->output_mutex);
        }
        else
        {
            pthread_mutex_lock(data->output_mutex);
            fprintf(stderr, "WARNING: Could not add procedural detail to %s, using original data\n",
                    fi->filename);
            pthread_mutex_unlock(data->output_mutex);
        }
    }

    // Allocate memory for pixel data - depending on output format
    size_t pixel_count;
    if (!safe_multiply_size_t(fi->width, fi->height, &pixel_count))
    {
        pthread_mutex_lock(data->output_mutex);
        fprintf(stderr, "Error: Pixel count overflow at %zu×%zu for %s\n",
                fi->width, fi->height, fi->filename);
        pthread_mutex_unlock(data->output_mutex);
        free(elevation_data);
        elevation_data = NULL;
        *(data->global_result) = 1;
        return NULL;
    }

    unsigned short *pixel_data_16 = NULL;
    rgba16_t *pixel_data_16_alpha = NULL;

    if (opts->output_16bit && opts->alpha_nodata)
    {
        // 16-bit grayscale + alpha for transparent NoData
        pixel_data_16_alpha = (rgba16_t *)safe_malloc_pixels(fi->width, fi->height, sizeof(rgba16_t));
        if (pixel_data_16_alpha == NULL)
        {
            pthread_mutex_lock(data->output_mutex);
            fprintf(stderr, "Error: Can't allocate 16-bit RGBA pixel data block for %s (%zu×%zu pixels)\n",
                    fi->filename, fi->width, fi->height);
            pthread_mutex_unlock(data->output_mutex);
            free(elevation_data);
            elevation_data = NULL;
            *(data->global_result) = 1;
            return NULL;
        }
    }
    else if (opts->output_16bit)
    {
        // 16-bit grayscale data with overflow-protected allocation
        pixel_data_16 = (unsigned short *)safe_malloc_pixels(fi->width, fi->height, sizeof(unsigned short));
        if (pixel_data_16 == NULL)
        {
            pthread_mutex_lock(data->output_mutex);
            fprintf(stderr, "Error: Can't allocate 16-bit pixel data block for %s (%zu×%zu pixels)\n",
                    fi->filename, fi->width, fi->height);
            pthread_mutex_unlock(data->output_mutex);
            free(elevation_data);
            elevation_data = NULL;
            *(data->global_result) = 1;
            return NULL;
        }
    }
    else if (opts->alpha_nodata)
    {
        // 8-bit RGBA data for alpha support with overflow-protected allocation
        pixel_data_rgba = (rgba_t *)safe_malloc_pixels(fi->width, fi->height, sizeof(struct tag_rgba));
        if (pixel_data_rgba == NULL)
        {
            pthread_mutex_lock(data->output_mutex);
            fprintf(stderr, "Error: Can't allocate RGBA pixel data block for %s (%zu×%zu pixels)\n",
                    fi->filename, fi->width, fi->height);
            pthread_mutex_unlock(data->output_mutex);
            free(elevation_data);
            elevation_data = NULL;
            *(data->global_result) = 1;
            return NULL;
        }
    }
    else
    {
        // 8-bit RGB data with overflow-protected allocation
        pixel_data = (rgb_t *)safe_malloc_pixels(fi->width, fi->height, sizeof(struct tag_rgb));
        if (pixel_data == NULL)
        {
            pthread_mutex_lock(data->output_mutex);
            fprintf(stderr, "Error: Can't allocate RGB pixel data block for %s (%zu×%zu pixels)\n",
                    fi->filename, fi->width, fi->height);
            pthread_mutex_unlock(data->output_mutex);
            free(elevation_data);
            elevation_data = NULL;
            *(data->global_result) = 1;
            return NULL;
        }
    }

    // Calculate effective min/max heights (custom range or auto-detection)
    int effective_min_height = (opts->min_height != -1) ? opts->min_height : overall_min_elevation;
    int effective_max_height = (opts->max_height != -1) ? opts->max_height : overall_max_elevation;

    // Safety check: Min < Max
    if (effective_min_height >= effective_max_height)
    {
        effective_min_height = overall_min_elevation;
        effective_max_height = overall_max_elevation;
    }

    // Convert elevation to pixels - depending on output format
    // OpenMP: SIMD vectorization for optimal performance with large images
#pragma omp parallel for simd aligned(elevation_data : 32) if (pixel_count > 10000)
    for (unsigned long m = 0; m < pixel_count; m++)
    {
        // Check for NoData pixels (for alpha transparency)
        int is_nodata_pixel = (elevation_data[m] == NODATA_REPLACEMENT);

        // Normalization to 0.0-1.0 with custom range
        float normalized_value = 0.5f; // Fallback
        if (effective_max_height > effective_min_height)
        {
            // Clamp elevation to effective range
            int clamped_elevation = elevation_data[m];
            if (clamped_elevation < effective_min_height)
                clamped_elevation = effective_min_height;
            if (clamped_elevation > effective_max_height)
                clamped_elevation = effective_max_height;

            normalized_value = (float)(clamped_elevation - effective_min_height) /
                              (float)(effective_max_height - effective_min_height);
        }

        // Apply curve mapping
        float curved_value = apply_curve_mapping(normalized_value, opts->curve_type, opts->gamma);

        if (opts->output_16bit && opts->alpha_nodata)
        {
            // 16-bit grayscale + alpha: make NoData pixels transparent
            uint16_t pixel_value = (uint16_t)(curved_value * 65535.0f);
            pixel_data_16_alpha[m].y = pixel_value;
            pixel_data_16_alpha[m].a = is_nodata_pixel ? 0 : 65535; // Transparent for NoData, opaque for valid data
        }
        else if (opts->output_16bit)
        {
            // 16-bit grayscale: use full 16-bit range (0-65535)
            pixel_data_16[m] = (unsigned short)(curved_value * 65535.0f);
        }
        else if (opts->alpha_nodata)
        {
            // 8-bit RGBA: with alpha channel for NoData transparency
            unsigned char pixel_value = (unsigned char)(curved_value * 255.0f);
            pixel_data_rgba[m].r = pixel_value;
            pixel_data_rgba[m].g = pixel_value;
            pixel_data_rgba[m].b = pixel_value;
            pixel_data_rgba[m].a = is_nodata_pixel ? 0 : 255; // Transparent for NoData, opaque for valid data
        }
        else
        {
            // 8-bit RGB: curve-corrected calculation
            unsigned char pixel_value = (unsigned char)(curved_value * 255.0f);
            pixel_data[m].r = pixel_data[m].g = pixel_data[m].b = pixel_value;
        }
    }

    // Generate PNG output filename (filename only, not full path)
    generate_output_filename(fi->filename, output_heightmap_file);

    // Write PNG (thread-safe with local PixelData)
    pthread_mutex_lock(data->output_mutex);
    if (opts->output_16bit && opts->alpha_nodata)
    {
        write_png_16bit_with_alpha(output_heightmap_file, fi->width, fi->height, pixel_data_16_alpha);
    }
    else if (opts->output_16bit)
    {
        write_png_16bit_grayscale(output_heightmap_file, fi->width, fi->height, pixel_data_16);
    }
    else if (opts->alpha_nodata)
    {
        write_png_with_alpha(output_heightmap_file, fi->width, fi->height, pixel_data_rgba);
    }
    else
    {
        write_png_with_data(output_heightmap_file, fi->width, fi->height, pixel_data);
    }

    // Write metadata file (if enabled)
    write_metadata_file(output_heightmap_file, opts, fi, effective_min_height, effective_max_height);

    // Generate vegetation mask (if enabled)
    if (opts->vegetation.enabled)
    {
        // We need to reload the elevation data for vegetation mask since we may have modified it
        // Use the original elevation data stored in elevation_data before any processing
        generate_vegetation_mask(fi, elevation_data, opts);
    }

    pthread_mutex_unlock(data->output_mutex);

    // Cleanup for this thread
    free(elevation_data);
    elevation_data = NULL;
    if (opts->output_16bit && opts->alpha_nodata)
    {
        free(pixel_data_16_alpha);
        pixel_data_16_alpha = NULL;
    }
    else if (opts->output_16bit)
    {
        free(pixel_data_16);
        pixel_data_16 = NULL;
    }
    else if (opts->alpha_nodata)
    {
        free(pixel_data_rgba); // Free thread-local RGBA data
        pixel_data_rgba = NULL;
    }
    else
    {
        free(pixel_data); // Free thread-local PixelData
        pixel_data = NULL;
    }

    // Update progress counter
    pthread_mutex_lock(data->output_mutex);
    (*(data->files_processed))++;
    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Completed %d/%d files\n", *(data->files_processed), data->total_files);
    }
    pthread_mutex_unlock(data->output_mutex);

    return NULL;
}

// PARALLELIZED BATCH PROCESSING
static int process_files_parallel(struct tag_file_info_srtm *fi, int num_files_to_convert, program_options_t opts)
{
    if (num_files_to_convert <= 1 || opts.num_threads <= 1)
    {
        // Fallback to sequential processing for few files or 1 thread
        return process_files_sequential(fi, num_files_to_convert, opts);
    }

    pthread_t *threads;
    thread_data_t *thread_data_arr;
    pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
    int global_result = 0;
    int files_processed = 0;

    // Adapt number of threads to available files
    int actual_threads = (opts.num_threads > num_files_to_convert) ? num_files_to_convert : opts.num_threads;

    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Starting parallel processing with %d threads for %d files\n",
                actual_threads, num_files_to_convert);
    }

    // Allocate thread arrays
    threads = (pthread_t *)malloc(actual_threads * sizeof(pthread_t));
    thread_data_arr = (thread_data_t *)malloc(num_files_to_convert * sizeof(thread_data_t));

    if (!threads || !thread_data_arr)
    {
        fprintf(stderr, "Error: Cannot allocate memory for threading\n");
        if (threads)
            free(threads);
        if (thread_data_arr)
            free(thread_data_arr);
        return 1;
    }

    // Prepare thread data for all files
    for (int i = 0; i < num_files_to_convert; i++)
    {
        thread_data_arr[i].file_index = i;
        thread_data_arr[i].file_info = fi;
        thread_data_arr[i].opts = &opts;
        thread_data_arr[i].global_result = &global_result;
        thread_data_arr[i].output_mutex = &output_mutex;
        thread_data_arr[i].files_processed = &files_processed;
        thread_data_arr[i].total_files = num_files_to_convert;
    }

    // Start threads in batches (thread pool simulation)
    int file_index = 0;
    while (file_index < num_files_to_convert && global_result == 0)
    {
        int threads_to_start = (num_files_to_convert - file_index > actual_threads) ? actual_threads : (num_files_to_convert - file_index);

        // Start threads
        for (int t = 0; t < threads_to_start; t++)
        {
            if (pthread_create(&threads[t], NULL, process_file_worker, &thread_data_arr[file_index + t]) != 0)
            {
                fprintf(stderr, "Error: Cannot create thread %d\n", t);
                global_result = 1;
                break;
            }
        }

        // Wait for all started threads
        for (int t = 0; t < threads_to_start; t++)
        {
            pthread_join(threads[t], NULL);
        }

        file_index += threads_to_start;
    }

    // Cleanup
    free(threads);
    free(thread_data_arr);
    pthread_mutex_destroy(&output_mutex);

    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Parallel processing completed. Result: %s\n",
                global_result == 0 ? "SUCCESS" : "ERROR");
    }

    return global_result;
}

// SEQUENTIAL PROCESSING (Fallback)
static int process_files_sequential(struct tag_file_info_srtm *fi, int num_files_to_convert, program_options_t opts)
{
    if (opts.verbose)
    {
        fprintf(stderr, "INFO: Using sequential processing for %d files\n", num_files_to_convert);
    }

    // The original loop as a separate function
    for (int k = 0; k < num_files_to_convert; k++)
    {
        // Use worker logic, but without threading
        thread_data_t data;
        data.file_index = k;
        data.file_info = fi;
        data.opts = &opts;
        int result = 0;
        data.global_result = &result;
        pthread_mutex_t dummy_mutex = PTHREAD_MUTEX_INITIALIZER;
        data.output_mutex = &dummy_mutex;
        int processed = k;
        data.files_processed = &processed;
        data.total_files = num_files_to_convert;

        process_file_worker(&data);

        if (result != 0)
        {
            pthread_mutex_destroy(&dummy_mutex);
            return result;
        }
        pthread_mutex_destroy(&dummy_mutex);
    }

    return 0;
}

// PROCEDURAL DETAIL GENERATION

// Simple noise function (Perlin-like)
static float simple_noise(int x, int y, int seed)
{
    int n = x + y * 57 + seed * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

// Bilinear interpolation for noise sampling (eliminates quantization)
static float bilinear_noise_interpolate(float x, float y, int seed)
{
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    float fx = x - x0;
    float fy = y - y0;

    float v00 = simple_noise(x0, y0, seed);
    float v10 = simple_noise(x0 + 1, y0, seed);
    float v01 = simple_noise(x0, y0 + 1, seed);
    float v11 = simple_noise(x0 + 1, y0 + 1, seed);

    float v0 = v00 * (1.0f - fx) + v10 * fx;
    float v1 = v01 * (1.0f - fx) + v11 * fx;

    return v0 * (1.0f - fy) + v1 * fy;
}

// Fractal noise for different detail levels (with sub-pixel precision)
static float fractal_noise(float x, float y, int octaves, float persistence, float scale, int seed)
{
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++)
    {
        // Bilinear interpolation instead of integer casting eliminates quantization
        total += bilinear_noise_interpolate(x * frequency, y * frequency, seed + i) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / max_value;
}

// Bilinear interpolation
static short int bilinear_interpolate(const short int *data, int width, int height, float x, float y)
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
static float get_pixel_distance(int srtm_type)
{
    if (srtm_type == SRTM3)
    {
        return 60.0f; // 2 * 30m for SRTM-1 (1201x1201)
    }
    else if (srtm_type == SRTM1)
    {
        return 180.0f; // 2 * 90m for SRTM-3 (3601x3601)
    }
    else
    {
        // Custom size: assume medium resolution
        return 60.0f; // Fallback to SRTM-1 resolution
    }
}

static float calculate_local_slope(const short int *data, int width, int height, float x, float y, int srtm_type)
{
    int ix = (int)x;
    int iy = (int)y;

    if (ix <= 0 || ix >= width - 1 || iy <= 0 || iy >= height - 1)
        return 0.0f;

    short int left = data[iy * width + (ix - 1)];
    short int right = data[iy * width + (ix + 1)];
    short int top = data[(iy - 1) * width + ix];
    short int bottom = data[(iy + 1) * width + ix];

    float pixel_distance = get_pixel_distance(srtm_type);
    float dx = (right - left) / pixel_distance;
    float dy = (bottom - top) / pixel_distance;

    float slope = sqrt(dx * dx + dy * dy) / 100.0f; // Normalized to 0-1
    if (slope > 1.0f)
        slope = 1.0f;
    return slope;
}

// Terrain-type-specific factors
static float get_height_type_factor(short int height)
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
static float apply_curve_mapping(float normalized_value, curve_type_t curve_type, float gamma)
{
    // Input clipping
    if (normalized_value < 0.0f)
        normalized_value = 0.0f;
    if (normalized_value > 1.0f)
        normalized_value = 1.0f;

    float result = normalized_value;

    switch (curve_type)
    {
    case CURVE_LINEAR:
        // Linear curve - no change
        result = normalized_value;
        break;

    case CURVE_LOG:
        // Logarithmic curve - emphasizes low values
        if (normalized_value > 0.0f)
        {
            result = logf(1.0f + normalized_value * 9.0f) / logf(10.0f); // log10(1 + x*9)
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
static int extract_geo_bounds(const char *filename, float *south, float *north, float *west, float *east)
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
    char lat_hemisphere = basename[0];
    if (lat_hemisphere != 'N' && lat_hemisphere != 'S')
        return 0;

    int lat_degrees = (basename[1] - '0') * 10 + (basename[2] - '0');
    if (lat_degrees < 0 || lat_degrees > 90)
        return 0;

    *south = (lat_hemisphere == 'N') ? (float)lat_degrees : -(float)(lat_degrees + 1);
    *north = (lat_hemisphere == 'N') ? (float)(lat_degrees + 1) : -(float)lat_degrees;

    // Parse longitude
    char lon_hemisphere = basename[3];
    if (lon_hemisphere != 'E' && lon_hemisphere != 'W')
        return 0;

    int lon_degrees = (basename[4] - '0') * 100 + (basename[5] - '0') * 10 + (basename[6] - '0');
    if (lon_degrees < 0 || lon_degrees > 180)
        return 0;

    *west = (lon_hemisphere == 'E') ? (float)lon_degrees : -(float)(lon_degrees + 1);
    *east = (lon_hemisphere == 'E') ? (float)(lon_degrees + 1) : -(float)lon_degrees;

    return 1;
}

// HGT samples include both tile edges: row 0 is the north edge, the last row the south edge
static float latitude_at_row(float south, float north, size_t row, size_t height)
{
    if (height < 2)
        return south;

    return north - (north - south) * (float)row / (float)(height - 1);
}

// Points must be sorted by latitude with unique values; clamps to the end values outside their range
static float aridity_at_lat(const aridity_point_t *points, int count, float lat)
{
    if (count <= 0)
        return 0.0f;
    if (lat <= points[0].lat)
        return points[0].value;

    for (int i = 1; i < count; i++)
    {
        if (lat <= points[i].lat)
        {
            float t = (lat - points[i - 1].lat) / (points[i].lat - points[i - 1].lat);
            return points[i - 1].value + t * (points[i].value - points[i - 1].value);
        }
    }
    return points[count - 1].value;
}

// Lowers the vegetation lines and sets the density multiplier in place for the given aridity
static void apply_aridity(vegetation_params_t *params, float aridity)
{
    // Keep at least 100 m between the minimum elevation and the tree line
    float max_shift = params->tree_line - params->min_elevation - 100.0f;
    float shift = ARIDITY_LINE_SHIFT * aridity;
    if (shift > max_shift)
        shift = max_shift;
    if (shift < 0.0f)
        shift = 0.0f;

    params->tree_line -= shift;
    params->bush_line -= shift;
    params->grass_line -= shift;
    params->max_elevation -= shift;

    params->density_scale = 1.0f - ARIDITY_DENSITY_REDUCTION * aridity;
}

// True if all four bilinear source samples are at or below sea level
static int is_flat_sea(const short int *data, int width, int height, float x, float y)
{
    int x1 = (int)x;
    int y1 = (int)y;
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    if (x2 >= width)
        x2 = width - 1;
    if (y2 >= height)
        y2 = height - 1;
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;

    return data[y1 * width + x1] <= 0 && data[y1 * width + x2] <= 0 &&
           data[y2 * width + x1] <= 0 && data[y2 * width + x2] <= 0;
}

// Main function for Procedural Detail Generation
static short int *add_procedural_detail(const short int *originalData, int originalWidth, int originalHeight,
                                      int scale_factor, float detail_intensity, int seed, int srtm_type, int flat_sea)
{

    size_t new_width, new_height;

    // Safe calculation of new dimensions with overflow protection
    if (!safe_multiply_size_t((size_t)originalWidth, (size_t)scale_factor, &new_width) ||
        !safe_multiply_size_t((size_t)originalHeight, (size_t)scale_factor, &new_height))
    {
        fprintf(stderr, "ERROR: Dimension overflow in procedural detail generation: %d×%d scale %d\n",
                originalWidth, originalHeight, scale_factor);
        return NULL;
    }

    // Safe memory allocation with overflow protection
    short int *detailed_data = (short int *)safe_malloc_pixels(new_width, new_height, sizeof(short int));
    if (!detailed_data)
    {
        fprintf(stderr, "ERROR: Cannot allocate memory for detailed heightmap (%zu×%zu pixels)\n",
                new_width, new_height);
        return NULL;
    }

    fprintf(stderr, "INFO: Generating %zu×%zu detailed heightmap (intensity: %.1f)\n",
            new_width, new_height, detail_intensity);

// OpenMP: Parallel processing + SIMD vectorization for optimal performance
// Distribute outer loop across multiple CPU cores
#pragma omp parallel for schedule(dynamic, 64) if (new_height > 1000)
    for (size_t y = 0; y < new_height; y++)
    {
        // Progress reporting (thread-safe, reduced frequency)
        if (y % (new_height / 10) == 0)
        {
#pragma omp critical
            {
                fprintf(stderr, "INFO: Progress: %zu%%\n", (y * 100) / new_height);
            }
        }

// Optimize inner loop for SIMD vectorization
#pragma omp simd aligned(detailed_data : 32) safelen(16)
        for (size_t x = 0; x < new_width; x++)
        {
            // Base height through bilinear interpolation
            float src_x = (float)x / scale_factor;
            float src_y = (float)y / scale_factor;

            if (src_x >= originalWidth - 1)
                src_x = originalWidth - 1.001f;
            if (src_y >= originalHeight - 1)
                src_y = originalHeight - 1.001f;

            short int base_height = bilinear_interpolate(originalData, originalWidth, originalHeight, src_x, src_y);

            // Add procedural detail
            // Different noise octaves for different detail sizes
            float detail_noise_1 = fractal_noise(x * 0.005f, y * 0.005f, 3, 0.5f, 1.0f, seed);     // Large features
            float detail_noise_2 = fractal_noise(x * 0.02f, y * 0.02f, 4, 0.6f, 1.0f, seed + 100); // Medium features
            float detail_noise_3 = fractal_noise(x * 0.08f, y * 0.08f, 2, 0.4f, 1.0f, seed + 200); // Fine details

            float combined_noise = detail_noise_1 * 0.5f + detail_noise_2 * 0.3f + detail_noise_3 * 0.2f;

            // Height-dependent variation (flat areas = less detail, steep areas = more detail)
            float local_slope = calculate_local_slope(originalData, originalWidth, originalHeight, src_x, src_y, srtm_type);
            float slope_multiplier = 0.3f + (local_slope * 0.7f); // 0.3 to 1.0

            // Adjust detail based on terrain type
            float height_factor = get_height_type_factor(base_height);

            // Calculate final detail with sub-pixel precision
            float detail_variation = combined_noise * detail_intensity * slope_multiplier * height_factor;

            // Noise on open sea would create phantom islands
            if (flat_sea && is_flat_sea(originalData, originalWidth, originalHeight, src_x, src_y))
            {
                detail_variation = 0.0f;
            }

            // Maintain float precision for final height (against terracing)
            float final_height = (float)base_height + detail_variation;

            // Ensure values stay within valid range
            if (final_height < 0.0f)
                final_height = 0.0f;
            if (final_height > (float)_MAX_HEIGHT)
                final_height = (float)_MAX_HEIGHT;

            // Rounding instead of truncation for better sub-pixel distribution
            detailed_data[y * new_width + x] = (short int)(final_height + 0.5f);
        }
    }

    return detailed_data;
}

// HELPER FUNCTIONS

// Zeroes 8-connected land components (> 0 m) smaller than minPixels; components touching the tile border are kept.
static int remove_small_islands(int16_t *data, size_t width, size_t height, size_t min_pixels, size_t *removed_islands)
{
    size_t pixel_count;
    *removed_islands = 0;

    if (!safe_multiply_size_t(width, height, &pixel_count) || pixel_count > UINT32_MAX)
    {
        return -1;
    }

    uint8_t *visited = (uint8_t *)calloc(pixel_count, sizeof(uint8_t));
    uint32_t *queue = (uint32_t *)safe_malloc_pixels(width, height, sizeof(uint32_t));
    if (visited == NULL || queue == NULL)
    {
        free(visited);
        free(queue);
        return -1;
    }

    for (size_t start = 0; start < pixel_count; start++)
    {
        if (visited[start] || data[start] <= 0)
        {
            continue;
        }

        size_t head = 0;
        size_t tail = 0;
        int touches_border = 0;

        queue[tail++] = (uint32_t)start;
        visited[start] = 1;

        while (head < tail)
        {
            size_t idx = queue[head++];
            size_t x = idx % width;
            size_t y = idx / width;

            if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
            {
                touches_border = 1;
            }

            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    // Unsigned wrap-around turns negative coordinates into values >= width/height
                    size_t nx = x + (size_t)dx;
                    size_t ny = y + (size_t)dy;
                    if ((dx == 0 && dy == 0) || nx >= width || ny >= height)
                    {
                        continue;
                    }

                    size_t nidx = ny * width + nx;
                    if (visited[nidx] || data[nidx] <= 0)
                    {
                        continue;
                    }

                    visited[nidx] = 1;
                    queue[tail++] = (uint32_t)nidx;
                }
            }
        }

        if (tail < min_pixels && !touches_border)
        {
            for (size_t i = 0; i < tail; i++)
            {
                data[queue[i]] = 0;
            }
            (*removed_islands)++;
        }
    }

    free(visited);
    free(queue);
    return 0;
}

// Extracts filename from path and generates PNG name in current directory
static void generate_output_filename(const char *input_path, char *output_path)
{
    const char *filename = input_path;
    const char *last_slash = strrchr(input_path, '/');

    // If a path separator was found, take the part after it
    if (last_slash != NULL)
    {
        filename = last_slash + 1;
    }

    // Copy the filename and replace the extension
    strcpy(output_path, filename);

    // Find the last .hgt extension and replace it with .png
    char *extension = strrchr(output_path, '.');
    if (extension != NULL && (strcmp(extension, ".hgt") == 0 || strcmp(extension, ".HGT") == 0))
    {
        strcpy(extension, ".png");
    }
    else
    {
        // If no .hgt extension was found, append .png
        strcat(output_path, ".png");
    }
}

// NODATA HANDLING

// Checks if a raw value is a NoData value (after endian conversion)
static int is_nodata_value(int16_t value, int srtm_type)
{
    // Only for SRTM data (30m/90m) NoData detection
    if (srtm_type != SRTM3 && srtm_type != SRTM1)
    {
        return 0;
    }

    // NoData value: -32768 (0x8000 in Big Endian)
    return (value == NODATA_VALUE_BE);
}

// Processes an elevation value: Network-to-Host Byte Order + NoData handling + Clamping
static int16_t process_elevation_value(int16_t raw_value, int srtm_type, int *nodata_count)
{
    // 1. Endian conversion for SRTM data (Big Endian -> Host Endian)
    int16_t host_value = raw_value;
    if (srtm_type == SRTM3 || srtm_type == SRTM1)
    {
        host_value = (int16_t)ntohs((uint16_t)raw_value); // Network (Big Endian) to Host
    }

    // 2. NoData detection after endian conversion
    if (is_nodata_value(host_value, srtm_type))
    {
        if (nodata_count != NULL)
        {
            (*nodata_count)++;
        }
        return NODATA_REPLACEMENT; // Replacement value for NoData
    }

    // 3. Clamping to valid elevation values
    if (host_value < 0)
        host_value = 0;
    if (host_value > _MAX_HEIGHT)
        host_value = _MAX_HEIGHT;

    return host_value;
}

// PARAMETER MANAGEMENT FUNCTIONS

// Initialize default options
static void init_default_options(program_options_t *opts)
{
    opts->scale_factor = DEFAULT_SCALE_FACTOR;
    opts->detail_intensity = DEFAULT_DETAIL_INTENSITY;
    opts->noise_seed = DEFAULT_NOISE_SEED;
    opts->enable_detail = ENABLE_PROCEDURAL_DETAIL;
    opts->verbose = 1; // Default: Verbose output
    opts->show_help = 0;
    opts->show_version = 0;
    opts->num_threads = DEFAULT_NUM_THREADS; // New option for thread count
    opts->output_16bit = 0;                  // Default: 8-bit RGB output
    opts->gamma = 1.0f;                     // Default: No gamma correction
    opts->curve_type = CURVE_LINEAR;         // Default: Linear curve
    opts->min_height = -1;                   // Auto-detection
    opts->max_height = -1;                   // Auto-detection
    opts->metadata_format = METADATA_NONE;   // Default: No metadata
    opts->alpha_nodata = 0;                  // Default: No transparent NoData pixels
    opts->remove_islands = 0;                // Default: Island removal disabled
    opts->aridity_point_count = 0;            // Default: No aridity influence

    // Initialize vegetation mask parameters (disabled by default)
    opts->vegetation.enabled = 0;
    opts->biome = BIOME_ALPINE; // Default biome

    // Initialize default biome parameters (Alpine)
    // Will be overridden if different biome is selected via CLI
    initialize_alpine_biome(&opts->vegetation);
    opts->vegetation.enabled = 0; // Override to disabled by default
}

// Show help text
static void show_help(const char *program_name)
{
    printf("hgt2png v1.5.1 - HGT to PNG Heightmap Converter with Procedural Detail Generation and Multi-Biome Vegetation Masks\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] <input.hgt|filelist.txt>\n\n", program_name);

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
    printf("      --remove-islands <n> Remove isolated land patches smaller than <n> source pixels\n");
    printf("                           (default: off, patches touching the tile border are kept;\n");
    printf("                           also keeps open sea flat during detail generation)\n");
    printf("      --lat-aridity <list> Aridity by latitude as <lat>:<0..1>,... (0=humid, 1=arid),\n");
    printf("                           linearly interpolated, e.g. 43.5:0.1,40:0.5,36:0.9\n");
    printf("      --aridity <f>        Constant aridity 0..1 (replaces --lat-aridity, last one wins)\n");
    printf("  -h, --help               Show this help message\n");
    printf("  -v, --version            Show version information\n\n");

    printf("INPUT:\n");
    printf("  Single HGT file:         %s terrain.hgt\n", program_name);
    printf("  Multiple files (list):   %s filelist.txt\n\n", program_name);

    printf("EXAMPLES:\n");
    printf("  %s N48E011.hgt                          # Standard processing\n", program_name);
    printf("  %s -s 2 -i 25.0 N48E011.hgt             # 2x scale, high detail\n", program_name);
    printf("  %s -t 8 terrain_files.txt               # 8 parallel threads\n", program_name);
    printf("  %s -d -q terrain_files.txt              # No detail, quiet mode\n", program_name);
    printf("  %s --alpha-nodata --metadata json terrain.hgt  # Transparent NoData with metadata\n", program_name);
    printf("  %s --threads 4 --scale-factor 3 --detail-intensity 10  # Parallel + custom\n", program_name);
    printf("  %s --vegetation-mask --biome alpine terrain.hgt  # Generate Alpine vegetation mask\n\n", program_name);

    printf("OUTPUT:\n");
    printf("  Creates PNG files with same basename as input HGT files.\n");
    printf("  Example: N48E011.hgt → N48E011.png\n");
    printf("  With vegetation masks: N48E011.hgt → N48E011.png + N48E011_vegetation_alpine.png\n\n");
}

// Show version
static void show_version(void)
{
    printf("hgt2png v1.5.1\n");
    printf("Professional SRTM HGT to PNG Converter with Multi-Biome Vegetation Masks\n");
    printf("Features: OpenMP parallelization, procedural detail, Alpine + Temperate biomes\n");
    printf("Copyright (c) 2025 Peter Ebel. Licensed under MIT License.\n");
}

// Parses "<lat>:<aridity>,..." into points sorted by latitude; returns -1 on any error without touching the output
static int parse_lat_aridity(const char *arg, aridity_point_t *points, int *count)
{
    aridity_point_t tmp[MAX_ARIDITY_POINTS];
    int n = 0;
    const char *p = arg;

    for (;;)
    {
        char *end;
        double lat = strtod(p, &end);
        if (end == p || *end != ':')
            return -1;
        p = end + 1;

        double value = strtod(p, &end);
        if (end == p)
            return -1;
        p = end;

        if (!(lat >= -90.0 && lat <= 90.0) || !(value >= 0.0 && value <= 1.0) || n >= MAX_ARIDITY_POINTS)
            return -1;

        int i = n;
        for (int k = 0; k < n; k++)
        {
            if (tmp[k].lat == (float)lat)
                return -1;
        }
        while (i > 0 && tmp[i - 1].lat > (float)lat)
        {
            tmp[i] = tmp[i - 1];
            i--;
        }
        tmp[i].lat = (float)lat;
        tmp[i].value = (float)value;
        n++;

        if (*p == '\0')
            break;
        if (*p != ',')
            return -1;
        p++;
    }

    memcpy(points, tmp, (size_t)n * sizeof(aridity_point_t));
    *count = n;
    return 0;
}

// Parse command line arguments
static int parse_arguments(int argc, char *argv[], program_options_t *opts, char **input_file)
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
        {"remove-islands", required_argument, 0, 'I'},
        {"lat-aridity", required_argument, 0, 'L'},
        {"aridity", required_argument, 0, 'Y'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    while ((c = getopt_long(argc, argv, "s:i:r:t:dq6ag:c:m:M:x:VB:hv", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 's':
            opts->scale_factor = atoi(optarg);
            if (opts->scale_factor < 1 || opts->scale_factor > 10)
            {
                fprintf(stderr, "Error: Scale factor must be between 1 and 10\n");
                return 1;
            }
            break;

        case 'i':
            opts->detail_intensity = atof(optarg);
            if (opts->detail_intensity < 0.0f || opts->detail_intensity > 100.0f)
            {
                fprintf(stderr, "Error: Detail intensity must be between 0.0 and 100.0\n");
                return 1;
            }
            break;

        case 'r':
            opts->noise_seed = atoi(optarg);
            break;

        case 't':
            opts->num_threads = atoi(optarg);
            if (opts->num_threads < 1 || opts->num_threads > 16)
            {
                fprintf(stderr, "Error: Number of threads must be between 1 and 16\n");
                return 1;
            }
            break;

        case 'd':
            opts->enable_detail = 0;
            break;

        case 'q':
            opts->verbose = 0;
            break;

        case '6':
            opts->output_16bit = 1;
            break;

        case 'a':
            opts->alpha_nodata = 1;
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
                opts->curve_type = CURVE_LINEAR;
            }
            else if (strcmp(optarg, "log") == 0)
            {
                opts->curve_type = CURVE_LOG;
            }
            else
            {
                fprintf(stderr, "Error: Curve type must be 'linear' or 'log'\n");
                return 1;
            }
            break;

        case 'm':
            opts->min_height = atoi(optarg);
            break;

        case 'M':
            opts->max_height = atoi(optarg);
            if (opts->min_height != -1 && opts->max_height != -1 && opts->max_height <= opts->min_height)
            {
                fprintf(stderr, "Error: max-height must be greater than min-height\n");
                return 1;
            }
            break;

        case 'x':
            if (strcmp(optarg, "json") == 0)
            {
                opts->metadata_format = METADATA_JSON;
            }
            else if (strcmp(optarg, "txt") == 0)
            {
                opts->metadata_format = METADATA_TXT;
            }
            else if (strcmp(optarg, "none") == 0)
            {
                opts->metadata_format = METADATA_NONE;
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
                initialize_alpine_biome(&opts->vegetation);
            }
            else if (strcmp(optarg, "temperate") == 0)
            {
                opts->biome = BIOME_TEMPERATE;
                initialize_temperate_biome(&opts->vegetation);
            }
            else if (strcmp(optarg, "tropical") == 0)
            {
                opts->biome = BIOME_TROPICAL;
                fprintf(stderr, "Warning: Tropical biome not yet implemented, using Alpine defaults\n");
                initialize_alpine_biome(&opts->vegetation);
            }
            else if (strcmp(optarg, "desert") == 0)
            {
                opts->biome = BIOME_DESERT;
                fprintf(stderr, "Warning: Desert biome not yet implemented, using Alpine defaults\n");
                initialize_alpine_biome(&opts->vegetation);
            }
            else if (strcmp(optarg, "arctic") == 0)
            {
                opts->biome = BIOME_ARCTIC;
                fprintf(stderr, "Warning: Arctic biome not yet implemented, using Alpine defaults\n");
                initialize_alpine_biome(&opts->vegetation);
            }
            else
            {
                fprintf(stderr, "Error: Biome must be 'alpine', 'temperate', 'tropical', 'desert', or 'arctic'\n");
                return 1;
            }
            break;

        case 'I':
        {
            long min_pixels = atol(optarg);
            if (min_pixels < 1 || min_pixels > 100000000L)
            {
                fprintf(stderr, "Error: remove-islands must be between 1 and 100000000 pixels\n");
                return 1;
            }
            opts->remove_islands = (size_t)min_pixels;
            break;
        }

        case 'L':
            if (parse_lat_aridity(optarg, opts->aridity_points, &opts->aridity_point_count) != 0)
            {
                fprintf(stderr, "Error: lat-aridity must be '<lat>:<aridity>,...' with lat -90..90, aridity 0..1, "
                                "unique latitudes and at most %d points\n", MAX_ARIDITY_POINTS);
                return 1;
            }
            break;

        case 'Y':
        {
            char *end;
            double aridity = strtod(optarg, &end);
            if (end == optarg || *end != '\0' || !(aridity >= 0.0 && aridity <= 1.0))
            {
                fprintf(stderr, "Error: aridity must be a number between 0.0 and 1.0\n");
                return 1;
            }
            opts->aridity_points[0].lat = 0.0f;
            opts->aridity_points[0].value = (float)aridity;
            opts->aridity_point_count = 1;
            break;
        }

        case 'h':
            opts->show_help = 1;
            return 0;

        case 'v':
            opts->show_version = 1;
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
        *input_file = argv[optind];
    }

    return 0;
}

// Writes metadata sidecar file for precise Blender scaling
static void write_metadata_file(const char *png_filename, const program_options_t *opts, const struct tag_file_info_srtm *fi,
                              int effective_min_height, int effective_max_height)
{

    if (opts->metadata_format == METADATA_NONE)
        return;

    // Generate metadata filename
    char metadata_filename[512];
    strcpy(metadata_filename, png_filename);

    // Replace PNG extension
    char *dot = strrchr(metadata_filename, '.');
    if (dot)
    {
        if (opts->metadata_format == METADATA_JSON)
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
        if (opts->metadata_format == METADATA_JSON)
        {
            strcat(metadata_filename, ".json");
        }
        else
        {
            strcat(metadata_filename, ".txt");
        }
    }

    // Extract geographic bounds
    float south, north, west, east;
    int has_geo_bounds = extract_geo_bounds(fi->filename, &south, &north, &west, &east);

    // Calculate pixel pitch (meters per pixel)
    float pixel_pitch_meters = 30.0f; // Default SRTM-1
    if (fi->srtm_type == SRTM1)
    {
        pixel_pitch_meters = 90.0f; // SRTM-3
    }
    else if (fi->srtm_type == SRTM3)
    {
        pixel_pitch_meters = 30.0f; // SRTM-1
    }

    // With procedural detail: divide pixel pitch by scale factor
    if (opts->enable_detail && opts->scale_factor > 1)
    {
        pixel_pitch_meters /= (float)opts->scale_factor;
    }

    FILE *metadata_file = fopen(metadata_filename, "w");
    if (!metadata_file)
    {
        fprintf(stderr, "WARNING: Could not write metadata file %s\n", metadata_filename);
        return;
    }

    if (opts->metadata_format == METADATA_JSON)
    {
        // JSON format for machine processing
        fprintf(metadata_file, "{\n");
        fprintf(metadata_file, "  \"source_file\": \"%s\",\n", fi->filename);
        fprintf(metadata_file, "  \"png_file\": \"%s\",\n", png_filename);
        fprintf(metadata_file, "  \"dimensions\": {\n");
        fprintf(metadata_file, "    \"width\": %zu,\n", fi->width);
        fprintf(metadata_file, "    \"height\": %zu\n", fi->height);
        fprintf(metadata_file, "  },\n");
        fprintf(metadata_file, "  \"elevation\": {\n");
        fprintf(metadata_file, "    \"min_meters\": %d,\n", effective_min_height);
        fprintf(metadata_file, "    \"max_meters\": %d,\n", effective_max_height);
        fprintf(metadata_file, "    \"range_meters\": %d,\n", effective_max_height - effective_min_height);
        fprintf(metadata_file, "    \"original_min\": %d,\n", fi->min_elevation);
        fprintf(metadata_file, "    \"original_max\": %d\n", fi->max_elevation);
        fprintf(metadata_file, "  },\n");
        fprintf(metadata_file, "  \"scaling\": {\n");
        fprintf(metadata_file, "    \"pixel_pitch_meters\": %.6f,\n", pixel_pitch_meters);
        fprintf(metadata_file, "    \"scale_factor\": %d,\n", opts->scale_factor);
        fprintf(metadata_file, "    \"world_size_meters\": {\n");
        fprintf(metadata_file, "      \"width\": %.2f,\n", fi->width * pixel_pitch_meters);
        fprintf(metadata_file, "      \"height\": %.2f\n", fi->height * pixel_pitch_meters);
        fprintf(metadata_file, "    }\n");
        fprintf(metadata_file, "  }");

        if (has_geo_bounds)
        {
            fprintf(metadata_file, ",\n");
            fprintf(metadata_file, "  \"geographic\": {\n");
            fprintf(metadata_file, "    \"bounds\": {\n");
            fprintf(metadata_file, "      \"south\": %.6f,\n", south);
            fprintf(metadata_file, "      \"north\": %.6f,\n", north);
            fprintf(metadata_file, "      \"west\": %.6f,\n", west);
            fprintf(metadata_file, "      \"east\": %.6f\n", east);
            fprintf(metadata_file, "    },\n");
            fprintf(metadata_file, "    \"center\": {\n");
            fprintf(metadata_file, "      \"latitude\": %.6f,\n", (south + north) / 2.0f);
            fprintf(metadata_file, "      \"longitude\": %.6f\n", (west + east) / 2.0f);
            fprintf(metadata_file, "    }\n");
            fprintf(metadata_file, "  }\n");
        }
        else
        {
            fprintf(metadata_file, "\n");
        }

        fprintf(metadata_file, "}\n");
    }
    else
    {
        // TXT format for human readability
        fprintf(metadata_file, "HGT2PNG Metadata\n");
        fprintf(metadata_file, "================\n\n");
        fprintf(metadata_file, "Source File: %s\n", fi->filename);
        fprintf(metadata_file, "PNG File: %s\n", png_filename);
        fprintf(metadata_file, "\nImage Dimensions:\n");
        fprintf(metadata_file, "  Width:  %zu pixels\n", fi->width);
        fprintf(metadata_file, "  Height: %zu pixels\n", fi->height);
        fprintf(metadata_file, "\nElevation Data:\n");
        fprintf(metadata_file, "  Effective Range: %d - %d meters\n", effective_min_height, effective_max_height);
        fprintf(metadata_file, "  Original Range:  %d - %d meters\n", fi->min_elevation, fi->max_elevation);
        fprintf(metadata_file, "  Total Range:     %d meters\n", effective_max_height - effective_min_height);
        fprintf(metadata_file, "\nBlender Scaling (Displacement Setup):\n");
        fprintf(metadata_file, "  Pixel Pitch: %.6f meters/pixel\n", pixel_pitch_meters);
        fprintf(metadata_file, "  World Size:  %.2f x %.2f meters\n",
                fi->width * pixel_pitch_meters, fi->height * pixel_pitch_meters);
        fprintf(metadata_file, "  Scale Factor: %d\n", opts->scale_factor);

        if (has_geo_bounds)
        {
            fprintf(metadata_file, "\nGeographic Coordinates:\n");
            fprintf(metadata_file, "  Bounds: %.6f°N to %.6f°N, %.6f°E to %.6f°E\n", south, north, west, east);
            fprintf(metadata_file, "  Center: %.6f°N, %.6f°E\n", (south + north) / 2.0f, (west + east) / 2.0f);
        }
    }

    fclose(metadata_file);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Wrote metadata file %s\n", metadata_filename);
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
static void initialize_alpine_biome(vegetation_params_t *params)
{
    params->enabled = 1;
    params->min_elevation = ALPINE_MIN_ELEVATION;     // 700m - montane forest zone
    params->max_elevation = ALPINE_MAX_ELEVATION;     // 2400m - above tree line
    params->max_slope = ALPINE_MAX_SLOPE;             // 45° maximum for vegetation
    params->tree_line = ALPINE_TREE_LINE;             // 1800m - spruce/larch limit
    params->bush_line = ALPINE_BUSH_LINE;             // 2200m - dwarf pine/rhododendron
    params->grass_line = ALPINE_GRASS_LINE;           // 2400m - alpine meadows
    params->aspect_modifier = ALPINE_ASPECT_MODIFIER; // 0.2 - moderate aspect influence
    params->drainage_bonus = ALPINE_DRAINAGE_BONUS;   // 0.3 - valleys have more vegetation
    params->density_scale = 1.0f;
}

/**
 * Initialize Temperate biome parameters with realistic values
 * Based on Central European lowlands and mid-elevation zones
 * Typical regions: Germany, France, UK, Eastern USA, etc.
 */
static void initialize_temperate_biome(vegetation_params_t *params)
{
    params->enabled = 1;
    params->min_elevation = TEMPERATE_MIN_ELEVATION;     // 0m - sea level
    params->max_elevation = TEMPERATE_MAX_ELEVATION;     // 1500m - upper limit
    params->max_slope = TEMPERATE_MAX_SLOPE;             // 50° maximum for vegetation
    params->tree_line = TEMPERATE_TREE_LINE;             // 1200m - deciduous forest limit
    params->bush_line = TEMPERATE_BUSH_LINE;             // 1400m - shrub zone
    params->grass_line = TEMPERATE_GRASS_LINE;           // 1500m - grassland limit
    params->aspect_modifier = TEMPERATE_ASPECT_MODIFIER; // 0.15 - less aspect influence
    params->drainage_bonus = TEMPERATE_DRAINAGE_BONUS;   // 0.4 - strong drainage effect
    params->density_scale = 1.0f;
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
                                                   float drainage, const vegetation_params_t *params)
{
    if (!params->enabled)
        return 0;

    // Base elevation factor - MORE REALISTIC DISTRIBUTION
    float elevation_factor = 0.0f;

    if (elevation < params->min_elevation)
    {
        elevation_factor = 0.0f; // Below vegetation zone
    }
    else if (elevation <= params->tree_line)
    {
        // Montane forest zone (700-1800m) - dense vegetation
        float range = params->tree_line - params->min_elevation;
        float position = (elevation - params->min_elevation) / range;
        elevation_factor = 0.6f + position * 0.4f; // 60-100% density (peak at tree line)
    }
    else if (elevation <= params->bush_line)
    {
        // Subalpine zone (1800-2200m) - shrubs and dwarf trees
        float range = params->bush_line - params->tree_line;
        float position = (elevation - params->tree_line) / range;
        elevation_factor = 0.8f - position * 0.5f; // 80% down to 30% density
    }
    else if (elevation <= params->grass_line)
    {
        // Alpine zone (2200-2400m) - alpine meadows and cushion plants
        float range = params->grass_line - params->bush_line;
        float position = (elevation - params->bush_line) / range;
        elevation_factor = 0.3f - position * 0.25f; // 30% down to 5% density
    }
    else
    {
        elevation_factor = 0.0f; // Above vegetation limit (nival zone)
    }

    // Slope factor - steeper slopes have less vegetation - MORE RESTRICTIVE
    float slope_factor = 1.0f;
    if (slope > params->max_slope)
    {
        slope_factor = 0.0f; // Too steep for vegetation
    }
    else if (slope > 25.0f)  // Start reducing at 25° instead of 30°
    {
        // More aggressive slope reduction: 25°-45° range
        float slope_range = params->max_slope - 25.0f;
        float slope_excess = slope - 25.0f;
        slope_factor = 1.0f - (slope_excess / slope_range) * 0.9f; // 90% reduction, not 80%
    }
    else if (slope > 15.0f)  // Even gentle slopes reduce vegetation
    {
        // Gentle slope reduction: 15°-25° range  
        slope_factor = 1.0f - ((slope - 15.0f) / 10.0f) * 0.3f; // 30% reduction max
    }

    // Aspect factor - South faces are drier, North faces moister - REFINED
    float aspect_factor = 1.0f;
    if (aspect >= 135.0f && aspect <= 225.0f)
    {
        // South-facing slopes (135°-225°) - significantly drier at high elevation
        float elevation_factor_aspect = elevation > 1500.0f ? 1.5f : 1.0f; // More effect at higher elevation
        aspect_factor = 1.0f - (params->aspect_modifier * elevation_factor_aspect);
    }
    else if (aspect >= 315.0f || aspect <= 45.0f)
    {
        // North-facing slopes (315°-45°) - moister, better for vegetation
        aspect_factor = 1.0f + params->aspect_modifier;
    }

    // Drainage factor bonus for valleys - REDUCED EFFECT
    float drainage_factor = 1.0f + (drainage * params->drainage_bonus);

    // Combine all factors - ADD MINIMUM THRESHOLD
    float final_density = elevation_factor * slope_factor * aspect_factor * drainage_factor * params->density_scale;

    // Apply minimum threshold - no vegetation below 5% density
    if (final_density < 0.05f)
        final_density = 0.0f;

    // Clamp to 0-1 range
    if (final_density < 0.0f)
        final_density = 0.0f;
    if (final_density > 1.0f)
        final_density = 1.0f;

    // Convert to 8-bit grayscale (0-255)
    return (uint8_t)(final_density * 255.0f);
}

/**
 * Calculate vegetation density for Temperate biome (0-255 grayscale)
 * Implements realistic temperate zone vegetation distribution patterns
 * Suitable for: Central Europe, Eastern USA, UK, temperate Asia
 */
static uint8_t calculate_vegetation_density_temperate(float elevation, float slope, float aspect,
                                                      float drainage, const vegetation_params_t *params)
{
    if (!params->enabled)
        return 0;

    // Base elevation factor - TEMPERATE DISTRIBUTION
    // Temperate zones have dense vegetation at low elevations, decreasing with height
    float elevation_factor = 0.0f;

    if (elevation < params->min_elevation)
    {
        elevation_factor = 0.0f; // Below sea level (rare)
    }
    else if (elevation <= params->tree_line)
    {
        // Lowland to mid-elevation forest zone (0-1200m) - very dense vegetation
        // Peak density at mid-elevations (400-800m), slightly less at very low elevations
        float range = params->tree_line - params->min_elevation;
        float position = (elevation - params->min_elevation) / range;
        
        // Parabolic distribution: peak at 0.5 position (~600m)
        float optimal_position = 0.5f;
        float distance_from_optimal = fabsf(position - optimal_position);
        elevation_factor = 0.95f - distance_from_optimal * 0.25f; // 70-95% density
    }
    else if (elevation <= params->bush_line)
    {
        // Transition zone (1200-1400m) - mixed forest/shrubs
        float range = params->bush_line - params->tree_line;
        float position = (elevation - params->tree_line) / range;
        elevation_factor = 0.7f - position * 0.35f; // 70% down to 35% density
    }
    else if (elevation <= params->grass_line)
    {
        // High grassland zone (1400-1500m) - meadows and sparse vegetation
        float range = params->grass_line - params->bush_line;
        float position = (elevation - params->bush_line) / range;
        elevation_factor = 0.35f - position * 0.25f; // 35% down to 10% density
    }
    else
    {
        elevation_factor = 0.0f; // Above temperate vegetation limit
    }

    // Slope factor - temperate vegetation more tolerant of slopes
    float slope_factor = 1.0f;
    if (slope > params->max_slope)
    {
        slope_factor = 0.0f; // Too steep for vegetation
    }
    else if (slope > 30.0f)  // Start reducing at 30° (more tolerant than alpine)
    {
        // Gradual slope reduction: 30°-50° range
        float slope_range = params->max_slope - 30.0f;
        float slope_excess = slope - 30.0f;
        slope_factor = 1.0f - (slope_excess / slope_range) * 0.85f; // 85% reduction
    }
    else if (slope > 20.0f)  // Gentle slope reduction
    {
        // Very gentle reduction: 20°-30° range  
        slope_factor = 1.0f - ((slope - 20.0f) / 10.0f) * 0.2f; // 20% reduction max
    }

    // Aspect factor - less important in temperate zones due to higher moisture
    float aspect_factor = 1.0f;
    if (aspect >= 135.0f && aspect <= 225.0f)
    {
        // South-facing slopes (135°-225°) - slightly drier
        aspect_factor = 1.0f - params->aspect_modifier * 0.8f;
    }
    else if (aspect >= 315.0f || aspect <= 45.0f)
    {
        // North-facing slopes (315°-45°) - slightly moister
        aspect_factor = 1.0f + params->aspect_modifier * 0.6f;
    }

    // Drainage factor - VERY important in temperate zones (wet lowlands vs dry ridges)
    float drainage_factor = 1.0f + (drainage * params->drainage_bonus);

    // Combine all factors
    float final_density = elevation_factor * slope_factor * aspect_factor * drainage_factor * params->density_scale;

    // Apply minimum threshold - no vegetation below 5% density
    if (final_density < 0.05f)
        final_density = 0.0f;

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
static int generate_vegetation_mask(const struct tag_file_info_srtm *fi, const int16_t *elevation_data,
                                    const program_options_t *opts)
{
    if (!opts->vegetation.enabled)
    {
        return 0; // Vegetation masks disabled
    }

    // Create vegetation mask filename
    char mask_filename[_MAX_PATH];
    char base_name[_MAX_PATH];

    // Extract base filename without directory and extension (the mask goes to the current directory like the heightmap)
    const char *name_start = strrchr(fi->filename, '/');
    name_start = name_start ? name_start + 1 : fi->filename;

    const char *last_dot = strrchr(name_start, '.');
    if (last_dot)
    {
        size_t base_len = last_dot - name_start;
        strncpy(base_name, name_start, base_len);
        base_name[base_len] = '\0';
    }
    else
    {
        strcpy(base_name, name_start);
    }

    // Create mask filename with biome suffix
    const char *biome_names[] = {"alpine", "temperate", "tropical", "desert", "arctic"};
    snprintf(mask_filename, sizeof(mask_filename), "%s_vegetation_%s.png",
             base_name, biome_names[opts->biome]);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Generating vegetation mask: %s\n", mask_filename);
        fprintf(stderr, "INFO: Biome: %s, Elevation range: %.0f-%.0f meters\n",
                biome_names[opts->biome], opts->vegetation.min_elevation, opts->vegetation.max_elevation);
    }

    float tile_south = 0.0f, tile_north = 0.0f, tile_west, tile_east;
    int has_geo_bounds = extract_geo_bounds(fi->filename, &tile_south, &tile_north, &tile_west, &tile_east);
    int use_aridity = opts->aridity_point_count > 0 && has_geo_bounds;

    if (opts->aridity_point_count > 0 && !has_geo_bounds)
    {
        fprintf(stderr, "WARNING: Cannot derive latitude from filename %s, aridity ignored\n", fi->filename);
    }

    if (opts->verbose && has_geo_bounds)
    {
        fprintf(stderr, "INFO: Latitude range: %.4f (first row) to %.4f (last row)\n",
                latitude_at_row(tile_south, tile_north, 0, fi->height),
                latitude_at_row(tile_south, tile_north, fi->height - 1, fi->height));
        if (use_aridity)
        {
            fprintf(stderr, "INFO: Aridity: %.3f (first row) to %.3f (last row)\n",
                    aridity_at_lat(opts->aridity_points, opts->aridity_point_count,
                                 latitude_at_row(tile_south, tile_north, 0, fi->height)),
                    aridity_at_lat(opts->aridity_points, opts->aridity_point_count,
                                 latitude_at_row(tile_south, tile_north, fi->height - 1, fi->height)));
        }
    }

    // Determine pixel pitch for slope calculations
    float pixel_pitch_meters = 90.0f; // Default SRTM-3
    if (fi->srtm_type == SRTM3)
    {
        pixel_pitch_meters = 30.0f; // SRTM-1
    }

    // The whole mask is computed first so threads never share a PNG row buffer and rows are written in order
    uint8_t *mask_data = (uint8_t *)safe_malloc_pixels(fi->width, fi->height, sizeof(uint8_t));
    if (!mask_data)
    {
        fprintf(stderr, "ERROR: Cannot allocate vegetation mask buffer\n");
        return -1;
    }

    // Create PNG file
    FILE *fp = fopen(mask_filename, "wb");
    if (!fp)
    {
        free(mask_data);
        fprintf(stderr, "ERROR: Cannot create vegetation mask file %s\n", mask_filename);
        return -1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        free(mask_data);
        fclose(fp);
        fprintf(stderr, "ERROR: Cannot create PNG write structure\n");
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        free(mask_data);
        fclose(fp);
        fprintf(stderr, "ERROR: Cannot create PNG info structure\n");
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        free(mask_data);
        fclose(fp);
        fprintf(stderr, "ERROR: PNG write error\n");
        return -1;
    }

    png_init_io(png_ptr, fp);

    // Set PNG headers for grayscale
    png_set_IHDR(png_ptr, info_ptr, fi->width, fi->height, 8,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Progress tracking
    size_t progress_step = fi->height / 20; // Update every 5%
    if (progress_step == 0)
        progress_step = 1;

// Process each row
#pragma omp parallel for schedule(dynamic, 1) if (opts->num_threads > 1)
    for (size_t y = 0; y < fi->height; y++)
    {
        // Progress indicator
        if (opts->verbose && (y % progress_step == 0))
        {
#pragma omp critical
            {
                fprintf(stderr, "\rProgress: %zu%% ", (y * 100) / fi->height);
                fflush(stderr);
            }
        }

        // Aridity depends only on the row, so the adjusted parameters are computed once per row
        vegetation_params_t row_params = opts->vegetation;
        if (use_aridity)
        {
            float aridity = aridity_at_lat(opts->aridity_points, opts->aridity_point_count,
                                         latitude_at_row(tile_south, tile_north, y, fi->height));
            apply_aridity(&row_params, aridity);
        }

        // Process each pixel in row
        for (size_t x = 0; x < fi->width; x++)
        {
            size_t index = y * fi->width + x;
            int16_t elevation = elevation_data[index];

            uint8_t vegetation_density = 0;

            // Skip NoData pixels
            if (elevation != NODATA_REPLACEMENT)
            {
                // Calculate terrain analysis factors
                float slope = calculate_slope_angle(elevation_data, fi->width, fi->height,
                                                    x, y, pixel_pitch_meters);
                float aspect = calculate_aspect_angle(elevation_data, fi->width, fi->height,
                                                      x, y, pixel_pitch_meters);
                float drainage = calculate_drainage_factor(elevation_data, fi->width, fi->height,
                                                           x, y, 2);

                // Calculate vegetation density based on biome
                switch (opts->biome)
                {
                case BIOME_ALPINE:
                    vegetation_density = calculate_vegetation_density_alpine(
                        (float)elevation, slope, aspect, drainage, &row_params);
                    break;
                case BIOME_TEMPERATE:
                    vegetation_density = calculate_vegetation_density_temperate(
                        (float)elevation, slope, aspect, drainage, &row_params);
                    break;
                // TODO: Add other biomes (tropical, desert, arctic)
                default:
                    vegetation_density = calculate_vegetation_density_alpine(
                        (float)elevation, slope, aspect, drainage, &row_params);
                    break;
                }
            }

            mask_data[index] = vegetation_density;
        }
    }

    if (opts->verbose)
    {
        fprintf(stderr, "\rProgress: 100%% \n");
    }

    for (size_t y = 0; y < fi->height; y++)
    {
        png_write_row(png_ptr, mask_data + y * fi->width);
    }

    // Finalize PNG
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(mask_data);
    fclose(fp);

    if (opts->verbose)
    {
        fprintf(stderr, "INFO: Vegetation mask saved: %s\n", mask_filename);
    }

    return 0;
}