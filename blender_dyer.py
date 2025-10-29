import bpy
import bmesh
import mathutils
import json
import os
import math
from pathlib import Path

# ================================================================================
# HGT2PNG BLENDER INTEGRATION SCRIPT v2.0
# Advanced Terrain Coloring with Biome Support and Slope Analysis
# ================================================================================

# Biom-spezifische Farbpaletten
BIOME_PALETTES = {
    'forest': {
        'name': 'Temperater Wald',
        'colors': [
            (0.96, 0.96, 0.86, 1.0),    # Beige (Talsohlen/Geröll)
            (0.4, 0.8, 0.2, 1.0),       # Helles Grasgrün
            (0.2, 0.65, 0.1, 1.0),      # Mittleres Grün
            (0.1, 0.45, 0.05, 1.0),     # Mitteldunkles Waldgrün
            (0.05, 0.3, 0.02, 1.0)      # Dunkelgrün/Tannengrün
        ]
    },
    'alpine': {
        'name': 'Alpin/Hochgebirge',
        'colors': [
            (0.6, 0.5, 0.3, 1.0),       # Braune Täler
            (0.3, 0.6, 0.2, 1.0),       # Alpengras
            (0.4, 0.3, 0.2, 1.0),       # Felsbraun
            (0.7, 0.7, 0.7, 1.0),       # Grauer Fels
            (0.95, 0.95, 1.0, 1.0)      # Schnee/Eis
        ]
    },
    'desert': {
        'name': 'Wüste/Trockengebiet',
        'colors': [
            (0.9, 0.8, 0.6, 1.0),       # Heller Sand
            (0.8, 0.6, 0.4, 1.0),       # Mittlerer Sand
            (0.7, 0.5, 0.3, 1.0),       # Dunkler Sand
            (0.6, 0.4, 0.2, 1.0),       # Felsbraun
            (0.5, 0.3, 0.2, 1.0)        # Dunkler Fels
        ]
    },
    'tropical': {
        'name': 'Tropisch/Regenwald',
        'colors': [
            (0.9, 0.8, 0.6, 1.0),       # Strand/Küste
            (0.5, 0.8, 0.3, 1.0),       # Helles Tropengrün
            (0.2, 0.7, 0.2, 1.0),       # Dichtes Grün
            (0.1, 0.5, 0.1, 1.0),       # Regenwald
            (0.05, 0.3, 0.05, 1.0)      # Dichter Dschungel
        ]
    }
}

def auto_detect_heightmap():
    """
    Automatische Erkennung von Heightmap-Objekten in der Szene
    """
    candidates = []
    for obj in bpy.data.objects:
        if obj.type == 'MESH':
            name_lower = obj.name.lower()
            # Suche nach typischen Heightmap-Namen
            if any(keyword in name_lower for keyword in ['heightmap', 'terrain', 'displacement', 'hgt', 'dem']):
                candidates.append(obj.name)
            # Oder Objekte mit vielen Vertices (typisch für Heightmaps)
            elif len(obj.data.vertices) > 10000:
                candidates.append(obj.name)
    
    if candidates:
        print(f"Gefundene Heightmap-Kandidaten: {candidates}")
        return candidates[0]  # Nimm den ersten Kandidaten
    
    # Fallback: Plane oder größtes Mesh
    if "Plane" in bpy.data.objects:
        return "Plane"
    
    # Nimm das Mesh mit den meisten Vertices
    largest_mesh = None
    max_vertices = 0
    for obj in bpy.data.objects:
        if obj.type == 'MESH' and len(obj.data.vertices) > max_vertices:
            max_vertices = len(obj.data.vertices)
            largest_mesh = obj.name
    
    return largest_mesh if largest_mesh else "Heightmap"

