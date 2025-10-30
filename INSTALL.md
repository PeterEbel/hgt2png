# HGT2PNG Installation Guide

## System Requirements

### **Minimum Requirements:**
- **OS:** Linux (Ubuntu/Debian/Mint 20.04+) 
- **RAM:** 4GB minimum, 8GB+ recommended for large files
- **CPU:** x86_64 with AVX2 support (Intel Core i3+ or AMD Ryzen)
- **Storage:** ~50MB for program + 500MB per 25MB HGT file

### **Installing Dependencies:**

#### **Ubuntu/Debian/Linux Mint:**
```bash
# Basic development tools
sudo apt update
sudo apt install build-essential

# Required libraries
sudo apt install libpng-dev pkg-config

# Optional: OpenMP support (usually included in gcc)
sudo apt install libomp-dev

# Verify installation
pkg-config --exists libpng && echo "✓ libpng OK" || echo "✗ libpng missing"
gcc --version | grep -q "gcc" && echo "✓ GCC OK" || echo "✗ GCC missing"
```

#### **Fedora/RHEL/CentOS:**
```bash
# Basic development tools
sudo dnf groupinstall "Development Tools"

# Required libraries  
sudo dnf install libpng-devel pkgconfig

# OpenMP (usually included)
sudo dnf install libgomp-devel
```

#### **Arch Linux:**
```bash
# Basic development tools
sudo pacman -S base-devel

# Required libraries
sudo pacman -S libpng pkg-config
```

## Compilation

### **Method 1: Automatic with Makefile (Recommended)**

```bash
# Clone repository or copy files
git clone https://github.com/PeterEbel/hgt2png.git
cd hgt2png

# Check dependencies
make check-deps

# Compile (optimized)
make

# Test
./hgt2png --help
```

### **Method 2: Manual (if Makefile issues)**

```bash
# Standard compilation (without OpenMP)
gcc -std=gnu99 -Wall -O2 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Optimized compilation (with OpenMP + AVX2)  
gcc -std=gnu99 -Wall -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp -mavx2

# Fallback (if pkg-config missing)
gcc -std=gnu99 -Wall -O2 hgt2png.c -o hgt2png -lpng -lm -pthread
```

### **Method 3: Using Updated Makefile**

The included Makefile is outdated. Here's an updated version:

```makefile
# Makefile for hgt2png v1.1.0 - OpenMP-optimized
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -O3
OMPFLAGS = -fopenmp -mavx2
TARGET = hgt2png
SOURCE = hgt2png.c

# Dependencies
PKG_DEPS = libpng
LIBS = -lm -pthread

# Use pkg-config for portable library detection
CFLAGS += $(shell pkg-config --cflags $(PKG_DEPS))
LIBS += $(shell pkg-config --libs $(PKG_DEPS))

# Default target (with OpenMP)
all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(OMPFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Fallback without OpenMP (for compatibility)
nomp: $(TARGET)-nomp

$(TARGET)-nomp: $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET)-nomp

check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists $(PKG_DEPS) && echo "✓ libpng OK" || (echo "✗ libpng missing - install: sudo apt install libpng-dev" && false)
	@gcc --version >/dev/null 2>&1 && echo "✓ GCC OK" || (echo "✗ GCC missing - install: sudo apt install build-essential" && false)
	@echo "Dependencies OK!"

.PHONY: all nomp clean check-deps
```

## Troubleshooting

### **"libpng not found"**
```bash
# Ubuntu/Debian
sudo apt install libpng-dev pkg-config

# Test
pkg-config --libs libpng
```

### **"OpenMP not supported"**
```bash
# Compile without OpenMP (slower but compatible version)
gcc -std=gnu99 -O2 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Or explicitly enable
sudo apt install libomp-dev
```

### **"AVX2 not supported"**
```bash
# Compile without AVX2 (older CPUs)
gcc -std=gnu99 -fopenmp -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Check CPU features
cat /proc/cpuinfo | grep -o avx2
```

### **"Permission denied"**
```bash
# Set executable permissions
chmod +x hgt2png

# Or install system-wide
sudo make install
```

## Performance Optimization

### **CPU Optimization:**
```bash
# Automatic CPU detection
gcc -march=native -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp

# Different architectures
gcc -march=skylake ...    # Intel Skylake+
gcc -march=znver2 ...     # AMD Zen2+
gcc -march=core2 ...      # Older CPUs
```

### **Thread Configuration:**
```bash
# Set thread count
export OMP_NUM_THREADS=8

# Or via command line
./hgt2png -t 8 terrain.hgt
```

## Required Files for Distribution

### **Minimal (Source-only):**
```
hgt2png.c          # Main program
INSTALL.md         # This guide  
```

### **Complete (recommended):**
```
hgt2png.c          # Main program
Makefile           # Build system
INSTALL.md         # Installation
Options.md         # Detailed documentation
README.md          # Project overview
LICENSE            # License information
```

### **Binary Distribution:**
```bash
# Static build for portability
gcc -static -std=gnu99 -O3 hgt2png.c -o hgt2png \
    -lpng -lz -lm -pthread -fopenmp

# Create package
tar -czf hgt2png-linux-x64.tar.gz hgt2png Options.md INSTALL.md
```

## Installation Verification

```bash
# Check version
./hgt2png --version

# Show help  
./hgt2png --help

# Quick test (if HGT file available)
./hgt2png --scale-factor 1 --disable-detail --quiet test.hgt

# Performance test
time ./hgt2png -s 2 terrain.hgt
```

## Support & Debugging

### **Create Debug Build:**
```bash
gcc -g -DDEBUG -std=gnu99 hgt2png.c -o hgt2png-debug \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Test with Valgrind
valgrind --leak-check=full ./hgt2png-debug test.hgt
```

### **Common Issues:**
1. **Not enough RAM:** Use `--scale-factor 1` for large files
2. **Old CPU:** Compile without `-mavx2` 
3. **Missing Libraries:** Use `make check-deps`
4. **Permission Errors:** Check write permissions in output directory

---

**For issues:** Create an issue with compiler version, OS info and error message.