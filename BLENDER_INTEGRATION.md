# HGT2PNG Blender Integration v2.0

## 🎯 **Overview**
The enhanced `blender_dyer.py` script provides professional automatic terrain coloring for heightmaps in Blender with **hgt2png integration**, **biome-specific color palettes**, and **slope analysis** for realistic landscape visualization.

## 🚀 **New Features v2.0**

### ✨ **Automatic Integration**
- **Automatic Heightmap Detection**: Finds terrain objects automatically
- **hgt2png JSON Metadata Support**: Loads precise elevation ranges and geographic data
- **Displacement Modifier Integration**: Takes Blender displacement scaling into account
- **Legacy Compatibility**: Supports old script versions

### 🌍 **4 Biome-specific Color Palettes**
```python
'forest'   → Temperate Forest (Beige→Green→Dark Green)
'alpine'   → High Mountains (Brown→Grass→Rock→Gray→Snow)
'desert'   → Desert (Light→Dark Sand→Rock)
'tropical' → Rainforest (Beach→Tropical Green→Jungle)
```

### ⛰️ **Intelligent Slope Analysis**
- **Slope-based Color Modification**: Steep slopes → Rock/Scree colors
- **Terrain-specific Adaptation**: Alpine areas with realistic rock distribution
- **Adjustable Intensity**: Slope effect from subtle to dramatic

### 🔄 **Batch Processing**
- **Multiple Heightmaps**: Processes all terrain objects simultaneously
- **Consistent Palettes**: Uniform coloring across multiple objects

## 📖 **Usage**

### **🚀 Quick Start (Recommended)**
```python
# Automatic detection with modern forest biome
color_heightmap_faces_advanced()
```

### **🌲 Different Biomes**
```python
color_heightmap_faces_advanced(biome='forest')      # Temperate Forest
color_heightmap_faces_advanced(biome='alpine')      # High Mountains
color_heightmap_faces_advanced(biome='desert')      # Desert
color_heightmap_faces_advanced(biome='tropical')    # Rainforest
```

### **⛰️ With Slope Analysis**
```python
# Realistic rock areas on steep slopes
color_heightmap_faces_advanced(
    biome='alpine', 
    use_slope=True, 
    slope_intensity=0.5
)
```

### **📊 With hgt2png Metadata**
```python
# Automatically loads JSON metadata for precise elevation ranges
color_heightmap_faces_advanced(use_metadata=True)
```

### **🔄 Batch Processing**
```python
# Process all heightmaps in the scene
batch_process_heightmaps(biome='forest', use_slope=True)
```

## 🛠️ **HGT2PNG → Blender Workflow**

### **1. Generate Heightmap with Metadata**
```bash
# In hgt2png directory:
./hgt2png --16bit --metadata json --scale-factor 3 terrain.hgt
# Creates: terrain.png + terrain.json
```

### **2. Import into Blender**
1. **Create Plane** and add subdivision
2. **Add Displacement Modifier**
3. **Load 16-bit PNG** as Displacement Texture
4. **Run blender_dyer.py**

### **3. Automatic Coloring**
```python
# Run script in Blender - automatically detects:
# - JSON metadata (terrain.json)  
# - Displacement scaling
# - Terrain object (Plane/Heightmap)
color_heightmap_faces_advanced(biome='alpine', use_slope=True)
```

## 📊 **Metadata Integration**

### **JSON Format (hgt2png v1.1.0)**
```json
{
  "elevation": {
    "min_meters": 54,
    "max_meters": 502
  },
  "scaling": {
    "pixel_pitch_meters": 45.0,
    "scale_factor": 3
  },
  "geographic": {
    "bounds": {...},
    "center": {...}
  }
}
```

### **Automatic Detection**
- Searches for JSON files with same name as heightmap object
- Fallback: Scans all JSON files in blend directory
- Legacy format support for older hgt2png versions

## 🎨 **Biome Details**