def load_hgt_metadata(obj_name):
    """
    Lädt Metadaten aus hgt2png JSON-Dateien für präzise Höhenbereiche
    """
    # Suche nach JSON-Dateien im gleichen Verzeichnis
    blend_dir = Path(bpy.data.filepath).parent if bpy.data.filepath else Path.cwd()
    
    # Zuerst: Suche nach JSON mit gleichem Namen wie das Objekt
    potential_names = [
        f"{obj_name}.json",
        f"{obj_name.replace('_', '')}.json",
        obj_name.replace('Plane', 'N49E004').replace('Heightmap', 'N49E004') + ".json"
    ]
    
    for name in potential_names:
        json_path = blend_dir / name
        if json_path.exists():
            try:
                with open(json_path, 'r') as f:
                    metadata = json.load(f)
                
                if 'elevation' in metadata and 'scaling' in metadata:
                    print(f"🎯 HGT2PNG Metadaten gefunden: {json_path.name}")
                    return metadata
            except (json.JSONDecodeError, FileNotFoundError):
                continue
    
    # Fallback: Suche alle JSON-Dateien
    for json_file in blend_dir.glob("*.json"):
        try:
            with open(json_file, 'r') as f:
                metadata = json.load(f)
                
            # Prüfe hgt2png Struktur (neue Format)
            if 'elevation' in metadata and 'scaling' in metadata and 'source_file' in metadata:
                print(f"📄 HGT2PNG Metadaten gefunden: {json_file.name}")
                return metadata
                
            # Legacy-Format unterstützen
            elif 'elevation_range' in metadata and 'pixel_pitch_meters' in metadata:
                print(f"📄 Legacy HGT2PNG Metadaten gefunden: {json_file.name}")
                # Konvertiere Legacy-Format
                return {
                    'elevation': {
                        'min_meters': metadata['elevation_range']['min_meters'],
                        'max_meters': metadata['elevation_range']['max_meters']
                    },
                    'scaling': {
                        'pixel_pitch_meters': metadata['pixel_pitch_meters']
                    }
                }
                
        except (json.JSONDecodeError, FileNotFoundError):
            continue
    
    print("❌ Keine hgt2png Metadaten gefunden, verwende automatische Erkennung")
    return None

def print_metadata_info(metadata):
    """
    Zeigt detaillierte Metadaten-Informationen an
    """
    if not metadata:
        return
        
    print("\n📊 METADATEN-INFORMATION:")
    print("-" * 40)
    
    if 'source_file' in metadata:
        print(f"Quelle: {metadata['source_file']}")
    
    if 'elevation' in metadata:
        elev = metadata['elevation']
        print(f"Höhenbereich: {elev['min_meters']}m - {elev['max_meters']}m")
        if 'range_meters' in elev:
            print(f"Höhendifferenz: {elev['range_meters']}m")
    
    if 'scaling' in metadata:
        scale = metadata['scaling']
        print(f"Pixel-Auflösung: {scale['pixel_pitch_meters']}m/pixel")
        if 'scale_factor' in scale:
            print(f"Vergrößerungsfaktor: {scale['scale_factor']}x")
        if 'world_size_meters' in scale:
            world = scale['world_size_meters']
            print(f"Weltgröße: {world['width']/1000:.1f}km × {world['height']/1000:.1f}km")
    
    if 'geographic' in metadata:
        geo = metadata['geographic']
        if 'bounds' in geo:
            bounds = geo['bounds']
            print(f"Koordinaten: {bounds['south']}°-{bounds['north']}°N, {bounds['west']}°-{bounds['east']}°E")
        if 'center' in geo:
            center = geo['center']
            print(f"Zentrum: {center['latitude']:.3f}°N, {center['longitude']:.3f}°E")
    
    print("-" * 40)

def calculate_face_slope(obj, face):
    """
    Berechnet die Neigung (Steigung) einer Face in Grad
    """
    # Face-Normale in Weltkoordinaten
    face_normal = face.normal @ obj.matrix_world.to_3x3()
    
    # Winkel zur Z-Achse (vertikal)
    z_axis = mathutils.Vector((0, 0, 1))
    angle = face_normal.angle(z_axis)
    
    # Konvertiere zu Grad
    slope_degrees = math.degrees(angle)
    return slope_degrees

