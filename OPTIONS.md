# 🛠️ HGT2PNG v1.1.0 - Complete Options Reference

## Overview

HGT2PNG v1.1.0 is a professional heightmap converter that converts SRTM HGT files to PNG images with advanced features for 3D workflows, game development, and GIS applications.

## Basic Syntax

```bash
./hgt2png [OPTIONS] <input.hgt|filelist.txt>
```

---

## 🚀 Performance & Quality Options

### `-s, --scale-factor <n>` (Default: 3)

**Purpose:** Controls resolution enhancement through procedural detail generation

**Values:**
- `1` = Original resolution (3601×3601 for SRTM-1)
- `2` = Double resolution (7202×7202)
- `3` = Triple resolution (10803×10803) **[Recommended]**
- `4+` = Higher values possible (exponentially more computation time)

**Application:**
- **3D Modeling:** Higher values for more detailed displacement maps
- **Game Engines:** `2-3` for realistic terrain meshes
- **Quick Previews:** `1` for maximum speed

```bash
# Examples
./hgt2png -s 1 terrain.hgt          # Fast, original quality
./hgt2png -s 3 terrain.hgt          # Recommended for production
./hgt2png -s 4 terrain.hgt          # Very high quality, takes time
```

### `-i, --detail-intensity <f>` (Default: 15.0)

**Purpose:** Controls the strength of added procedural details in meters

**Values:**
- `5.0-10.0` = Subtle details (realistic erosion)
- `15.0-25.0` = Moderate details **[Recommended]**
- `30.0+` = Dramatic details (fantasy landscapes)

**Technical Background:**
The system adds multiple noise octaves:
- **Large Features** (0.005× frequency): General landscape forms
- **Medium Features** (0.02× frequency): Hills and valleys
- **Fine Details** (0.08× frequency): Surface texture

```bash
# Examples
./hgt2png -i 8.0 terrain.hgt        # Realistic erosion
./hgt2png -i 20.0 terrain.hgt       # Dramatic landscape
./hgt2png -i 35.0 terrain.hgt       # Fantasy/alien terrain
```

### `-r, --noise-seed <n>` (Default: 12345)

**Purpose:** Deterministic random seed for reproducible results

**Application:**
- **Consistency:** Same seed = identical details on multiple runs
- **Variation:** Different seeds = different detail patterns
- **Workflow:** Document seed for project documentation

```bash
# Examples
./hgt2png -r 42 terrain.hgt         # Seed 42 for Project A
./hgt2png -r 1337 terrain.hgt       # Seed 1337 for Project B
```

---

## ⚡ Parallel Processing

### `-t, --threads <n>` (Default: 4)

**Purpose:** Utilizes multiple CPU cores for OpenMP parallelization

**Values:**
- `1` = Sequential (only for debugging)
- `2-8` = Optimal for most systems
- `16` = Maximum (for high-end workstations)

**Performance Impact:**
- **SIMD Vectorization:** Uses AVX2 for 8 pixels in parallel
- **Multi-Threading:** Distributes rows across cores
- **Expected Speedup:** ~4-8× with 8 cores

```bash
# Examples
./hgt2png -t 1 terrain.hgt          # Debug mode, single-threaded
./hgt2png -t 8 terrain.hgt          # Full CPU utilization
OMP_NUM_THREADS=8 ./hgt2png terrain.hgt  # Alternative via environment variable
```

---

## 🎨 Output Formats & Quality

### `--16bit`

**Purpose:** Generates 16-bit grayscale PNG (0-65535 instead of 0-255)

**Advantages:**
- **Higher Precision:** 256× more elevation levels
- **Displacement Maps:** Ideal for 3D software (Blender, Maya, 3ds Max)
- **GIS Analysis:** Precise elevation values for scientific evaluation

**Disadvantages:**
- **File Size:** ~2× larger than 8-bit
- **Compatibility:** Not all viewers display 16-bit correctly

```bash
# Examples
./hgt2png --16bit terrain.hgt                    # 16-bit grayscale
./hgt2png --16bit --alpha-nodata terrain.hgt     # 16-bit + transparency
```

### `--alpha-nodata`

**Purpose:** NoData pixels become transparent (Alpha=0) instead of black

**Application:**
- **Island Terrain:** Water areas transparent
- **Tile Systems:** Seamless transitions between tiles
- **Compositing:** Easy overlay in graphics programs

