# HGT2PNG Blender Integration v2.0

## 🎯 **Übersicht**
Das erweiterte `blender_dyer.py` Script bietet professionelle automatische Terrain-Färbung für Heightmaps in Blender mit **hgt2png Integration**, **Biom-spezifischen Farbpaletten**, und **Neigungsanalyse** für realistische Landschaftsvisualisierung.

## 🚀 **Neue Features v2.0**

### ✨ **Automatische Integration**
- **Automatische Heightmap-Erkennung**: Findet Terrain-Objekte automatisch
- **hgt2png JSON-Metadaten Support**: Lädt präzise Höhenbereiche und geografische Daten
- **Displacement-Modifier Integration**: Berücksichtigt Blender Displacement-Skalierung
- **Legacy-Kompatibilität**: Unterstützt alte Script-Versionen

### 🌍 **4 Biom-spezifische Farbpaletten**
```python
'forest'   → Temperater Wald (Beige→Grün→Dunkelgrün)
'alpine'   → Hochgebirge (Braun→Gras→Fels→Grau→Schnee)
'desert'   → Wüste (Hell-→Dunkel-Sand→Fels)
'tropical' → Regenwald (Strand→Tropengrün→Dschungel)
```

### ⛰️ **Intelligente Neigungsanalyse**
- **Slope-basierte Farbmodifikation**: Steile Hänge → Fels/Geröll-Farben
- **Terrain-spezifische Anpassung**: Alpine Gebiete mit realistischer Fels-Verteilung
- **Einstellbare Intensität**: Slope-Effekt von subtil bis dramatisch

### 🔄 **Batch-Processing**
- **Mehrere Heightmaps**: Verarbeitet alle Terrain-Objekte gleichzeitig
- **Konsistente Paletten**: Einheitliche Farbgebung über multiple Objekte

## 📖 **Verwendung**

### **🚀 Schnellstart (Empfohlen)**
```python
# Automatische Erkennung mit modernem Wald-Biom
color_heightmap_faces_advanced()
```

### **🌲 Verschiedene Biome**
```python
color_heightmap_faces_advanced(biome='forest')      # Temperater Wald
color_heightmap_faces_advanced(biome='alpine')      # Hochgebirge
color_heightmap_faces_advanced(biome='desert')      # Wüste
color_heightmap_faces_advanced(biome='tropical')    # Regenwald
```

### **⛰️ Mit Neigungsanalyse**
```python
# Realistische Fels-Bereiche an steilen Hängen
color_heightmap_faces_advanced(
    biome='alpine', 
    use_slope=True, 
    slope_intensity=0.5
)
```

### **📊 Mit hgt2png Metadaten**
```python
# Lädt automatisch JSON-Metadaten für präzise Höhenbereiche
color_heightmap_faces_advanced(use_metadata=True)
```

### **🔄 Batch-Verarbeitung**
```python
# Alle Heightmaps in der Szene verarbeiten
batch_process_heightmaps(biome='forest', use_slope=True)
```

## 🛠️ **HGT2PNG → Blender Workflow**

### **1. Heightmap mit Metadaten generieren**
```bash
# In hgt2png Verzeichnis:
./hgt2png --16bit --metadata json --scale-factor 3 terrain.hgt
# Erzeugt: terrain.png + terrain.json
```

### **2. In Blender importieren**
1. **Plane erstellen** und subdivision
2. **Displacement Modifier** hinzufügen
3. **16-bit PNG** als Displacement Texture laden
4. **blender_dyer.py** ausführen

### **3. Automatische Färbung**
```python
# Script in Blender ausführen - erkennt automatisch:
# - JSON-Metadaten (terrain.json)  
# - Displacement-Skalierung
# - Terrain-Objekt (Plane/Heightmap)
color_heightmap_faces_advanced(biome='alpine', use_slope=True)
```

## 📊 **Metadaten-Integration**

### **JSON-Format (hgt2png v1.1.0)**
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

### **Automatische Erkennung**
- Sucht JSON-Dateien mit gleichem Namen wie Heightmap-Objekt
- Fallback: Alle JSON-Dateien im Blend-Verzeichnis scannen
- Legacy-Format Unterstützung für ältere hgt2png Versionen

## 🎨 **Biom-Details**

### **🌲 Forest (Temperater Wald)**
```
0-20%: Beige (Talsohlen/Geröll)
20-40%: Helles Grasgrün  
40-60%: Mittleres Grün (Wiesen)
60-80%: Waldgrün (Dichte Wälder)
80-100%: Dunkelgrün (Nadelwälder)
```

### **🏔️ Alpine (Hochgebirge)**
```
0-20%: Braune Täler
20-40%: Alpengras  
40-60%: Felsbraun
60-80%: Grauer Fels
80-100%: Schnee/Eis
```

### **🏜️ Desert (Wüste)**
```
0-20%: Heller Sand
20-40%: Mittlerer Sand
40-60%: Dunkler Sand  
60-80%: Felsbraun
80-100%: Dunkler Fels
```

### **🌺 Tropical (Regenwald)**
```
0-20%: Strand/Küste
20-40%: Helles Tropengrün
40-60%: Dichtes Grün
60-80%: Regenwald  
80-100%: Dichter Dschungel
```

## ⚙️ **Erweiterte Parameter**

### **color_heightmap_faces_advanced() Parameter:**
```python
obj_name=None,           # Objekt-Name (None=auto)
biome='forest',          # Biom-Typ  
use_slope=True,          # Neigungsanalyse
use_metadata=True,       # JSON-Metadaten laden
slope_intensity=0.3      # Neigungseffekt-Stärke (0.0-1.0)
```

### **Neigungsanalyse:**
- `slope_intensity=0.0`: Keine Neigungseffekte
- `slope_intensity=0.3`: Subtile Fels-Bereiche (Standard)
- `slope_intensity=0.7`: Deutliche Fels-Strukturen
- `slope_intensity=1.0`: Dramatische Fels-Dominanz

## 🔧 **Material-System**
- **Automatisches Material-Setup**: Erstellt Shader-Knoten automatisch
- **Face Color Attributes**: Nutzt moderne Blender Color Attribute Pipeline
- **Multi-Material Support**: Verschiedene Biom-Materialien parallel

## 🚨 **Fehlerbehebung**

### **Keine Färbung sichtbar:**
- Stelle sicher, dass Viewport Shading auf "Material Preview" oder "Rendered" steht
- Prüfe, ob das Material korrekt zugewiesen wurde

### **Falsche Höhenbereiche:**
```python
# Erzwinge manuelle Z-Koordinaten-Analyse
color_heightmap_faces_advanced(use_metadata=False)
```

### **JSON-Metadaten nicht gefunden:**
- Stelle sicher, dass JSON-Datei im gleichen Verzeichnis wie .blend liegt
- Prüfe Dateinamen: `terrain.hgt` → `terrain.json`

## 💡 **Performance-Tipps**
- **Neigungsanalyse** bei großen Meshes (>1M Faces) deaktivieren für bessere Performance
- **Batch-Processing** für konsistente Ergebnisse bei mehreren Heightmaps
- **16-bit PNG** Displacement Maps für beste Qualität verwenden

## 🔄 **Legacy-Kompatibilität**
```python
# Alte Funktion funktioniert weiterhin:
color_heightmap_faces_five_colors_green("Plane")
# → Ruft automatisch neue Funktion mit forest-Biom auf
```

---

**Entwickelt für hgt2png v1.1.0 | Blender 3.0+ | Python 3.8+**