# 🎯 BLENDER SCRIPT TEST - Step-by-Step Guide

## ✅ **Preparation completed:**
```
📁 Generated files:
   N49E004.png (7202×7202 16-bit Heightmap)
   N49E004.json (Metadata with elevation range 54-502m)
   blender_dyer.py (Enhanced coloring script v2.0)
```

## 🚀 **Blender Setup (5 minutes)**

### **1. Create new Blender file**
- Open Blender
- Delete default cube (X → Delete)
- **Add Plane**: `Shift+A` → Mesh → Plane

### **2. Prepare Plane for heightmap**
```
1. Select plane
2. Tab → Edit Mode
3. Right-click → Subdivide
4. Repeat until ~1000-5000 faces (depending on performance)
5. Tab → Object Mode back
```

### **3. Add Displacement Modifier**
```
Properties Panel → Modifier Properties (Wrench icon)
→ Add Modifier → Displace

Displacement Settings:
- Texture → New → Image Texture
- Open Image → Select N49E004.png
- Strength: 0.01 (adjust for desired height)
- Direction: Z
```

### **4. Run script**
```
1. Scripting tab in Blender
2. Text → Open → Load blender_dyer.py
3. Run script (Play button or Alt+P)
```

## 📝 **Script Commands for Testing**

### **🚀 Automatic Detection (Recommended)**
```python
# Run this in Blender Script Editor:
color_heightmap_faces_advanced()
```

### **🌲 Test Different Biomes**
```python
# Forest (default)
color_heightmap_faces_advanced(biome='forest')

# High Mountains with slope analysis
color_heightmap_faces_advanced(biome='alpine', use_slope=True, slope_intensity=0.5)

# Desert
color_heightmap_faces_advanced(biome='desert')

# Rainforest
color_heightmap_faces_advanced(biome='tropical')
```

### **⚙️ Advanced Options**
```python
# With metadata and slope analysis
color_heightmap_faces_advanced(
    obj_name="Plane",           # Object name
    biome='alpine',             # Biome type  
    use_slope=True,             # Activate slope analysis
    use_metadata=True,          # Use JSON metadata
    slope_intensity=0.7         # Strong rock effects
)
```

## 🎨 **Viewport Settings for Best View**

### **Material Preview Mode**
```
Viewport Shading → Material Preview (3rd icon from left)
or Key: 3
```

### **Rendered View**
```
Viewport Shading → Rendered (4th icon from left) 
or Key: 4
```

## 🔧 **Troubleshooting**

### **❌ "No coloring visible"**
```python
# 1. Check material assignment
print(bpy.context.object.data.materials)

# 2. Force material setup
setup_material_nodes(bpy.data.materials["Heightmap_Material_Forest"])

# 3. Switch viewport to Material Preview
```

### **❌ "JSON not found"**
```python
# Run without metadata (uses Z-coordinates)
color_heightmap_faces_advanced(use_metadata=False)
```

### **❌ "Object not recognized"**
```python
# Manually specify object name
color_heightmap_faces_advanced(obj_name="Plane")
```

## 📊 **Expected Results**

### **🌲 Forest Biome**
- Valley areas: Beige/Sand colors
- Medium heights: Various green tones
- Highest areas: Dark coniferous forest green

### **🏔️ Alpine Biome (with Slope)**
- Flat areas: Alpine grass (Green)
- Steep slopes: Rock colors (Gray/Brown)
- Highest peaks: Snow (White)

### **Console Output**
```
🎯 HGT2PNG Metadata found: N49E004.json
📊 METADATA INFORMATION:
Source: N49E004.hgt
Elevation range: 54m - 502m
Pixel resolution: 45.0m/pixel
✅ Terrain coloring completed for 'Plane'
```

## 🎮 **Live Demo Commands**

### **Quick Demo - Try all biomes**
```python
# Run sequentially:
import time

biomes = ['forest', 'alpine', 'desert', 'tropical']
for biome in biomes:
    print(f"\n🌍 Testing {biome.title()} biome...")
    color_heightmap_faces_advanced(biome=biome, use_slope=True)
    # In Blender: Wait and visually check
```

### **Performance Test**
```python
# Test batch processing
batch_process_heightmaps(biome='alpine', use_slope=True)
```

## 💡 **Pro Tips**

1. **Subdivision Level**: Start with low subdivision, increase as needed
2. **Displacement Strength**: 0.01-0.1 depending on desired height
3. **Viewport Performance**: Disable `use_slope` on large meshes
4. **Lighting**: Add HDRI or Sun light for dramatic results
5. **Camera Angle**: Set camera at angle for best terrain view

---

**🎯 Goal: Realistic, automatically colored terrain in under 5 minutes!**