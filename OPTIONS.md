# 🔧 HGT2PNG v1.5.2 - Advanced Technical Reference

> **Note:** For basic usage and command options, see [README.md](README.md). This document covers advanced workflows and technical integration details.

---

## 🧬 Technical Deep Dive

### Procedural Detail Generation Algorithm

The detail enhancement uses multi-octave fractal noise with three frequency layers:

```c
// Simplified algorithm (actual implementation in hgt2png.c)
for (octave = 0; octave < 3; octave++) {
    frequency = base_frequency * pow(2.0, octave);     // 0.005×, 0.02×, 0.08×
    amplitude = detail_intensity * pow(0.5, octave);   // Decreasing contribution
    noise_value = simplex_noise(x * frequency, y * frequency, seed);
    detail_height += noise_value * amplitude;
}

// Slope-aware detail application
slope_factor = min(1.0, current_slope / 45.0);
final_detail = detail_height * slope_factor;         // Less detail on steep slopes
```

**Parameter Guidelines:**
- **`detail-intensity 5-10`**: Realistic erosion patterns
- **`detail-intensity 15-25`**: Enhanced landscape features  
- **`detail-intensity 30+`**: Fantasy/artistic terrain

### Island Removal (`--remove-islands <n>`)

SRTM tiles with sea areas can contain isolated spikes and rock patches in the water. `--remove-islands` removes them before detail generation:

1. Elevation data is normalized (endian, NoData → 0, clamping).
2. All land pixels (`> 0 m`) are grouped into 8-connected components.
3. Components smaller than `<n>` source pixels are set to `0`.

```bash
# Remove isolated patches up to 100 source pixels (~0.8 km² on SRTM-3)
./hgt2png --remove-islands 100 --alpha-nodata --16bit N43W005.hgt
```

**Behavior:**
- **Unit:** `<n>` counts pixels of the original tile, independent of `--scale-factor`.
- **Tile border:** Components touching the tile border are always kept, since they may continue in the neighbouring tile.
- **Inland NoData voids:** Voids are `0` values enclosed by land. They belong to no island and are not touched.
- **Real islands:** Genuine small islands below the threshold are removed as well, so choose `<n>` conservatively.
- **Detail generation:** With `--remove-islands`, procedural noise is skipped where all four source samples of a pixel are at or below 0 m. Without this, noise on the open sea would create new phantom islands after the removal step. Coastlines and land are unaffected.
- **Range:** `1` to `100000000`. Default: disabled; output is then identical to previous versions.
- **Memory:** about 5 bytes per source pixel (visited mask + queue), freed after processing.

**Parameter Guidelines:**
- **`remove-islands 10-50`**: Single-pixel artifacts and tiny spikes
- **`remove-islands 100-1000`**: Small rock patches in the sea
- **`remove-islands 5000+`**: May remove real islands, check the result

### Latitude Aridity (`--lat-aridity <list>`, `--aridity <f>`)

The vegetation mask normally depends on elevation, slope, aspect and drainage only. With aridity, the latitude of each pixel row also reduces vegetation, e.g. to contrast the humid north of the Iberian Peninsula with the dry south.

```bash
# Cantabria (humid) -> Meseta -> Andalusia (dry), one run per tile
./hgt2png --vegetation-mask --biome temperate \
          --lat-aridity 43.5:0.1,40:0.5,36:0.9 N40W006.hgt

# Constant aridity, independent of latitude
./hgt2png --vegetation-mask --aridity 0.6 N40W006.hgt
```

