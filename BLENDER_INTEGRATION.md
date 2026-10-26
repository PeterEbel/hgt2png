# Blender Integration

## Batch rendering with `render.py`

`render.py` renders every heightmap PNG in a directory. The `TerrainMaterial` (sea shader, heightmap coloring, vegetation mask, ColorRamp) is created on the fly, so the .blend file only needs:

- World and camera
- A subdivided `Plane` with a `Displace` modifier

### 1. Generate heightmap and vegetation mask

```bash
hgt2png -V --biome temperate --alpha-nodata --16bit --lat-aridity 43.5:0.1,40:0.5,36:0.9 ../hgt/N37W007.hgt
```

Use `--16bit --alpha-nodata` with hgt2png 1.5.2 or newer, so NoData pixels keep gray value 0 under alpha 0 (rendered as sea).

### 2. Render

```bash
blender terrain.blend --background --python scripts/render.py -- --samples 1000 --resolution 7202
```

Everything after `--` is passed to the script. [scripts/render.sh](scripts/render.sh) wraps the call and forwards its arguments.

| Option | Default | Description |
|---|---|---|
| `--input-dir` | see script | Directory with `<tile>.png` and `<tile>_vegetation_<biome>.png` |
| `--output-dir` | see script | Output directory for `<tile>_render.png` (created if missing) |
| `--biome` | `temperate` | Biome of the vegetation masks |
| `--samples` | `1000` | Cycles samples |
| `--resolution` | `7202` | Edge length in pixels (square) |
| `--bare-ground` | `0.2` | ColorRamp position (0..1) for areas without vegetation (mask = 0); rises with elevation up to 1.0 |

### Behavior

- Vegetation masks and `*_render.png` files are ignored when listing heightmaps.
- The vegetation mask drives the Mix factor, the elevation-stretched heightmap color goes to A, the heightmap color to B, and the result is the ColorRamp `Fac`.
- The heightmap alpha mixes the sea shader (Diffuse) with the land shader (Principled BSDF).
- Without a mask file the tile is rendered with a white mask, which equals pure height coloring.
- An existing `TerrainMaterial` in the .blend file is replaced.
