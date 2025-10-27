# hgt2png - HGT to PNG Heightmap Converter

High-performance converter for SRTM HGT terrain data to PNG displacement maps with procedural detail generation and parallel processing support.

## Features

- **Procedural Detail Generation**: Enhance resolution 2x-10x with realistic terrain details
- **Parallel Processing**: Multi-threaded batch processing for maximum performance  
- **NoData Handling**: Proper SRTM NoData value detection and processing
- **Multiple Formats**: Support for 30m and 90m SRTM resolution
- **Professional Output**: Optimized PNG displacement maps for Blender 3D workflows

## Build Instructions

### Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install libpng-dev pkg-config build-essential

# Fedora/RHEL
sudo dnf install libpng-devel pkgconfig gcc

# Arch Linux  
sudo pacman -S libpng pkg-config gcc
```

### Compilation

**Recommended (using Makefile):**
```bash
make                # Build optimized version
make debug          # Build with debug symbols
make install        # Install to /usr/local/bin
make check-deps     # Verify dependencies
```

**Manual compilation:**
```bash
# Using pkg-config (portable)
gcc -std=gnu99 -O2 $(pkg-config --cflags libpng) -o hgt2png hgt2png.c $(pkg-config --libs libpng) -lm -pthread

# Fallback (manual linking)
gcc -std=gnu99 -O2 -o hgt2png hgt2png.c -lpng -lm -pthread
```

## Usage

```bash
# Single file processing
hgt2png N48E011.hgt

# Custom parameters
hgt2png --scale-factor 4 --detail-intensity 25 --threads 8 terrain.hgt

# Batch processing
hgt2png --threads 4 filelist.txt

# Disable enhancement (original resolution)
hgt2png --disable-detail terrain.hgt
```