**Behavior:**
- **Values:** `0` = humid (no effect), `1` = arid. `--lat-aridity` takes control points as `<lat>:<aridity>`, separated by commas, in any order. Latitudes must be unique (-90 to 90), at most 16 points.
- **Interpolation:** Linear between the points, constant beyond the first and last point. The latitude is computed per pixel row from the tile name (`N40W006` = 40° to 41° N), so the curve is continuous across tile borders.
- **Effect at aridity 1.0:** Tree, bush and grass line drop by 400 m and the density is multiplied by 0.3 (`1 - 0.7 * aridity`). The factor is applied before the density is limited to its maximum, so relief structure (valleys, slopes) stays visible instead of being flattened to full density. Values in between scale linearly.
- **Combination:** `--aridity` replaces `--lat-aridity` and vice versa; the last one on the command line wins.
- **Default:** Disabled; the mask is then identical to previous versions (`--aridity 0` gives the same result).
- **Tile names:** The latitude is read from names like `N40W006.hgt`. Without a valid name, a warning is printed and aridity is ignored.
- **Verification:** With verbose output, the mask step prints the latitude and aridity of the first and last row.
- **Biomes:** Applies to the Alpine and Temperate curves. Tropical, desert and arctic currently use the Alpine curve and are affected accordingly.

---

## 🎨 Professional Workflow Integration

### Blender Python Integration

#### Automatic Heightmap Import with Metadata
```python
import json
import bpy
import bmesh
from mathutils import Vector

def import_heightmap_with_metadata(hgt_path, json_path):
    """Import HGT2PNG output with real-world scaling"""
    
    # Load metadata
    with open(json_path, "r") as f:
        metadata = json.load(f)
    
    # Create high-resolution plane
    bpy.ops.mesh.primitive_plane_add()
    terrain_obj = bpy.context.active_object
    terrain_obj.name = f"Terrain_{metadata['source_file']}"
    
    # Add subdivision for displacement detail
    subdivision_mod = terrain_obj.modifiers.new(name="Subdivision", type='SUBSURF')
    subdivision_mod.levels = 6  # 2^6 = 64x subdivision
    
    # Real-world scaling
    bounds = metadata["spatial_info"]["geographic_bounds"]
    meters_per_degree = 111000  # Approximate
    
    real_width = (bounds["east"] - bounds["west"]) * meters_per_degree
    real_height = (bounds["north"] - bounds["south"]) * meters_per_degree
    terrain_obj.scale = (real_width/2, real_height/2, 1)
    
    # Create material with heightmap
    material = bpy.data.materials.new(name=f"Material_{terrain_obj.name}")
    terrain_obj.data.materials.append(material)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    
    # Image texture node
    texture_node = nodes.new(type='ShaderNodeTexImage')
    texture_node.image = bpy.data.images.load(hgt_path.replace('.hgt', '.png'))
    texture_node.image.colorspace_settings.name = 'Linear'  # Important for displacement
    
    # Displacement node
    displacement_node = nodes.new(type='ShaderNodeDisplacement')
    material.node_tree.links.new(texture_node.outputs['Color'], displacement_node.inputs['Height'])
    
    # Material output
    output_node = nodes.new(type='ShaderNodeOutputMaterial')
    material.node_tree.links.new(displacement_node.outputs['Displacement'], output_node.inputs['Displacement'])
    
    # Set displacement strength based on real elevation range
    elevation_range = metadata["elevation_range"]["max_meters"] - metadata["elevation_range"]["min_meters"]
    displacement_node.inputs['Scale'].default_value = elevation_range
    
    # Apply displacement modifier
    displacement_mod = terrain_obj.modifiers.new(name="Heightmap", type='DISPLACE')
    displacement_mod.texture_coords = 'UV'
    displacement_mod.strength = 1.0  # Controlled by material node

# Usage example
import_heightmap_with_metadata("N49E004.hgt", "N49E004.json")
```

