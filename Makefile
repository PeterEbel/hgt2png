# Makefile for hgt2png - HGT to PNG Heightmap Converter
# Author: Peter Ebel
# Version: 1.1.0

# Compiler and flags
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -O2
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

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(SOURCE)
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
	rm -f $(TARGET)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists $(PKG_DEPS) || (echo "Error: libpng development files not found. Install with: sudo apt-get install libpng-dev pkg-config" && false)
	@echo "All dependencies satisfied."

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build hgt2png (default)"
	@echo "  debug      - Build with debug symbols"  
	@echo "  install    - Install to $(BINDIR)"
	@echo "  uninstall  - Remove from $(BINDIR)"
	@echo "  clean      - Remove build artifacts"
	@echo "  check-deps - Verify dependencies"
	@echo "  help       - Show this help"

# Test compilation with different methods
test-compile:
	@echo "Testing compilation methods..."
	@echo "1. Using pkg-config (recommended):"
	$(CC) $(CFLAGS) -o $(TARGET)-test1 $(SOURCE) $(LIBS)
	@echo "2. Manual linking (fallback):"
	$(CC) -std=gnu99 -o $(TARGET)-test2 $(SOURCE) -lpng -lm -pthread
	@echo "3. Testing includes:"
	@$(CC) -E $(SOURCE) | grep -q "png\.h" && echo "✓ png.h included successfully" || echo "✗ png.h include failed"
	@rm -f $(TARGET)-test1 $(TARGET)-test2

.PHONY: all debug install uninstall clean check-deps help test-compile