def get_slope_factor(slope_degrees, terrain_type='mixed'):
    """
    Bestimmt Farbmodifikation basierend auf Neigung
    """
    if terrain_type == 'alpine':
        # In alpinen Gebieten: steile Hänge = Fels
        if slope_degrees > 45:
            return 'rock'  # Felsfarben
        elif slope_degrees > 25:
            return 'scree'  # Geröll
        else:
            return 'vegetation'  # Vegetation
    else:
        # Standard: moderate Anpassung
        if slope_degrees > 35:
            return 'rock'
        elif slope_degrees > 20:
            return 'mixed'
        else:
            return 'vegetation'

def color_heightmap_faces_advanced(obj_name=None, biome='forest', use_slope=True, use_metadata=True, slope_intensity=0.3):
    """
    Erweiterte Färbung von Heightmap-Faces mit Biom-Unterstützung und Neigungsanalyse
    
    Args:
        obj_name (str): Name des Mesh-Objekts (None = automatische Erkennung)
        biome (str): Biom-Typ ('forest', 'alpine', 'desert', 'tropical')
        use_slope (bool): Neigungsbasierte Farbmodifikation verwenden
        use_metadata (bool): hgt2png JSON-Metadaten verwenden
        slope_intensity (float): Intensität der Neigungseffekte (0.0-1.0)
    """
    
    # Automatische Objekterkennung
    if obj_name is None:
        obj_name = auto_detect_heightmap()
        if obj_name is None:
            print("Fehler: Kein geeignetes Heightmap-Objekt gefunden.")
            return
    
    print(f"Verarbeite Objekt: {obj_name}")
    print(f"Gewähltes Biom: {BIOME_PALETTES[biome]['name']}")
    
    # Das Objekt finden
    obj = bpy.data.objects.get(obj_name)
    if not obj or obj.type != 'MESH':
        print(f"Fehler: Objekt '{obj_name}' nicht gefunden oder ist kein Mesh-Objekt.")
        return

    # Sicherstellen, dass wir im Object Mode sind
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Metadaten laden (falls verfügbar)
    metadata = None
    if use_metadata:
        metadata = load_hgt_metadata(obj_name)
        if metadata:
            print_metadata_info(metadata)
    
    # Material erstellen oder vorhandenes Material verwenden
    mat_name = f"Heightmap_Material_{biome.title()}"
    if mat_name not in bpy.data.materials:
        mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True
    else:
        mat = bpy.data.materials[mat_name]

    # Material dem Objekt zuweisen
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    # Shader-Setup für Face-Farben
    setup_material_nodes(mat)

    # Höhenbereich ermitteln
    if metadata and 'elevation' in metadata:
        min_elevation = metadata['elevation']['min_meters']
        max_elevation = metadata['elevation']['max_meters']
        print(f"📏 Metadaten-Höhenbereich: {min_elevation}m - {max_elevation}m")
        
        # Skalierung basierend auf Displacement-Modifier (falls vorhanden)
        displacement_scale = get_displacement_scale(obj)
        if displacement_scale:
            min_z = min_elevation * displacement_scale
            max_z = max_elevation * displacement_scale
            print(f"📐 Displacement-Skalierung: {displacement_scale:.3f}")
        else:
            # Versuche automatische Skalierung basierend auf Pixel-Pitch
            if 'scaling' in metadata and 'pixel_pitch_meters' in metadata['scaling']:
                pixel_pitch = metadata['scaling']['pixel_pitch_meters']
                # Heuristik: 1 Blender-Unit = 30m Pixel-Pitch
                auto_scale = pixel_pitch / 30.0
                min_z = min_elevation * auto_scale
                max_z = max_elevation * auto_scale
                print(f"🔄 Auto-Skalierung: {auto_scale:.3f} (basierend auf {pixel_pitch}m/pixel)")
            else:
                min_z = min_elevation
                max_z = max_elevation
    else:
        # Fallback: Z-Koordinaten der Vertices analysieren
        z_coords = [v.co.z for v in obj.data.vertices]
        min_z = min(z_coords)
        max_z = max(z_coords)
        print("📊 Verwende automatische Z-Koordinaten-Analyse")

    print(f"Verwendeter Z-Bereich: {min_z:.2f} - {max_z:.2f}")

    if max_z == min_z:
        print("Alle Vertices haben die gleiche Z-Koordinate. Keine Höhenunterschiede zum Färben.")
        return

    # Farbpalette für gewähltes Biom
    colors = BIOME_PALETTES[biome]['colors']
    
    # Face-Farben erstellen oder aktualisieren
    if "FaceColor" not in obj.data.color_attributes:
        color_attribute = obj.data.color_attributes.new(name="FaceColor", type='FLOAT_COLOR', domain='FACE')
    else:
        color_attribute = obj.data.color_attributes["FaceColor"]
        
    # Neigungsanalyse vorbereiten (falls aktiviert)
    if use_slope:
        print("Berechne Neigungswinkel für realistische Farbverteilung...")
        obj.data.calc_loop_triangles()
    
    # Faces durchlaufen und Farbe zuweisen
    for face_idx, face in enumerate(obj.data.polygons):
        # Durchschnittliche Höhe der Face berechnen
        face_z_sum = sum(obj.data.vertices[v_idx].co.z for v_idx in face.vertices)
        face_avg_z = face_z_sum / len(face.vertices)
        
        # Normalisierte Höhe (0.0 - 1.0)
        normalized_z = (face_avg_z - min_z) / (max_z - min_z)
        
        # Basis-Farbindex basierend auf Höhe
        color_index = min(int(normalized_z * len(colors)), len(colors) - 1)
        base_color = list(colors[color_index])
        
        # Neigungsbasierte Modifikation
        if use_slope:
            slope_degrees = calculate_face_slope(obj, face)
            slope_factor = get_slope_factor(slope_degrees, biome)
            
            # Farbmodifikation basierend auf Neigung
            if slope_factor == 'rock':
                # Felsige Bereiche: grauer/brauner
                base_color[0] = min(1.0, base_color[0] + 0.2 * slope_intensity)  # Mehr Rot
                base_color[1] = min(1.0, base_color[1] + 0.1 * slope_intensity)  # Weniger Grün
                base_color[2] = max(0.0, base_color[2] - 0.1 * slope_intensity)  # Weniger Blau
            elif slope_factor == 'scree':
                # Geröll: braunlicher
                base_color[0] = min(1.0, base_color[0] + 0.1 * slope_intensity)
                base_color[2] = max(0.0, base_color[2] - 0.05 * slope_intensity)
        
        # Farbe zuweisen
        color_attribute.data[face_idx].color = base_color

    print(f"✅ Terrain-Färbung abgeschlossen für '{obj_name}'")
    print(f"   Biom: {BIOME_PALETTES[biome]['name']}")
    print(f"   Neigungsanalyse: {'✅' if use_slope else '❌'}")
    print(f"   Metadaten: {'✅' if metadata else '❌'}")