#### Vegetation Mask Integration (NEW! 🌲)
```python
def setup_vegetation_material(terrain_obj, heightmap_path, vegetation_mask_path, biome_type="alpine"):
    """Create PBR material with vegetation mask mixing for Alpine or Temperate biomes"""
    
    material = terrain_obj.data.materials[0]  # Assume heightmap material exists
    nodes = material.node_tree.nodes
    
    # Load vegetation mask
    vegetation_node = nodes.new(type='ShaderNodeTexImage')
    vegetation_node.image = bpy.data.images.load(vegetation_mask_path)
    vegetation_node.location = (-400, 0)
    
    # Rock material (base)
    rock_bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    rock_bsdf.inputs['Base Color'].default_value = (0.3, 0.25, 0.2, 1.0)  # Brown rock
    rock_bsdf.inputs['Roughness'].default_value = 0.8
    rock_bsdf.location = (0, 200)
    
    # Vegetation material (biome-specific colors)
    vegetation_bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
    if biome_type == "alpine":
        # Alpine: Coniferous forest green
        vegetation_bsdf.inputs['Base Color'].default_value = (0.1, 0.3, 0.1, 1.0)
    elif biome_type == "temperate":
        # Temperate: Deciduous forest green (brighter)
        vegetation_bsdf.inputs['Base Color'].default_value = (0.2, 0.4, 0.15, 1.0)
    vegetation_bsdf.inputs['Roughness'].default_value = 0.9
    vegetation_bsdf.location = (0, -200)
    
    # Mix shader based on vegetation mask
    mix_shader = nodes.new(type='ShaderNodeMixShader')
    material.node_tree.links.new(vegetation_node.outputs['Color'], mix_shader.inputs['Fac'])
    material.node_tree.links.new(rock_bsdf.outputs['BSDF'], mix_shader.inputs[1])
    material.node_tree.links.new(vegetation_bsdf.outputs['BSDF'], mix_shader.inputs[2])
    
    # Connect to output
    output_node = nodes.get('Material Output')
    material.node_tree.links.new(mix_shader.outputs['Shader'], output_node.inputs['Surface'])

# Usage with Alpine biome (mountains)
setup_vegetation_material(terrain_obj, "N46E007.png", "N46E007_vegetation_alpine.png", "alpine")

# Usage with Temperate biome (lowlands)
setup_vegetation_material(terrain_obj, "N49E004.png", "N49E004_vegetation_temperate.png", "temperate")
```

### Unity Engine Integration