**Technical Details:**
- **8-bit Mode:** RGBA PNG (4 channels)
- **16-bit Mode:** 16-bit grayscale + 16-bit alpha
- **NoData Values:** Typically -32768 in SRTM files

```bash
# Examples
./hgt2png --alpha-nodata terrain.hgt            # Transparent water surfaces
./hgt2png --16bit --alpha-nodata terrain.hgt    # Highest quality + transparency
```

---

## 📊 Elevation Mapping & Curves

### `-m, --min-height <n>` and `-M, --max-height <n>`

**Purpose:** Defines the elevation range for pixel mapping

**Default:** Auto-detection (uses actual min/max values of the file)

**Application:**
- **Consistency:** Same scale for multiple tiles
- **Contrast:** Focus on interesting elevation range
- **Normalization:** Uniform representation of different regions

```bash
# Examples
./hgt2png -m 0 -M 1000 terrain.hgt              # Sea level to 1000m
./hgt2png -m 500 -M 2500 terrain.hgt            # Mountain range 500-2500m
./hgt2png terrain.hgt                            # Auto-detection (recommended)
```

### `-c, --curve <type>` (Default: linear)

**Purpose:** Mapping curve between elevation values and pixel brightness

#### **linear** (Default)
- **Function:** `pixel = (height - min) / (max - min)`
- **Characteristic:** Even distribution
- **Application:** Scientific evaluation, technical representation

#### **log** (Logarithmic)
- **Function:** `pixel = log(height - min + 1) / log(max - min + 1)`
- **Characteristic:** More details in low elevations
- **Application:** Coastal regions, emphasize flat landscapes

```bash
# Examples
./hgt2png -c linear terrain.hgt                 # Even representation
./hgt2png -c log terrain.hgt                    # Emphasize lowland details
```

### `-g, --gamma <f>` (Default: 1.0, Range: 0.1-10.0)

**Purpose:** Gamma correction for visual adjustment

**Values:**
- `<1.0` (e.g. 0.5): **Brighten** - more details in dark areas
- `1.0`: **Linear** - no correction
- `>1.0` (e.g. 2.2): **Darken** - more contrast, photorealistic representation

**Technical Details:**
- **Monitor Gamma:** Most displays have gamma ~2.2
- **Workflow:** For displacement maps often gamma 1.0, for visualization 2.2

```bash
# Examples
./hgt2png -g 0.5 terrain.hgt                    # Brightened, more shadow details
./hgt2png -g 2.2 terrain.hgt                    # Monitor standard, realistic
./hgt2png -g 1.0 terrain.hgt                    # Linear, for 3D software
```

---

## 📄 Metadata & Workflow Integration

### `-x, --metadata <format>` (Default: none)

**Purpose:** Generates sidecar files with technical metadata

#### **json** Format
```json
{
  "source_file": "N49E004.hgt",
  "png_file": "N49E004.png",
  "elevation_range": {
    "min_meters": 54,
    "max_meters": 502,
    "effective_min": 54,
    "effective_max": 502
  },
  "spatial_info": {
    "pixel_pitch_meters": 30.0,
    "width_pixels": 10803,
    "height_pixels": 10803,
    "geographic_bounds": {
      "south": 49.0,
      "north": 50.0,
      "west": 4.0,
      "east": 5.0
    }
  },
  "processing_info": {
    "scale_factor": 3,
    "detail_intensity": 15.0,
    "output_format": "16-bit_grayscale_alpha",
    "curve_type": "linear",
    "gamma": 1.0
  }
}
```

#### **txt** Format (Human-readable)
```
Source: N49E004.hgt
Output: N49E004.png
Elevation Range: 54m - 502m
Resolution: 10803×10803 pixels
Pixel Pitch: 30.0 meters/pixel
Geographic Bounds: 49.0°N - 50.0°N, 4.0°E - 5.0°E
Scale Factor: 3
Detail Intensity: 15.0m
```

### **Blender Integration with Metadata**

