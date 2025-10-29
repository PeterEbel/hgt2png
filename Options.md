# HGT2PNG - Kommandozeilen-Optionen Detailhandbuch

## Übersicht

HGT2PNG v1.1.0 ist ein professioneller Heightmap-Converter, der SRTM HGT-Dateien in PNG-Bilder konvertiert mit erweiterten Features für 3D-Workflows, Spieleentwicklung und GIS-Anwendungen.

## Basis-Syntax

```bash
./hgt2png [OPTIONEN] <input.hgt|filelist.txt>
```

---

## 🚀 Performance & Qualitäts-Optionen

### `-s, --scale-factor <n>` (Standard: 3)


**Werte:**
- `1` = Original-Auflösung (3601×3601 für SRTM-1)
- `2` = Doppelte Auflösung (7202×7202)
- `3` = Dreifache Auflösung (10803×10803) **[Empfohlen]**
- `4+` = Höhere Werte möglich (exponentiell mehr Rechenzeit)

**Anwendung:**
- **3D-Modellierung:** Höhere Werte für detailliertere Displacement Maps
- **Spiele-Engine:** `2-3` für realistische Terrain-Meshes
- **Schnelle Previews:** `1` für maximale Geschwindigkeit

```bash
# Beispiele
./hgt2png -s 4 terrain.hgt          # Sehr hochwertig, braucht Zeit
```

### `-i, --detail-intensity <f>` (Standard: 15.0)

**Zweck:** Steuert die Stärke der hinzugefügten prozeduralen Details in Metern

**Werte:**
- `5.0-10.0` = Subtile Details (realistische Erosion)
- `15.0-25.0` = Moderate Details **[Empfohlen]**
- `30.0+` = Dramatische Details (Fantasy-Landschaften)

**Technischer Hintergrund:**
Das System fügt mehrere Noise-Oktaven hinzu:
- **Große Features** (0.005× Frequenz): Generelle Landschaftsformen
- **Mittlere Features** (0.02× Frequenz): Hügel und Täler
- **Feine Details** (0.08× Frequenz): Oberflächentextur

```bash
# Beispiele
./hgt2png -i 8.0 terrain.hgt        # Realistische Erosion
./hgt2png -i 20.0 terrain.hgt       # Dramatische Landschaft
./hgt2png -i 35.0 terrain.hgt       # Fantasy/Alien-Terrain
```

### `-r, --noise-seed <n>` (Standard: 12345)

**Zweck:** Deterministischer Zufalls-Seed für reproduzierbare Ergebnisse

**Anwendung:**
- **Konsistenz:** Gleicher Seed = identische Details bei mehrfachen Runs
- **Variation:** Verschiedene Seeds = unterschiedliche Detail-Muster
- **Workflow:** Seed dokumentieren für Projektdokumentation

```bash
# Beispiele
./hgt2png -r 42 terrain.hgt         # Seed 42 für Projekt A
./hgt2png -r 1337 terrain.hgt       # Seed 1337 für Projekt B
```

---

## ⚡ Parallel-Processing

### `-t, --threads <n>` (Standard: 4)

**Zweck:** Nutzt mehrere CPU-Kerne für OpenMP-Parallelisierung

**Werte:**
- `1` = Sequenziell (nur bei Debugging)
- `2-8` = Optimal für die meisten Systeme
- `16` = Maximum (für High-End-Workstations)

**Performance-Impact:**
- **SIMD-Vektorisierung:** Nutzt AVX2 für 8 Pixel parallel
- **Multi-Threading:** Zeilen werden auf Kerne verteilt
- **Erwarteter Speedup:** ~4-8× bei 8 Kernen

```bash
# Beispiele
./hgt2png -t 1 terrain.hgt          # Debug-Modus, single-threaded
./hgt2png -t 8 terrain.hgt          # Vollständige CPU-Nutzung
OMP_NUM_THREADS=8 ./hgt2png terrain.hgt  # Alternative per Umgebungsvariable
```

---

## 🎨 Ausgabe-Formate & Qualität

### `--16bit`

**Zweck:** Generiert 16-Bit Grayscale PNG (0-65535 statt 0-255)

