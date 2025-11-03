# 🌲 Vegetation Mask Generation - Alpine Biome

## Overview

The HGT2PNG Vegetation Mask Generator creates realistic vegetation density maps from SRTM elevation data. Based on scientific Alpine vegetation zone models, it generates grayscale PNG masks where brightness represents vegetation density (0-255).

## Features

### 🏔️ **Alpine Biome (Implemented)**
- **Elevation-based vegetation zones**
- **Slope analysis** (max 60° for vegetation)
- **Aspect consideration** (South faces drier, North faces moister) 
- **Drainage analysis** (valleys more vegetated than ridges)
- **Realistic European Alps parameters**

### 🌿 **Vegetation Zones**
1. **Montane Forest Zone** (700-1800m): Dense coniferous forests
2. **Subalpine Zone** (1800-2200m): Shrubs and dwarf trees
3. **Alpine Zone** (2200-2500m): Alpine meadows and cushion plants
4. **Nival Zone** (>2500m): No vegetation (permanent snow/ice)

## Usage

### Basic Command
```bash
./hgt2png --vegetation-mask --biome alpine terrain.hgt
```

### Advanced Usage
```bash
# With metadata and high detail
./hgt2png --vegetation-mask --biome alpine --metadata json --scale-factor 2 N46E007.hgt

# Parallel processing for multiple files
./hgt2png --vegetation-mask --biome alpine --threads 8 filelist.txt
```

### Output Files
- `terrain.png` - Standard heightmap
- `terrain_vegetation_alpine.png` - Vegetation density mask

## Parameters (Alpine Biome)

| Parameter | Value | Description |
|-----------|--------|-------------|
| **Min Elevation** | 700m | Vegetation starts (montane zone) |
| **Max Elevation** | 2000m | Vegetation ends (above tree line) |
| **Tree Line** | 1800m | Spruce/Larch limit |
| **Bush Line** | 2200m | Dwarf pine/Rhododendron limit |
| **Grass Line** | 2500m | Alpine meadows limit |
| **Max Slope** | 60° | Maximum slope for vegetation |
| **Aspect Modifier** | 0.3 | South faces drier factor |
| **Drainage Bonus** | 0.4 | Valley vegetation bonus |

## Algorithm Details

### 1. **Elevation Factor**
```
Montane (700-1800m):  70-100% density
Subalpine (1800-2200m): 30-70% density  
Alpine (2200-2500m):   10-30% density
Nival (>2500m):        0% density
```

### 2. **Slope Factor**
```
0-30°:     100% factor
30-60°:    Linear reduction to 20%
>60°:      0% factor (too steep)
```

### 3. **Aspect Factor**
```
North faces (315-45°):   +30% vegetation
South faces (135-225°):  -30% vegetation
East/West faces:         Neutral
```

### 4. **Drainage Factor**
```
Valley bottoms: +40% vegetation bonus
Ridges:         No bonus
Flat areas:     Neutral
```

### 5. **Final Calculation**
```c
final_density = elevation_factor × slope_factor × aspect_factor × drainage_factor
grayscale_value = (uint8_t)(final_density × 255)
```

## Integration with Blender

### Using with blender_dyer.py
```python
# Load terrain and create PBR material
create_pbr_terrain_material(biome='alpine', use_advanced_mixing=True)

# The vegetation mask can be used as:
# 1. Mix factor for material blending
# 2. Density input for particle systems
# 3. Weight map for geometry nodes
```

### Manual Blender Workflow
1. Import heightmap PNG as Displacement modifier
2. Import vegetation mask PNG as Image Texture
3. Use mask as Factor for mixing:
   - **White areas**: Dense vegetation (trees/grass)
   - **Black areas**: Bare rock/snow
   - **Gray areas**: Sparse vegetation

## Validation Data

Based on **European Alps ecological research**:
- Tree line elevation matches field observations
- Slope limits verified against vegetation surveys  
- Aspect effects consistent with microclimate studies
- Drainage patterns match Alpine valley ecosystems

## Future Biomes (Planned)

### 🌲 **Temperate Forest**
- Lower elevation limits (200-1500m)
- Different tree species composition
- Seasonal deciduous effects

### 🌺 **Tropical Rainforest** 
- Sea level to 3000m vegetation
- Humidity-based factors
- Cloud forest transitions

### 🏜️ **Desert**
- Extreme elevation ranges
- Water availability modeling
- Rare precipitation effects

### ❄️ **Arctic Tundra**
- Permafrost considerations
- Short growing seasons
- Wind exposure effects

## Technical Specifications

### Performance
- **OpenMP parallelized** for multi-core processing
- **Memory efficient** - processes row by row
- **SIMD optimized** gradient calculations

### Output Format
- **8-bit grayscale PNG** (0-255)
- **Same resolution** as input heightmap
- **Lossless compression** for precise values

### Quality Assurance
- **Boundary handling** for edge pixels
- **NoData value support** (transparent in mask)
- **Range validation** for all parameters

## Examples

### Swiss Alps (N46E007.hgt)
```bash
./hgt2png --vegetation-mask --biome alpine N46E007.hgt
```
**Expected results:**
- Rhône Valley: Dense vegetation (bright areas)
- Matterhorn peaks: No vegetation (black areas)  
- Mid-elevation slopes: Gradual transitions (gray areas)

### Austrian Alps (N47E012.hgt)
```bash
./hgt2png --vegetation-mask --biome alpine --metadata json N47E012.hgt
```
**Expected results:**
- Inn Valley: Agricultural vegetation
- Stubai Alps: Alpine meadows and rock
- Glaciated areas: No vegetation

## Troubleshooting

### Common Issues
1. **All black output**: Check elevation range in metadata
2. **All white output**: Verify biome parameters match terrain
3. **Compilation errors**: Ensure libpng and OpenMP installed

### Debug Information
```bash
# Verbose output for debugging
./hgt2png --vegetation-mask --biome alpine -v terrain.hgt

# Check metadata for elevation ranges
./hgt2png --metadata json terrain.hgt
```

## Scientific References

1. **Alpine Vegetation Zones**: Körner, C. (2003). Alpine Plant Life
2. **Tree Line Ecology**: Holtmeier, F. K. (2009). Mountain Timberlines  
3. **Slope-Vegetation Relationships**: Beniston, M. (2003). Climate Change in Mountain Regions
4. **Aspect Effects**: Barry, R. G. (2008). Mountain Weather and Climate

---
*HGT2PNG v1.1.0 - Professional Heightmap Converter with Vegetation Mask Generation*