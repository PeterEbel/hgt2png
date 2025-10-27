# HGT2PNG Installation Guide

## Systemvoraussetzungen

### **Mindestanforderungen:**
- **OS:** Linux (Ubuntu/Debian/Mint 20.04+) 
- **RAM:** 4GB minimum, 8GB+ empfohlen für große Dateien
- **CPU:** x86_64 mit AVX2-Unterstützung (Intel Core i3+ oder AMD Ryzen)
- **Speicher:** ~50MB für Programm + 500MB pro 25MB HGT-Datei

### **Abhängigkeiten installieren:**

#### **Ubuntu/Debian/Linux Mint:**
```bash
# Basis-Entwicklungstools
sudo apt update
sudo apt install build-essential

# Erforderliche Libraries
sudo apt install libpng-dev pkg-config

# Optional: OpenMP-Unterstützung (meist schon in gcc enthalten)
sudo apt install libomp-dev

# Überprüfung der Installation
pkg-config --exists libpng && echo "✓ libpng OK" || echo "✗ libpng fehlt"
gcc --version | grep -q "gcc" && echo "✓ GCC OK" || echo "✗ GCC fehlt"
```

#### **Fedora/RHEL/CentOS:**
```bash
# Basis-Entwicklungstools
sudo dnf groupinstall "Development Tools"

# Erforderliche Libraries  
sudo dnf install libpng-devel pkgconfig

# OpenMP (meist schon enthalten)
sudo dnf install libgomp-devel
```

#### **Arch Linux:**
```bash
# Basis-Entwicklungstools
sudo pacman -S base-devel

# Erforderliche Libraries
sudo pacman -S libpng pkg-config
```

## Kompilierung

### **Methode 1: Automatisch mit Makefile (Empfohlen)**

```bash
# Repository klonen oder Dateien kopieren
git clone https://github.com/PeterEbel/hgt2png.git
cd hgt2png

# Dependencies überprüfen
make check-deps

# Kompilieren (optimiert)
make

# Testen
./hgt2png --help
```

### **Methode 2: Manuell (falls Makefile-Probleme)**

```bash
# Standard-Kompilierung (ohne OpenMP)
gcc -std=gnu99 -Wall -O2 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Optimierte Kompilierung (mit OpenMP + AVX2)  
gcc -std=gnu99 -Wall -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp -mavx2

# Fallback (wenn pkg-config fehlt)
gcc -std=gnu99 -Wall -O2 hgt2png.c -o hgt2png -lpng -lm -pthread
```

### **Methode 3: Aktualisiertes Makefile verwenden**

Das mitgelieferte Makefile ist veraltet. Hier ist eine aktualisierte Version:

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
	@pkg-config --exists $(PKG_DEPS) && echo "✓ libpng OK" || (echo "✗ libpng fehlt - installiere: sudo apt install libpng-dev" && false)
	@gcc --version >/dev/null 2>&1 && echo "✓ GCC OK" || (echo "✗ GCC fehlt - installiere: sudo apt install build-essential" && false)
	@echo "Dependencies OK!"

.PHONY: all nomp clean check-deps
```

## Problembehandlung

### **"libpng not found"**
```bash
# Ubuntu/Debian
sudo apt install libpng-dev pkg-config

# Test
pkg-config --libs libpng
```

### **"OpenMP not supported"**
```bash
# Ohne OpenMP kompilieren (langsamere aber kompatible Version)
gcc -std=gnu99 -O2 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Oder explizit aktivieren
sudo apt install libomp-dev
```

### **"AVX2 not supported"**
```bash
# Ohne AVX2 kompilieren (ältere CPUs)
gcc -std=gnu99 -fopenmp -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# CPU-Features prüfen
cat /proc/cpuinfo | grep -o avx2
```

### **"Permission denied"**
```bash
# Ausführungsrechte setzen
chmod +x hgt2png

# Oder systemweit installieren
sudo make install
```

## Performance-Optimierung

### **CPU-Optimierung:**
```bash
# Automatische CPU-Erkennung
gcc -march=native -O3 hgt2png.c -o hgt2png \
    $(pkg-config --cflags --libs libpng) -lm -pthread -fopenmp

# Verschiedene Architekturen
gcc -march=skylake ...    # Intel Skylake+
gcc -march=znver2 ...     # AMD Zen2+
gcc -march=core2 ...      # Ältere CPUs
```

### **Thread-Konfiguration:**
```bash
# Anzahl Threads festlegen
export OMP_NUM_THREADS=8

# Oder per Kommandozeile
./hgt2png -t 8 terrain.hgt
```

## Benötigte Dateien für Distribution

### **Minimal (Source-only):**
```
hgt2png.c          # Hauptprogramm
INSTALL.md         # Diese Anleitung  
```

### **Vollständig (empfohlen):**
```
hgt2png.c          # Hauptprogramm
Makefile           # Build-System
INSTALL.md         # Installation
Options.md         # Detaillierte Dokumentation
README.md          # Projekt-Übersicht
LICENSE            # Lizenz-Information
```

### **Binary-Distribution:**
```bash
# Static Build für Portabilität
gcc -static -std=gnu99 -O3 hgt2png.c -o hgt2png \
    -lpng -lz -lm -pthread -fopenmp

# Package erstellen
tar -czf hgt2png-linux-x64.tar.gz hgt2png Options.md INSTALL.md
```

## Verifikation der Installation

```bash
# Version prüfen
./hgt2png --version

# Hilfe anzeigen  
./hgt2png --help

# Schneller Test (wenn HGT-Datei vorhanden)
./hgt2png --scale-factor 1 --disable-detail --quiet test.hgt

# Performance-Test
time ./hgt2png -s 2 terrain.hgt
```

## Support & Debugging

### **Debug-Build erstellen:**
```bash
gcc -g -DDEBUG -std=gnu99 hgt2png.c -o hgt2png-debug \
    $(pkg-config --cflags --libs libpng) -lm -pthread

# Mit Valgrind testen
valgrind --leak-check=full ./hgt2png-debug test.hgt
```

### **Häufige Issues:**
1. **Zu wenig RAM:** Verwende `--scale-factor 1` für große Dateien
2. **Alte CPU:** Kompiliere ohne `-mavx2` 
3. **Fehlende Libraries:** Nutze `make check-deps`
4. **Permission Errors:** Überprüfe Schreibrechte im Ausgabeverzeichnis

---

**Bei Problemen:** Erstelle ein Issue mit Compiler-Version, OS-Info und Fehlermeldung.