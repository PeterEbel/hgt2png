# 🎯 BLENDER SCRIPT TEST - Schritt-für-Schritt Anleitung

## ✅ **Vorbereitung abgeschlossen:**
```
📁 Generierte Dateien:
   N49E004.png (7202×7202 16-bit Heightmap)
   N49E004.json (Metadaten mit Höhenbereich 54-502m)
   blender_dyer.py (Erweitertes Färbungs-Script v2.0)
```

## 🚀 **Blender Setup (5 Minuten)**

### **1. Neue Blender-Datei erstellen**
- Blender öffnen
- Standard-Cube löschen (X → Delete)
- **Plane hinzufügen**: `Shift+A` → Mesh → Plane

### **2. Plane für Heightmap vorbereiten**
```
1. Plane auswählen
2. Tab → Edit Mode
3. Rechtsklick → Subdivide
4. Wiederholen bis ~1000-5000 Faces (je nach Performance)
5. Tab → Object Mode zurück
```

### **3. Displacement Modifier hinzufügen**
```
Properties Panel → Modifier Properties (Schraubenschlüssel-Icon)
→ Add Modifier → Displace

Displacement Settings:
- Texture → New → Image Texture
- Open Image → N49E004.png auswählen
- Strength: 0.01 (anpassen je nach gewünschter Höhe)
- Direction: Z
```

### **4. Script ausführen**
```
1. Scripting Tab in Blender
2. Text → Open → blender_dyer.py laden
3. Script ausführen (Play-Button oder Alt+P)
```

## 📝 **Script-Befehle zum Testen**

### **🚀 Automatische Erkennung (Empfohlen)**
```python
# Führe dies im Blender Script-Editor aus:
color_heightmap_faces_advanced()
```

### **🌲 Verschiedene Biome testen**
```python
# Wald (Standard)
color_heightmap_faces_advanced(biome='forest')

# Hochgebirge mit Neigungsanalyse
color_heightmap_faces_advanced(biome='alpine', use_slope=True, slope_intensity=0.5)

# Wüste
color_heightmap_faces_advanced(biome='desert')

# Regenwald
color_heightmap_faces_advanced(biome='tropical')
```

### **⚙️ Erweiterte Optionen**
```python
# Mit Metadaten und Neigungsanalyse
color_heightmap_faces_advanced(
    obj_name="Plane",           # Objekt-Name
    biome='alpine',             # Biom-Typ  
    use_slope=True,             # Neigungsanalyse aktivieren
    use_metadata=True,          # JSON-Metadaten verwenden
    slope_intensity=0.7         # Starke Fels-Effekte
)
```

## 🎨 **Viewport-Einstellungen für beste Sicht**

### **Material Preview Mode**
```
Viewport Shading → Material Preview (3. Icon von links)
oder Taste: 3
```

### **Rendered View**
```
Viewport Shading → Rendered (4. Icon von links) 
oder Taste: 4
```

## 🔧 **Troubleshooting**

### **❌ "Keine Färbung sichtbar"**
```python
# 1. Prüfe Material-Zuweisung
print(bpy.context.object.data.materials)

# 2. Erzwinge Material-Setup
setup_material_nodes(bpy.data.materials["Heightmap_Material_Forest"])

# 3. Viewport auf Material Preview umstellen
```

### **❌ "JSON nicht gefunden"**
```python
# Ohne Metadaten ausführen (verwendet Z-Koordinaten)
color_heightmap_faces_advanced(use_metadata=False)
```

### **❌ "Objekt nicht erkannt"**
```python
# Manuell Objekt-Namen angeben
color_heightmap_faces_advanced(obj_name="Plane")
```

## 📊 **Erwartete Ergebnisse**

### **🌲 Forest Biom**
- Tal-Bereiche: Beige/Sand-Farben
- Mittlere Höhen: Verschiedene Grüntöne
- Höchste Bereiche: Dunkles Nadelwald-Grün

### **🏔️ Alpine Biom (mit Slope)**
- Flache Bereiche: Alpengras (Grün)
- Steile Hänge: Fels-Farben (Grau/Braun)
- Höchste Gipfel: Schnee (Weiß)

### **Console-Output**
```
🎯 HGT2PNG Metadaten gefunden: N49E004.json
📊 METADATEN-INFORMATION:
Quelle: N49E004.hgt
Höhenbereich: 54m - 502m
Pixel-Auflösung: 45.0m/pixel
✅ Terrain-Färbung abgeschlossen für 'Plane'
```

## 🎮 **Live-Demo Commands**

### **Quick Demo - Alle Biome durchprobieren**
```python
# Führe nacheinander aus:
import time

biomes = ['forest', 'alpine', 'desert', 'tropical']
for biome in biomes:
    print(f"\n🌍 Testing {biome.title()} biome...")
    color_heightmap_faces_advanced(biome=biome, use_slope=True)
    # In Blender: Warten und visuell prüfen
```

### **Performance Test**
```python
# Teste Batch-Processing
batch_process_heightmaps(biome='alpine', use_slope=True)
```

## 💡 **Pro-Tips**

1. **Subdivision Level**: Start mit wenig Subdivision, erhöhe bei Bedarf
2. **Displacement Strength**: 0.01-0.1 je nach gewünschter Höhe
3. **Viewport Performance**: Deaktiviere `use_slope` bei großen Meshes
4. **Lighting**: Füge HDRI oder Sun-Light für dramatische Ergebnisse hinzu
5. **Camera Angle**: Setze Kamera schräg für beste Terrain-Sicht

---

**🎯 Ziel: Realistisches, automatisch eingefärbtes Terrain in unter 5 Minuten!**