**Vorteile:**
- **Höhere Präzision:** 256× mehr Höhenstufen
- **Displacement Maps:** Ideal für 3D-Software (Blender, Maya, 3ds Max)
- **GIS-Analyse:** Präzise Höhenwerte für wissenschaftliche Auswertungen

**Nachteile:**
- **Dateigröße:** ~2× größer als 8-Bit
- **Kompatibilität:** Nicht alle Viewer zeigen 16-Bit korrekt an

```bash
# Beispiele
./hgt2png --16bit terrain.hgt                    # 16-Bit Graustufen
./hgt2png --16bit --alpha-nodata terrain.hgt     # 16-Bit + Transparenz
```

### `--alpha-nodata`

**Zweck:** NoData-Pixel werden transparent (Alpha=0) statt schwarz

**Anwendung:**
- **Insel-Terrain:** Wasser-Bereiche transparent
- **Tile-Systeme:** Nahtlose Übergänge zwischen Tiles
- **Compositing:** Einfaches Überlagern in Grafikprogrammen

**Technische Details:**
- **8-Bit Modus:** RGBA PNG (4 Kanäle)
- **16-Bit Modus:** 16-Bit Grayscale + 16-Bit Alpha
- **NoData-Werte:** Typisch -32768 in SRTM-Dateien

```bash
# Beispiele
./hgt2png --alpha-nodata terrain.hgt            # Transparente Wasserflächen
./hgt2png --16bit --alpha-nodata terrain.hgt    # Höchste Qualität + Transparenz
```

---

## 📊 Höhenwert-Mapping & Kurven

### `-m, --min-height <n>` und `-M, --max-height <n>`

**Zweck:** Definiert den Höhenbereich für die Pixel-Mapping

**Standard:** Auto-Detection (nutzt tatsächliche Min/Max-Werte der Datei)

**Anwendung:**
- **Konsistenz:** Gleicher Maßstab für mehrere Tiles
- **Kontrast:** Fokus auf interessanten Höhenbereich
- **Normalisierung:** Einheitliche Darstellung verschiedener Regionen

```bash
# Beispiele
./hgt2png -m 0 -M 1000 terrain.hgt              # Meeresspiegel bis 1000m
./hgt2png -m 500 -M 2500 terrain.hgt            # Bergland 500-2500m
./hgt2png terrain.hgt                            # Auto-Erkennung (empfohlen)
```

### `-c, --curve <type>` (Standard: linear)

**Zweck:** Mapping-Kurve zwischen Höhenwerten und Pixel-Helligkeit

#### **linear** (Standard)
- **Funktion:** `pixel = (height - min) / (max - min)`
- **Charakteristik:** Gleichmäßige Verteilung
- **Anwendung:** Wissenschaftliche Auswertung, technische Darstellung

#### **log** (Logarithmisch)
- **Funktion:** `pixel = log(height - min + 1) / log(max - min + 1)`
- **Charakteristik:** Mehr Details in niedrigen Höhen
- **Anwendung:** Küstenregionen, flache Landschaften betonen

```bash
# Beispiele
./hgt2png -c linear terrain.hgt                 # Gleichmäßige Darstellung
./hgt2png -c log terrain.hgt                    # Flachland-Details betonen
```

### `-g, --gamma <f>` (Standard: 1.0, Bereich: 0.1-10.0)

**Zweck:** Gamma-Korrektur für visuelle Anpassung

**Werte:**
- `<1.0` (z.B. 0.5): **Aufhellen** - mehr Details in dunklen Bereichen
- `1.0`: **Linear** - keine Korrektur
- `>1.0` (z.B. 2.2): **Abdunkeln** - mehr Kontrast, photorealistische Darstellung

**Technische Details:**
- **Monitor-Gamma:** Die meisten Displays haben Gamma ~2.2
- **Workflow:** Für Displacement Maps oft Gamma 1.0, für Visualisierung 2.2

```bash
# Beispiele
./hgt2png -g 0.5 terrain.hgt                    # Aufgehellt, mehr Schatten-Details
./hgt2png -g 2.2 terrain.hgt                    # Monitor-Standard, realistisch
./hgt2png -g 1.0 terrain.hgt                    # Linear, für 3D-Software
```

