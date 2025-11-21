
# HGT2PNG v1.3.0 - Professional SRTM Terrain Converter

High-performance, OpenMP-optimized converter for SRTM HGT terrain data to PNG displacement maps with advanced procedural detail generation, **multi-biome vegetation mask generation** (Alpine + Temperate), and professional 3D workflow integration.

[![Performance](https://img.shields.io/badge/Performance-6x_faster-green.svg)](##performance)
[![OpenMP](https://img.shields.io/badge/OpenMP-Optimized-blue.svg)](#features)
[![16-bit](https://img.shields.io/badge/PNG-16--bit_support-orange.svg)](#output-formats)
[![Vegetation](https://img.shields.io/badge/Vegetation-Alpine_Biomes-forestgreen.svg)](#vegetation-masks)

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

### 🌲 **Vegetation Mask Generation**
- **Multi-Biome System**: Alpine (700-2400m) and Temperate (0-1500m) biomes with scientifically accurate vegetation distribution
- **Realistic Parameters**: Elevation-based tree/bush/grass lines, slope tolerance, aspect effects
- **Grayscale Masks**: 0-255 density values for Blender material mixing and procedural landscapes
- **Geographic Accuracy**: North/south slope effects, valley drainage patterns, elevation-dependent density curves

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

### Vegetation Mask Generation (NEW! 🌲)
```bash
# Alpine vegetation masks for mountain terrain (700-2400m)
./hgt2png --vegetation-mask --biome alpine N46E007.hgt
# → Creates N46E007.png + N46E007_vegetation_alpine.png

# Temperate vegetation masks for lowland/mid-elevation terrain (0-1500m)
./hgt2png --vegetation-mask --biome temperate N49E004.hgt
# → Creates N49E004.png + N49E004_vegetation_temperate.png

# Combined heightmap + vegetation mask + metadata
./hgt2png --16bit --vegetation-mask --biome alpine --metadata json terrain.hgt
# → terrain.png (displacement) + terrain_vegetation_alpine.png (vegetation density)

# Blender-ready workflow: heightmap + vegetation + metadata
./hgt2png --16bit --alpha-nodata --vegetation-mask --biome temperate \
          --metadata json --scale-factor 3 lowlands.hgt
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
| **`-V, --vegetation-mask`** | **Generate vegetation density mask** | **`-V`** |
| **`-B, --biome <type>`** | **Select biome (alpine\|temperate\|tropical)** | **`-B alpine`** |

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

### Vegetation Mask Format
- **Use case**: Blender material mixing, procedural landscape generation
- **Format**: 8-bit grayscale PNG (0-255 vegetation density)
- **Usage**: White = dense vegetation, Black = no vegetation (rock/snow/steep slopes)
- **File size**: ~6MB for 10803×10803 (highly compressed due to natural patterns)

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
// Generated metadata (actual structure)
{
  "elevation": {"min_meters": 54, "max_meters": 502},
  "scaling": {
    "pixel_pitch_meters": 90.0,
    "world_size_meters": {"width": 324090.0, "height": 324090.0}
  },
  "geographic": {
    "bounds": {"south": 49.0, "north": 50.0, "west": 4.0, "east": 5.0}
  }
}
```

### Research Use Cases
- **Climate Modeling**: High-resolution terrain for atmospheric simulations
- **Hydrology**: Watershed analysis with enhanced detail
- **Geology**: Terrain visualization for geological surveys
- **Urban Planning**: Base terrain for development simulations
- **Ecology**: Alpine vegetation distribution modeling for conservation and climate research

## 🌲 Vegetation Masks

HGT2PNG generates scientifically accurate vegetation distribution masks for realistic landscape rendering. Choose the biome that matches your terrain's elevation range.

### Biome Comparison

| Feature | Alpine Biome | Temperate Biome |
|---------|--------------|-----------------|
| **Elevation Range** | 700-2400m | 0-1500m |
| **Target Terrain** | Mountain landscapes, high-altitude regions | Lowlands, mid-elevation hills, Central Europe |
| **Max Slope** | 45° | 50° |
| **Density Distribution** | Linear decrease with elevation | Parabolic (peaks at ~600m) |
| **Tree Line** | 1800m | 1200m |
| **Bush Line** | 2200m | 1400m |
| **Grass Line** | 2400m | 1500m |
| **Aspect Influence** | Strong (0.2) | Moderate (0.15) |
| **Drainage Bonus** | Moderate (0.3) | Strong (0.4) |

### Alpine Biome System (700-2400m)
Perfect for mountain terrain, high-altitude regions, and alpine landscapes.

#### Elevation Zones
- **Montane Forest** (700-1800m): Dense coniferous forests (spruce, fir, larch)
- **Subalpine Zone** (1800-2200m): Transition zone with shrinking trees and dwarf pine
- **Alpine Zone** (2200-2400m): Alpine meadows, cushion plants, sparse grass
- **Nival Zone** (>2400m): No vegetation (permanent snow and rock)

#### Environmental Factors
- **Slope Limitation**: Maximum 45° for vegetation growth
- **Aspect Effects**: South faces drier (less vegetation), North faces moister (more vegetation)
- **Drainage Patterns**: Valley bottoms support more vegetation than exposed ridges
- **Gradual Transitions**: Realistic density gradients between zones

### Temperate Biome System (0-1500m)
Ideal for lowland terrain, mid-elevation hills, and Central European landscapes.

#### Elevation Zones
- **Lowland Forest** (0-600m): Dense deciduous/mixed forests (oak, beech, maple) - **maximum density**
- **Mid-Elevation Forest** (600-1200m): Transition to coniferous forests with decreasing density
- **Montane Forest** (1200-1400m): Sparse high-elevation forests and shrubland
- **Subalpine Meadows** (1400-1500m): Grass and low vegetation
- **Alpine Zone** (>1500m): No temperate vegetation (use Alpine biome instead)

#### Environmental Factors
- **Slope Tolerance**: Maximum 50° (higher than Alpine due to lowland terrain stability)
- **Aspect Effects**: Moderate influence (0.15) - less pronounced than Alpine
- **Drainage Bonus**: Strong effect (0.4) - lowland rivers and valleys very fertile
- **Parabolic Distribution**: Vegetation density peaks at ~600m elevation, gradually decreasing above and below

### Blender Integration
```glsl
// Use vegetation mask as Mix Factor in Shader Editor
vegetationDensity = texture(vegetation_mask, uv).r;
finalColor = mix(rockMaterial, vegetationMaterial, vegetationDensity);

// Example: Alpine terrain with snow above tree line
alpineDensity = texture(alpine_vegetation_mask, uv).r;
finalColor = mix(snowMaterial, mix(rockMaterial, forestMaterial, alpineDensity), alpineDensity);

// Example: Temperate lowlands with rivers
temperateDensity = texture(temperate_vegetation_mask, uv).r;
finalColor = mix(grassMaterial, mix(soilMaterial, forestMaterial, temperateDensity), temperateDensity);
```

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
