# Makefile for hgt2png - HGT to PNG Heightmap Converter  
# Author: Peter Ebel
# Version: 1.1.0 with OpenMP optimization

# Compiler and flags
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -O3
OMPFLAGS = -fopenmp -mavx2
DEBUGFLAGS = -g -DDEBUG
TARGET = hgt2png
SOURCE = hgt2png.c

# Dependencies
PKG_DEPS = libpng
LIBS = -lm -pthread

# Use pkg-config for portable library detection
CFLAGS += $(shell pkg-config --cflags $(PKG_DEPS))
LIBS += $(shell pkg-config --libs $(PKG_DEPS))

# Installation directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Default target (OpenMP optimized)
all: $(TARGET)

# Build target with OpenMP
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(OMPFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Fallback build without OpenMP (for compatibility)
nomp: $(TARGET)-compat

$(TARGET)-compat: $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Debug build
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)

# Install binary
install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)

# Uninstall binary  
uninstall:
	rm -f $(BINDIR)/$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TARGET)-compat

# Check dependencies  
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists $(PKG_DEPS) && echo "✓ libpng OK" || (echo "✗ libpng missing - install: sudo apt install libpng-dev pkg-config" && false)
	@gcc --version >/dev/null 2>&1 && echo "✓ GCC OK" || (echo "✗ GCC missing - install: sudo apt install build-essential" && false)
	@echo "All dependencies satisfied."

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build hgt2png with OpenMP optimization (default)"
	@echo "  nomp       - Build without OpenMP (compatibility mode)"
	@echo "  debug      - Build with debug symbols"  
	@echo "  install    - Install to $(BINDIR)"
	@echo "  uninstall  - Remove from $(BINDIR)"
	@echo "  clean      - Remove build artifacts"
	@echo "  check-deps - Verify dependencies"
	@echo "  help       - Show this help"

.PHONY: all nomp debug install uninstall clean check-deps help