def setup_material_nodes(mat):
    """
    Richtet die Material-Knoten für Face-Farben ein
    """
    if not mat.node_tree.nodes.get('Attribute') or mat.node_tree.nodes['Attribute'].attribute_name != "FaceColor":
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links

        # Lösche vorhandene Knoten außer Output
        for node in nodes:
            if node.type != 'OUTPUT_MATERIAL':
                nodes.remove(node)

        # Erstelle neue Knoten
        principled_bsdf = nodes.new(type='ShaderNodeBsdfPrincipled')
        principled_bsdf.location = (0, 0)
        
        attr_node = nodes.new(type='ShaderNodeAttribute')
        attr_node.attribute_name = "FaceColor"
        attr_node.location = (-300, 0)
        
        output_node = nodes.get('Material Output')
        if not output_node:
            output_node = nodes.new(type='ShaderNodeOutputMaterial')
            output_node.location = (300, 0)
        
        # Verbindungen erstellen
        links.new(attr_node.outputs['Color'], principled_bsdf.inputs['Base Color'])
        links.new(principled_bsdf.outputs['BSDF'], output_node.inputs['Surface'])
        
        print("✅ Material-Knoten konfiguriert")

def get_displacement_scale(obj):
    """
    Ermittelt den Displacement-Scale von Modifiern
    """
    for modifier in obj.modifiers:
        if modifier.type == 'DISPLACE':
            return modifier.strength
    return None