### **🌲 Forest (Temperate Forest)**
```
0-20%: Beige (Valley floors/Scree)
20-40%: Light Grass Green  
40-60%: Medium Green (Meadows)
60-80%: Forest Green (Dense Forests)
80-100%: Dark Green (Coniferous Forests)
```

### **🏔️ Alpine (High Mountains)**
```
0-20%: Brown Valleys
20-40%: Alpine Grass  
40-60%: Rock Brown
60-80%: Gray Rock
80-100%: Snow/Ice
```

### **🏜️ Desert (Desert)**
```
0-20%: Light Sand
20-40%: Medium Sand
40-60%: Dark Sand  
60-80%: Rock Brown
80-100%: Dark Rock
```

### **🌺 Tropical (Rainforest)**
```
0-20%: Beach/Coast
20-40%: Light Tropical Green
40-60%: Dense Green
60-80%: Rainforest  
80-100%: Dense Jungle
```

## ⚙️ **Advanced Parameters**

### **color_heightmap_faces_advanced() Parameters:**
```python
obj_name=None,           # Object name (None=auto)
biome='forest',          # Biome type  
use_slope=True,          # Slope analysis
use_metadata=True,       # Load JSON metadata
slope_intensity=0.3      # Slope effect strength (0.0-1.0)
```

### **Slope Analysis:**
- `slope_intensity=0.0`: No slope effects
- `slope_intensity=0.3`: Subtle rock areas (default)
- `slope_intensity=0.7`: Clear rock structures
- `slope_intensity=1.0`: Dramatic rock dominance

## 🔧 **Material System**
- **Automatic Material Setup**: Creates shader nodes automatically
- **Face Color Attributes**: Uses modern Blender Color Attribute pipeline
- **Multi-Material Support**: Different biome materials in parallel

## 🌲 **Batch Rendering with Vegetation Masks**

[scripts/generate.py](scripts/generate.py) renders every heightmap in a directory with a `TerrainMaterial` material and a displaced `Plane`, and uses the vegetation mask of each tile:

```bash
hgt2png -V --biome temperate --alpha-nodata --16bit --lat-aridity 43.5:0.1,40:0.5,36:0.9 ../hgt/N37W007.hgt
blender terrain.blend --background --python scripts/generate.py
```

- **Files:** `<tile>.png` and `<tile>_vegetation_<biome>.png` in `INPUT_DIR` (the script ignores masks and `*_render.png` when listing heightmaps).
- **Nodes added:** `VegetationMask` (image texture, Non-Color) and `VegetationMix` (Mix Color). The mask drives the factor, the heightmap color goes to B, and the result replaces the heightmap as ColorRamp `Fac`. The sea shader (Diffuse + Mix Shader with the heightmap alpha) is left untouched.
- **No mask:** The tile is rendered with a white mask, which equals the pure height coloring.
- **Settings:** `INPUT_DIR`, `OUTPUT_DIR`, `VEGETATION_BIOME` and `BARE_GROUND_VALUE` (ColorRamp position for areas without vegetation) at the top of the script.
- **Alpha:** Use `--16bit --alpha-nodata` with hgt2png 1.5.2 or newer, so NoData pixels keep gray value 0 under alpha 0.

## 🚨 **Troubleshooting**

### **No coloring visible:**
- Make sure Viewport Shading is set to "Material Preview" or "Rendered"
- Check if material is correctly assigned

### **Wrong elevation ranges:**
```python
# Force manual Z-coordinate analysis
color_heightmap_faces_advanced(use_metadata=False)
```

### **JSON metadata not found:**
- Make sure JSON file is in same directory as .blend file
- Check filename: `terrain.hgt` → `terrain.json`

## 💡 **Performance Tips**
- **Disable slope analysis** on large meshes (>1M faces) for better performance
- **Use batch processing** for consistent results with multiple heightmaps
- **Use 16-bit PNG** displacement maps for best quality

## 🔄 **Legacy Compatibility**
```python
# Old function still works:
color_heightmap_faces_five_colors_green("Plane")
# → Automatically calls new function with forest biome
```

---

**Developed for hgt2png v1.1.0 | Blender 3.0+ | Python 3.8+**