# 🏔️ HGT2PNG v1.1.0 - Professional Heightmap Converter

High-performance, OpenMP-optimized converter for SRTM HGT terrain data to PNG displacement maps with advanced procedural detail generation and professional 3D workflow integration.

[![Performance](https://img.shields.io/badge/Performance-6x_faster-green.svg)](##performance)
[![OpenMP](https://img.shields.io/badge/OpenMP-Optimized-blue.svg)](#features)
[![16-bit](https://img.shields.io/badge/PNG-16--bit_support-orange.svg)](#output-formats)

## ✨ Key Features

### 🚀 **Performance & Quality**
- **OpenMP Parallelization**: 6x performance boost using multi-core CPUs + SIMD vectorization
- **Procedural Detail Generation**: Enhance resolution 2x-4x with realistic terrain details using fractal noise
- **Memory-Safe**: Overflow protection, const-correctness, professional C development standards
- **Cache-Optimized**: 32-byte memory alignment for AVX2 vector instructions

### 🎨 **Professional Output Formats**
- **16-Bit PNG**: Full precision displacement maps (65,535 height levels vs 255)
- **Alpha Transparency**: NoData pixels become transparent for seamless compositing
- **Gamma Correction**: Visual curve mapping for realistic or technical visualization
- **Curve Types**: Linear (scientific) or logarithmic (emphasize lowlands)

### 🔧 **3D Workflow Integration**
- **Blender Metadata**: JSON sidecar files with elevation, coordinates, pixel pitch
- **Game Engines**: Unity/Unreal-compatible heightmaps with scaling information
- **GIS Integration**: Geographic bounds and projection data for scientific use

### ⚡ **Batch Processing**
- **Multi-Threading**: Process multiple HGT files in parallel using all CPU cores
- **Smart Scheduling**: Dynamic load balancing for optimal resource utilization
- **Progress Tracking**: Real-time progress indication for large-scale processing

## 📊 Performance Comparison

| Version | Resolution | Processing Time | CPU Usage | Memory |
|---------|------------|-----------------|-----------|---------|
| **v1.1.0 (OpenMP)** | 10803×10803 | **14.8s** | **163% (multi-core)** | 280MB |
| v1.0.x (single-thread) | 10803×10803 | ~90s | 25% (1 core) | 250MB |

*Test system: 8-core CPU, 3601×3601 SRTM-1 input, scale factor 3*

## 🛠️ Installation

### Quick Start (Ubuntu/Debian/Linux Mint)

```bash
# Install dependencies
sudo apt update && sudo apt install build-essential libpng-dev pkg-config

# Clone and build
git clone https://github.com/PeterEbel/hgt2png.git
cd hgt2png
make check-deps
make

# Test installation  
./hgt2png --help
```

### Alternative Distributions

<details>
<summary>Click to expand for Fedora, Arch, etc.</summary>

**Fedora/RHEL/CentOS:**
```bash
sudo dnf install gcc libpng-devel pkgconfig
make
```

**Arch Linux:**
```bash  
sudo pacman -S base-devel libpng pkg-config
make
```

**From Source (any Linux):**
```bash
gcc -std=gnu99 -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp -mavx2
```
</details>

## 🎯 Usage Examples

### Basic Conversion
```bash
# Standard high-quality output
./hgt2png N49E004.hgt
# → Creates N49E004.png (10803×10803, 3x enhanced resolution)
```

### Professional 3D Workflows  
```bash
# Blender displacement maps with metadata
./hgt2png --16bit --alpha-nodata --metadata json N49E004.hgt
# → Creates N49E004.png (16-bit) + N49E004.json (coordinates, scaling)

# Game engine heightmaps
./hgt2png --scale-factor 2 --detail-intensity 20 --curve linear terrain.hgt

# Scientific visualization (no enhancement)
./hgt2png --16bit --disable-detail --curve linear --gamma 1.0 data.hgt
```

### Batch Processing
```bash
# Process multiple files with 8 threads
echo -e "N49E004.hgt\nN49E005.hgt\nN50E004.hgt" > filelist.txt
./hgt2png --threads 8 --quiet filelist.txt

# High-performance pipeline
OMP_NUM_THREADS=8 ./hgt2png --scale-factor 4 *.hgt
```

### Advanced Parameters
```bash
# Custom height range and gamma correction
./hgt2png --min-height 0 --max-height 2000 --gamma 2.2 mountain.hgt

# Artistic/fantasy landscapes  
./hgt2png --scale-factor 4 --detail-intensity 35 --noise-seed 42 fantasy.hgt

# Technical analysis (linear, no gamma)
./hgt2png --16bit --curve linear --gamma 1.0 --metadata json survey.hgt
```

## 📖 Documentation

- **[Options.md](Options.md)** - Complete parameter reference with 3D workflow examples
- **[INSTALL.md](INSTALL.md)** - Detailed installation guide for all platforms  
- **Built-in help:** `./hgt2png --help`

## 🔧 Command Reference

| Option | Description | Example |
|--------|-------------|---------|
| `-s, --scale-factor <n>` | Resolution enhancement (1-4) | `-s 3` |
| `--16bit` | 16-bit PNG output | `--16bit` |
| `--alpha-nodata` | Transparent NoData pixels | `--alpha-nodata` |
| `-i, --detail-intensity <f>` | Detail strength in meters | `-i 20.0` |
| `-t, --threads <n>` | CPU cores to use (1-16) | `-t 8` |
| `-g, --gamma <f>` | Gamma correction (0.1-10.0) | `-g 2.2` |
| `-c, --curve <type>` | Mapping curve (linear\|log) | `-c log` |
| `-x, --metadata <format>` | Export metadata (json\|txt) | `-x json` |
| `-d, --disable-detail` | Original resolution only | `-d` |

## 🎨 Output Formats & Quality

### Standard 8-bit RGB
- **Use case**: Previews, web display, general use
- **Range**: 0-255 brightness levels  
- **File size**: ~25MB for 10803×10803

### Professional 16-bit Grayscale
- **Use case**: Blender, Maya, scientific analysis
- **Range**: 0-65,535 height levels (256x more precision)
- **File size**: ~50MB for 10803×10803

### 16-bit + Alpha Transparency
- **Use case**: Island terrain, tile systems, compositing
- **Features**: NoData pixels transparent, seamless overlays
- **File size**: ~75MB for 10803×10803

## 🧬 Technical Architecture

### OpenMP Optimization
- **Parallel Processing**: Distributes pixel rows across CPU cores
- **SIMD Vectorization**: AVX2 instructions process 8 pixels simultaneously  
- **Memory Alignment**: 32-byte aligned allocations for optimal cache performance
- **Dynamic Scheduling**: Intelligent work distribution for load balancing

### Procedural Detail Generation
- **Multi-Octave Noise**: 3 frequency layers for realistic terrain features
- **Slope-Aware**: More detail on steep terrain, less on flat areas
- **Height-Dependent**: Adapts detail intensity based on elevation
- **Deterministic**: Same seed produces identical results

### Memory Management
- **Overflow Protection**: Safe multiplication with size_t validation
- **RAII-Style**: Automatic cleanup on error paths
- **Const-Correctness**: Read-only parameters marked const
- **Thread-Safe**: No shared mutable state between worker threads

## 🔬 Scientific Applications

### GIS Integration
```json
// Generated metadata (example)
{
  "elevation_range": {"min_meters": 54, "max_meters": 502},
  "spatial_info": {
    "pixel_pitch_meters": 30.0,
    "geographic_bounds": {
      "south": 49.0, "north": 50.0,
      "west": 4.0, "east": 5.0
    }
  }
}
```

### Research Use Cases
- **Climate Modeling**: High-resolution terrain for atmospheric simulations
- **Hydrology**: Watershed analysis with enhanced detail
- **Geology**: Terrain visualization for geological surveys
- **Urban Planning**: Base terrain for development simulations

## 📊 System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **CPU** | x86_64 dual-core | 8+ cores with AVX2 |
| **RAM** | 4GB | 16GB+ |
| **Storage** | 100MB free | 1GB+ (for batch processing) |
| **OS** | Linux (Ubuntu 20.04+) | Any modern Linux |

## 🤝 Contributing

### Development Setup
```bash
git clone https://github.com/PeterEbel/hgt2png.git
cd hgt2png
make debug
valgrind --leak-check=full ./hgt2png test.hgt
```

### Code Style
- **C Standard**: GNU99
- **Memory Safety**: All allocations checked, overflow protection
- **Threading**: OpenMP for parallelization, pthread for file I/O
- **Performance**: Profile before optimizing, measure impact

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🏆 Acknowledgments

- **SRTM Data**: NASA/NGA Shuttle Radar Topography Mission
- **libpng**: PNG Reference Library for graphics output
- **OpenMP**: Industry standard for parallel programming
- **Community**: Users and contributors who provided feedback and testing

---

**Ready to enhance your terrain workflows?** 
[📥 Download Latest Release](https://github.com/PeterEbel/hgt2png/releases) | [📖 Full Documentation](Options.md) | [🐛 Report Issues](https://github.com/PeterEbel/hgt2png/issues)