#### **1. Displacement Modifier Setup**
```python
# Blender Python Script - Automatic displacement map import
import json
import bpy

# Load metadata
with open("N49E004.json", "r") as f:
    metadata = json.load(f)

# Create plane with correct dimensions
bpy.ops.mesh.primitive_plane_add()
plane = bpy.context.active_object

# Size based on real coordinates
real_width = (metadata["spatial_info"]["geographic_bounds"]["east"] - 
              metadata["spatial_info"]["geographic_bounds"]["west"]) * 111000  # Degrees to meters
real_height = (metadata["spatial_info"]["geographic_bounds"]["north"] - 
               metadata["spatial_info"]["geographic_bounds"]["south"]) * 111000

plane.scale = (real_width/2, real_height/2, 1)

# Add displacement modifier
displacement_mod = plane.modifiers.new(name="Heightmap", type='DISPLACE')

# Material with heightmap texture
material = bpy.data.materials.new(name="Terrain")
plane.data.materials.append(material)
material.use_nodes = True

# Image Texture Node
texture_node = material.node_tree.nodes.new(type='ShaderNodeTexImage')
texture_node.image = bpy.data.images.load("N49E004.png")

# Height scaling based on real data
height_range = metadata["elevation_range"]["max_meters"] - metadata["elevation_range"]["min_meters"]
displacement_mod.strength = height_range
```

#### **2. GIS Workflow Integration**
```bash
# Batch processing for large areas
./hgt2png -x json --16bit --alpha-nodata *.hgt

# Collect metadata for GIS import
for json_file in *.json; do
    echo "Processing $json_file"
    # GDAL tools can use JSON metadata for georeferencing
    # Python scripts for automatic coordinate extraction
done
```

#### **3. Game Engine Integration (Unity/Unreal)**
```csharp
// Unity C# - Automatic terrain import
[System.Serializable]
public class HeightmapMetadata
{
    public float pixel_pitch_meters;
    public int width_pixels;
    public int height_pixels;
    public ElevationRange elevation_range;
}

// Create terrain from PNG + JSON
TerrainData terrainData = new TerrainData();
HeightmapMetadata meta = JsonUtility.FromJson<HeightmapMetadata>(File.ReadAllText("terrain.json"));

terrainData.heightmapResolution = meta.width_pixels;
terrainData.size = new Vector3(
    meta.pixel_pitch_meters * meta.width_pixels, 
    meta.elevation_range.max_meters - meta.elevation_range.min_meters,
    meta.pixel_pitch_meters * meta.height_pixels
);
```

---

## 🛠️ Workflow Options

### `-d, --disable-detail`

**Purpose:** Disables procedural detail generation

**Application:**
- **Fast Conversion:** Only basic conversion without enhancement
- **Comparison Tests:** Original vs. enhanced
- **Debugging:** Isolation of problems

```bash
# Examples
./hgt2png -d terrain.hgt                        # Only basic conversion
./hgt2png -d --16bit terrain.hgt                # 16-bit without details
```

### `-q, --quiet`

**Purpose:** Suppresses verbose output (only errors are shown)

**Application:**
- **Batch Processing:** Clean log files
- **Automated Scripts:** Reduced output
- **Performance:** Minimal I/O overhead

```bash
# Examples
./hgt2png -q terrain.hgt                        # Silent execution
./hgt2png -q *.hgt > processing.log 2>&1        # Log only errors
```

---

## 📁 Input Formats

### **Single HGT File**
```bash
./hgt2png N49E004.hgt
./hgt2png /path/to/terrain.hgt
```

### **File List (Batch Processing)**
```bash
# Create filelist.txt:
echo "N49E004.hgt" > filelist.txt
echo "N49E005.hgt" >> filelist.txt
echo "N50E004.hgt" >> filelist.txt

# Process all:
./hgt2png filelist.txt
```

---

## 🎯 Recommended Workflows

### **3D Modeling (Blender/Maya)**
```bash
./hgt2png --16bit --alpha-nodata -s 3 -i 12.0 -g 1.0 -x json terrain.hgt
```

### **Game Development**
```bash
./hgt2png -s 2 -i 20.0 -c linear --alpha-nodata -x json terrain.hgt
```

### **Scientific Visualization**
```bash
./hgt2png --16bit -d -c linear -g 1.0 -x json terrain.hgt
```

### **Fast Preview Generation**
```bash
./hgt2png -s 1 -d -q terrain.hgt
```

### **High-Quality Artistic**
```bash
./hgt2png -s 4 -i 25.0 -g 2.2 -c log --16bit -x json terrain.hgt
```

---

## 💡 Tips & Tricks

1. **Memory Usage:** With `scale-factor > 3`, watch RAM usage (>8GB recommended)
2. **Batch Processing:** Use `-q` and log redirection for large quantities
3. **Consistency:** Document parameters used for projects
4. **Format Choice:** 16-bit for production, 8-bit for previews
5. **Thread Count:** Usually optimal: Number of CPU cores - 1

---