---

## 📄 Metadaten & Workflow-Integration

### `-x, --metadata <format>` (Standard: none)

**Zweck:** Generiert Sidecar-Dateien mit technischen Metadaten

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

### **Blender-Integration mit Metadaten**

#### **1. Displacement Modifier Setup**
```python
# Blender Python Script - Automatische Displacement-Map-Import
import json
import bpy

# Metadaten laden
with open("N49E004.json", "r") as f:
    metadata = json.load(f)

# Plane erstellen mit korrekten Dimensionen
bpy.ops.mesh.primitive_plane_add()
plane = bpy.context.active_object

# Größe basierend auf realen Koordinaten (aus tatsächlicher JSON-Struktur)
bounds = metadata["geographic"]["bounds"]
real_width = (bounds["east"] - bounds["west"]) * 111000  # Grad zu Meter (111km/°)
real_height = (bounds["north"] - bounds["south"]) * 111000

plane.scale = (real_width/2, real_height/2, 1)

# Subdivision für Displacement (wichtig!)
subdivision_mod = plane.modifiers.new(name="Subdivision", type='SUBSURF')
subdivision_mod.levels = 6  # Mehr Geometrie für Details

# Displacement Modifier hinzufügen
displacement_mod = plane.modifiers.new(name="Heightmap", type='DISPLACE')

# Material mit Heightmap-Texture
material = bpy.data.materials.new(name="Terrain")
plane.data.materials.append(material)
material.use_nodes = True

# Image Texture Node
texture_node = material.node_tree.nodes.new(type='ShaderNodeTexImage')
texture_node.image = bpy.data.images.load("N49E004.png")

# ColorRamp für bessere Displacement-Kontrolle
colorramp_node = material.node_tree.nodes.new(type='ShaderNodeValToRGB')
material.node_tree.links.new(texture_node.outputs['Color'], colorramp_node.inputs['Fac'])

# Displacement Output
displacement_output = material.node_tree.nodes.new(type='ShaderNodeDisplacement')
material.node_tree.links.new(colorramp_node.outputs['Color'], displacement_output.inputs['Height'])
material.node_tree.links.new(displacement_output.outputs['Displacement'], 
                             material.node_tree.nodes['Material Output'].inputs['Displacement'])

# Höhen-Skalierung basierend auf realen Daten
elevation = metadata["elevation"]
height_range = elevation["max_meters"] - elevation["min_meters"]
displacement_mod.strength = height_range / 100  # Skalierung für Blender-Units

# Texture zu Displacement verknüpfen
displacement_mod.texture = bpy.data.textures.new("HeightmapTexture", type='IMAGE')
displacement_mod.texture.image = texture_node.image

print(f"Terrain erstellt: {real_width/1000:.1f}km x {real_height/1000:.1f}km")
print(f"Höhenbereich: {elevation['min_meters']}m - {elevation['max_meters']}m")
print(f"Displacement-Stärke: {displacement_mod.strength:.2f}")
```

#### **2. GIS-Workflow Integration**
```bash
# Batch-Processing für große Gebiete
./hgt2png -x json --16bit --alpha-nodata *.hgt

# Metadaten für GIS-Import sammeln
for json_file in *.json; do
    echo "Processing $json_file"
    # GDAL-Tools können JSON-Metadaten für Georeferenzierung nutzen
    # Python-Scripts für automatische Koordinaten-Extraktion
done
```