#### Automatic Terrain Creation
```csharp
using UnityEngine;
using UnityEditor;
using System.IO;
using Newtonsoft.Json;

[System.Serializable]
public class HGT2PNGMetadata
{
    public string source_file;
    public string png_file;
    public ElevationRange elevation_range;
    public SpatialInfo spatial_info;
    public ProcessingInfo processing_info;
    
    [System.Serializable]
    public class ElevationRange
    {
        public float min_meters;
        public float max_meters;
    }
    
    [System.Serializable]
    public class SpatialInfo
    {
        public float pixel_pitch_meters;
        public int width_pixels;
        public int height_pixels;
    }
    
    [System.Serializable]
    public class ProcessingInfo
    {
        public int scale_factor;
        public float detail_intensity;
    }
}

public class HGT2PNGImporter : MonoBehaviour
{
    public static TerrainData CreateTerrainFromHGT2PNG(string pngPath, string jsonPath)
    {
        // Load metadata
        string jsonContent = File.ReadAllText(jsonPath);
        HGT2PNGMetadata metadata = JsonConvert.DeserializeObject<HGT2PNGMetadata>(jsonContent);
        
        // Load heightmap texture
        Texture2D heightmapTexture = AssetDatabase.LoadAssetAtPath<Texture2D>(pngPath);
        
        // Create terrain data
        TerrainData terrainData = new TerrainData();
        terrainData.name = Path.GetFileNameWithoutExtension(metadata.source_file);
        
        // Set dimensions based on real-world data
        float terrainWidth = metadata.spatial_info.pixel_pitch_meters * metadata.spatial_info.width_pixels;
        float terrainLength = metadata.spatial_info.pixel_pitch_meters * metadata.spatial_info.height_pixels;
        float terrainHeight = metadata.elevation_range.max_meters - metadata.elevation_range.min_meters;
        
        terrainData.size = new Vector3(terrainWidth, terrainHeight, terrainLength);
        terrainData.heightmapResolution = metadata.spatial_info.width_pixels;
        
        // Import heightmap from 16-bit PNG
        Color[] pixels = heightmapTexture.GetPixels();
        float[,] heights = new float[metadata.spatial_info.width_pixels, metadata.spatial_info.height_pixels];
        
        for (int y = 0; y < metadata.spatial_info.height_pixels; y++)
        {
            for (int x = 0; x < metadata.spatial_info.width_pixels; x++)
            {
                Color pixel = pixels[y * metadata.spatial_info.width_pixels + x];
                heights[y, x] = pixel.r; // Use red channel for 16-bit data
            }
        }
        
        terrainData.SetHeights(0, 0, heights);
        
        return terrainData;
    }
    
    // Create terrain with vegetation mask (Alpine or Temperate)
    public static void SetupVegetationMaterial(Terrain terrain, string vegetationMaskPath, string biomeType = "alpine")
    {
        Texture2D vegetationMask = AssetDatabase.LoadAssetAtPath<Texture2D>(vegetationMaskPath);
        
        // Setup terrain layers for rock and vegetation
        TerrainLayer rockLayer = new TerrainLayer();
        rockLayer.diffuseTexture = Resources.Load<Texture2D>("Textures/Rock_Diffuse");
        rockLayer.normalMapTexture = Resources.Load<Texture2D>("Textures/Rock_Normal");
        
        TerrainLayer vegetationLayer = new TerrainLayer();
        if (biomeType == "alpine")
        {
            // Alpine: Coniferous forest textures
            vegetationLayer.diffuseTexture = Resources.Load<Texture2D>("Textures/Coniferous_Diffuse");
            vegetationLayer.normalMapTexture = Resources.Load<Texture2D>("Textures/Coniferous_Normal");
        }
        else if (biomeType == "temperate")
        {
            // Temperate: Deciduous forest textures
            vegetationLayer.diffuseTexture = Resources.Load<Texture2D>("Textures/Deciduous_Diffuse");
            vegetationLayer.normalMapTexture = Resources.Load<Texture2D>("Textures/Deciduous_Normal");
        }
        
        terrain.terrainData.terrainLayers = new TerrainLayer[] { rockLayer, vegetationLayer };
        
        // Apply vegetation mask as alpha map
        Color[] maskPixels = vegetationMask.GetPixels();
        float[,,] alphaMap = new float[vegetationMask.width, vegetationMask.height, 2];
        
        for (int y = 0; y < vegetationMask.height; y++)
        {
            for (int x = 0; x < vegetationMask.width; x++)
            {
                float vegetationDensity = maskPixels[y * vegetationMask.width + x].r;
                alphaMap[x, y, 0] = 1.0f - vegetationDensity;  // Rock (inverse)
                alphaMap[x, y, 1] = vegetationDensity;         // Vegetation
            }
        }
        
        terrain.terrainData.SetAlphamaps(0, 0, alphaMap);
    }
}
```

### Unreal Engine 5 Integration

#### Landscape Import with World Partition
```cpp
// C++ header for UE5 landscape import
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Landscape/Classes/Landscape.h"
#include "Json.h"

USTRUCT()
struct FHGT2PNGMetadata
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString source_file;
    
    UPROPERTY()
    float pixel_pitch_meters;
    
    UPROPERTY()
    int32 width_pixels;
    
    UPROPERTY()
    int32 height_pixels;
    
    UPROPERTY()
    float min_elevation_meters;
    
    UPROPERTY()
    float max_elevation_meters;
};

class TERRAIN_API HGT2PNGImporter
{
public:
    static ALandscape* CreateLandscapeFromHGT2PNG(
        UWorld* World,
        const FString& HeightmapPath,
        const FString& MetadataPath,
        const FString& VegetationMaskPath = TEXT("")
    );
    
private:
    static FHGT2PNGMetadata LoadMetadata(const FString& JsonPath);
    static TArray<uint16> LoadHeightmapData(const FString& PngPath);
    static void SetupVegetationMaterial(ALandscape* Landscape, const FString& VegetationMaskPath);
};
```