def batch_process_heightmaps(biome='forest', use_slope=True):
    """
    Verarbeitet alle erkannten Heightmaps in der Szene
    """
    processed_objects = []
    
    for obj in bpy.data.objects:
        if obj.type == 'MESH':
            name_lower = obj.name.lower()
            if any(keyword in name_lower for keyword in ['heightmap', 'terrain', 'displacement', 'hgt', 'dem']):
                print(f"\n🔄 Verarbeite {obj.name}...")
                color_heightmap_faces_advanced(obj.name, biome=biome, use_slope=use_slope)
                processed_objects.append(obj.name)
    
    if processed_objects:
        print(f"\n✅ Batch-Verarbeitung abgeschlossen: {len(processed_objects)} Objekte")
        for obj_name in processed_objects:
            print(f"   - {obj_name}")
    else:
        print("❌ Keine Heightmap-Objekte gefunden")

# Legacy-Funktion für Rückwärtskompatibilität
def color_heightmap_faces_five_colors_green(obj_name="Heightmap"):
    """
    Legacy-Funktion: Ruft die neue erweiterte Funktion mit Standard-Parametern auf
    """
    print("⚠️  Legacy-Funktion verwendet. Empfehlung: Nutze color_heightmap_faces_advanced()")
    color_heightmap_faces_advanced(obj_name=obj_name, biome='forest', use_slope=False, use_metadata=True)

# ================================================================================
# VERWENDUNGSBEISPIELE
# ================================================================================

def demo_all_biomes(obj_name=None):
    """
    Demonstriert alle verfügbaren Biom-Paletten
    """
    if obj_name is None:
        obj_name = auto_detect_heightmap()
    
    print("\n🎨 BIOM-DEMO: Alle verfügbaren Farbpaletten")
    print("=" * 50)
    
    for biome_key, biome_data in BIOME_PALETTES.items():
        print(f"\n🌍 {biome_data['name']} ({biome_key})")
        # Kommentiere die nächste Zeile aus, um alle Biome zu testen
        # color_heightmap_faces_advanced(obj_name, biome=biome_key, use_slope=True)

def quick_setup_examples():
    """
    Zeigt verschiedene Verwendungsbeispiele
    """
    print("\n📖 HGT2PNG BLENDER INTEGRATION - VERWENDUNGSBEISPIELE")
    print("=" * 60)
    print("""
# 🚀 SCHNELLSTART - Automatische Erkennung:
color_heightmap_faces_advanced()

# 🌲 VERSCHIEDENE BIOME:
color_heightmap_faces_advanced(biome='forest')      # Temperater Wald
color_heightmap_faces_advanced(biome='alpine')      # Hochgebirge
color_heightmap_faces_advanced(biome='desert')      # Wüste
color_heightmap_faces_advanced(biome='tropical')    # Regenwald

# ⛰️  MIT NEIGUNGSANALYSE (realistische Fels-Bereiche):
color_heightmap_faces_advanced(biome='alpine', use_slope=True, slope_intensity=0.5)

# 📊 MIT HGT2PNG METADATEN:
color_heightmap_faces_advanced(use_metadata=True)

# 🔄 BATCH-VERARBEITUNG (alle Heightmaps):
batch_process_heightmaps(biome='forest', use_slope=True)

# 📋 VERFÜGBARE BIOME ANZEIGEN:
demo_all_biomes()
    """)

# ================================================================================
# SCRIPT AUSFÜHRUNG
# ================================================================================

if __name__ == "__main__":
    # Zeige Verwendungsbeispiele
    quick_setup_examples()
    
    # Standard-Ausführung: Automatische Erkennung mit Wald-Biom
    print("\n🔄 Führe Standard-Terrain-Färbung aus...")
    color_heightmap_faces_advanced(biome='forest', use_slope=True, use_metadata=True)
    
    # Legacy-Kompatibilität (auskommentiert)
    # color_heightmap_faces_five_colors_green(obj_name="Plane")