#### **3. Game Engine Integration (Unity/Unreal)**
```csharp
// Unity C# - Automatische Terrain-Import (korrigierte JSON-Struktur)
using UnityEngine;
using System.IO;

[System.Serializable]
public class ElevationData
{
    public float min_meters;
    public float max_meters;
    public float range_meters;
}

[System.Serializable]
public class ScalingData
{
    public float pixel_pitch_meters;
    public int scale_factor;
    public WorldSize world_size_meters;
}

[System.Serializable]
public class WorldSize
{
    public float width;
    public float height;
}

[System.Serializable]
public class HeightmapMetadata
{
    public string source_file;
    public string png_file;
    public ElevationData elevation;
    public ScalingData scaling;
}

// Terrain aus PNG + JSON erstellen
public void CreateTerrainFromHGT2PNG(string jsonPath, string pngPath)
{
    // Metadaten laden
    string jsonContent = File.ReadAllText(jsonPath);
    HeightmapMetadata meta = JsonUtility.FromJson<HeightmapMetadata>(jsonContent);
    
    // Terrain Data erstellen
    TerrainData terrainData = new TerrainData();
    
    // Auflösung aus PNG ermitteln
    Texture2D heightmap = Resources.Load<Texture2D>(Path.GetFileNameWithoutExtension(pngPath));
    terrainData.heightmapResolution = heightmap.width;
    
    // Real-World-Größe anwenden
    terrainData.size = new Vector3(
        meta.scaling.world_size_meters.width / 1000f,  // Meter zu Unity-Units (km)
        (meta.elevation.max_meters - meta.elevation.min_meters) / 10f,  // Höhe skaliert
        meta.scaling.world_size_meters.height / 1000f
    );
    
    // Heightmap anwenden
    float[,] heights = new float[heightmap.width, heightmap.height];
    for (int x = 0; x < heightmap.width; x++)
    {
        for (int y = 0; y < heightmap.height; y++)
        {
            Color pixel = heightmap.GetPixel(x, y);
            heights[y, x] = pixel.grayscale;  // Normalized height
        }
    }
    terrainData.SetHeights(0, 0, heights);
    
    // Terrain GameObject erstellen
    GameObject terrainGO = Terrain.CreateTerrainGameObject(terrainData);
    terrainGO.name = $"Terrain_{meta.source_file}";
    
    Debug.Log($"Terrain erstellt: {meta.scaling.world_size_meters.width/1000:F1}km x {meta.scaling.world_size_meters.height/1000:F1}km");
    Debug.Log($"Höhenbereich: {meta.elevation.min_meters}m - {meta.elevation.max_meters}m");
}
```

---

## 🛠️ Workflow-Optionen

### `-d, --disable-detail`

**Zweck:** Deaktiviert procedurale Detail-Generierung

**Anwendung:**
- **Schnelle Konvertierung:** Nur Basis-Konvertierung ohne Enhancement
- **Vergleichstests:** Original vs. Enhanced
- **Debugging:** Isolation von Problemen

```bash
# Beispiele
./hgt2png -d terrain.hgt                        # Nur Basis-Konvertierung
./hgt2png -d --16bit terrain.hgt                # 16-Bit ohne Details
```

### `-q, --quiet`

**Zweck:** Unterdrückt verbose Ausgaben (nur Fehler werden angezeigt)

**Anwendung:**
- **Batch-Processing:** Saubere Log-Dateien
- **Automated Scripts:** Reduzierter Output
- **Performance:** Minimaler I/O-Overhead

```bash
# Beispiele
./hgt2png -q terrain.hgt                        # Stille Ausführung
./hgt2png -q *.hgt > processing.log 2>&1        # Log nur Fehler
```

---

## 📁 Eingabe-Formate

### **Einzelne HGT-Datei**
```bash
./hgt2png N49E004.hgt
./hgt2png /path/to/terrain.hgt
```

### **Dateiliste (Batch-Processing)**
```bash
# Erstelle filelist.txt:
echo "N49E004.hgt" > filelist.txt
echo "N49E005.hgt" >> filelist.txt
echo "N50E004.hgt" >> filelist.txt

# Verarbeite alle:
./hgt2png filelist.txt
```

---

## 🎯 Empfohlene Workflows

### **3D-Modeling (Blender/Maya)**
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

## 💡 Tipps & Tricks

1. **Memory Usage:** Bei `scale-factor > 3` auf RAM achten (>8GB empfohlen)
2. **Batch Processing:** Nutze `-q` und Log-Redirection für große Mengen
3. **Konsistenz:** Dokumentiere verwendete Parameter für Projekte
4. **Format-Wahl:** 16-Bit für Produktion, 8-Bit für Previews
5. **Thread-Anzahl:** Meist optimal: Anzahl CPU-Kerne - 1

---

*Diese Dokumentation beschreibt HGT2PNG v1.1.0 mit OpenMP-Optimierung und professionellen 3D-Workflow-Features.*