---

## 📊 JSON Metadata Schema

### Complete Metadata Structure
```json
{
  "$schema": "https://schemas.hgt2png.org/v1.1.0/metadata.json",
  "version": "1.1.0",
  "source_file": "N49E004.hgt",
  "png_file": "N49E004.png",
  "vegetation_mask": "N49E004_vegetation_alpine.png",
  "timestamp": "2025-11-05T10:30:00Z",
  
  "elevation_range": {
    "min_meters": 54,
    "max_meters": 502,
    "effective_min": 54,
    "effective_max": 502,
    "nodata_count": 0
  },
  
  "spatial_info": {
    "pixel_pitch_meters": 90.0,
    "width_pixels": 10803,
    "height_pixels": 10803,
    "world_size_meters": {
      "width": 324090.0,
      "height": 324090.0
    },
    "geographic_bounds": {
      "south": 49.0,
      "north": 50.0,
      "west": 4.0,
      "east": 5.0,
      "datum": "WGS84"
    }
  },
  
  "processing_info": {
    "scale_factor": 3,
    "detail_intensity": 15.0,
    "noise_seed": 12345,
    "output_format": "16bit_grayscale_alpha",
    "curve_type": "linear",
    "gamma": 1.0,
    "threads_used": 8,
    "processing_time_seconds": 14.8
  },
  
  "vegetation_info": {
    "biome_type": "alpine",  // or "temperate"
    "enabled": true,
    "parameters": {
      "min_elevation": 700.0,
      "max_elevation": 2400.0,
      "tree_line": 1800.0,
      "bush_line": 2200.0,
      "grass_line": 2400.0,
      "max_slope_degrees": 45.0
    },
    "coverage_statistics": {
      "total_vegetation_pixels": 42568920,
      "vegetation_coverage_percent": 36.7,
      "average_density": 127
    }
  }
}
```

---

## 🎯 Advanced Production Workflows

### Hollywood VFX Pipeline
```bash
# Ultra-high quality for film production (Alpine mountains)
./hgt2png --16bit --alpha-nodata --vegetation-mask --biome alpine \
          --scale-factor 4 --detail-intensity 8.0 --gamma 1.0 \
          --metadata json --threads 16 terrain.hgt

# Temperate lowland terrain for establishing shots
./hgt2png --16bit --alpha-nodata --vegetation-mask --biome temperate \
          --scale-factor 4 --detail-intensity 8.0 --gamma 1.0 \
          --metadata json --threads 16 lowlands.hgt

# Batch process for large territories  
find /srtm_data -name "*.hgt" | parallel -j 4 \
  "./hgt2png --16bit --vegetation-mask --biome alpine --metadata json {}"
```

### Game Development AAA Pipeline
```bash
# Optimized for real-time engines (Alpine)
./hgt2png --scale-factor 2 --detail-intensity 18.0 --alpha-nodata \
          --vegetation-mask --biome alpine --metadata json mountains.hgt

# Optimized for real-time engines (Temperate)
./hgt2png --scale-factor 2 --detail-intensity 18.0 --alpha-nodata \
          --vegetation-mask --biome temperate --metadata json plains.hgt

# Generate LOD versions
./hgt2png --scale-factor 1 --disable-detail --quiet terrain.hgt  # LOD0 (distant)
mv terrain.png terrain_lod0.png
```

### Scientific Research Workflow
```bash
# Maximum precision for analysis
./hgt2png --16bit --disable-detail --curve linear --gamma 1.0 \
          --min-height 0 --max-height 4000 --metadata json terrain.hgt

# Generate analysis masks for different biomes
./hgt2png --vegetation-mask --biome alpine --metadata json mountains.hgt
./hgt2png --vegetation-mask --biome temperate --metadata json lowlands.hgt

# Compare vegetation distribution across biomes
./hgt2png --vegetation-mask --biome alpine N46E007.hgt    # Alps
./hgt2png --vegetation-mask --biome temperate N49E004.hgt # Central Europe
```

---
