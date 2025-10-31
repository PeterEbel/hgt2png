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

    # ===============================================================================
    # SMOOTH SHADING für natürliches Terrain (Anti-Minecraft-Effekt)
    # ===============================================================================
    
    # Zu Edit Mode wechseln für Mesh-Operationen
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    
    # Alle Faces auswählen
    bpy.ops.mesh.select_all(action='SELECT')
    
    # Smooth Shading anwenden
    bpy.ops.mesh.faces_shade_smooth()
    
    # Optional: Auto Smooth für noch bessere Ergebnisse
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.use_auto_smooth = True
    obj.data.auto_smooth_angle = math.radians(45)  # 45 Grad Auto-Smooth
    
    print(f"✅ Terrain-Färbung abgeschlossen für '{obj_name}'")
    print(f"   Biom: {BIOME_PALETTES[biome]['name']}")
    print(f"   Neigungsanalyse: {'✅' if use_slope else '❌'}")
    print(f"   Metadaten: {'✅' if metadata else '❌'}")
    print(f"   🎨 Smooth Shading: ✅ (Auto-Smooth: 45°)")

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

def setup_procedural_pbr_material(mat, biome='forest', detail_scale=10.0, use_advanced_mixing=True):
    """
    Erstellt fortgeschrittenes PBR-Material mit prozeduralen Texturen
    
    Args:
        mat: Blender Material-Objekt
        biome: Biom-Typ für spezifische Texturen
        detail_scale: Skalierung der prozeduralen Details (1.0-50.0)
        use_advanced_mixing: Erweiterte Höhen/Neigungs-basierte Mischung
    """
    
    print(f"🎨 Erstelle PBR-Material für Biom: {biome}")
    
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    
    # Lösche vorhandene Knoten außer Output
    for node in nodes:
        if node.type != 'OUTPUT_MATERIAL':
            nodes.remove(node)
    
    # ===============================================================================
    # HAUPTKNOTEN ERSTELLEN
    # ===============================================================================
    
    # Material Output
    output_node = nodes.new(type='ShaderNodeOutputMaterial')
    output_node.location = (800, 0)
    
    # Principled BSDF
    principled = nodes.new(type='ShaderNodeBsdfPrincipled')
    principled.location = (500, 0)
    
    # Vertex Color Attribute (Biom-Färbung)
    vertex_color = nodes.new(type='ShaderNodeAttribute')
    vertex_color.attribute_name = "FaceColor"
    vertex_color.location = (-800, 300)
    
    # ===============================================================================
    # PROZEDURALE TEXTUREN FÜR VERSCHIEDENE BIOME
    # ===============================================================================
    
    if biome == 'alpine':
        setup_alpine_procedural_nodes(nodes, links, detail_scale)
    elif biome == 'forest':
        setup_forest_procedural_nodes(nodes, links, detail_scale)
    elif biome == 'desert':
        setup_desert_procedural_nodes(nodes, links, detail_scale)
    elif biome == 'tropical':
        setup_tropical_procedural_nodes(nodes, links, detail_scale)
    
    # ===============================================================================
    # ERWEITERTE MISCHUNG (falls aktiviert)
    # ===============================================================================
    
    if use_advanced_mixing:
        # Geometrie-Informationen für erweiterte Mischung
        geometry = nodes.new(type='ShaderNodeNewGeometry')
        geometry.location = (-800, -200)
        
        # Z-Position für Höhen-basierte Effekte
        separate_xyz = nodes.new(type='ShaderNodeSeparateXYZ')
        separate_xyz.location = (-600, -200)
        links.new(geometry.outputs['Position'], separate_xyz.inputs['Vector'])
        
        # Normal für Neigungs-basierte Effekte  
        dot_product = nodes.new(type='ShaderNodeVectorMath')
        dot_product.operation = 'DOT_PRODUCT'
        dot_product.location = (-600, -400)
        dot_product.inputs[1].default_value = (0, 0, 1)  # Up-Vector
        links.new(geometry.outputs['Normal'], dot_product.inputs[0])
        
        # Color Ramp für Slope-to-Factor Konvertierung
        slope_ramp = nodes.new(type='ShaderNodeValToRGB')
        slope_ramp.location = (-400, -400)
        slope_ramp.color_ramp.elements[0].position = 0.3  # Flache Bereiche
        slope_ramp.color_ramp.elements[1].position = 0.8  # Steile Bereiche
        links.new(dot_product.outputs['Value'], slope_ramp.inputs['Fac'])
        
        # Height Color Ramp
        height_ramp = nodes.new(type='ShaderNodeValToRGB')
        height_ramp.location = (-400, -200)
        links.new(separate_xyz.outputs['Z'], height_ramp.inputs['Fac'])
        
        print("✅ Erweiterte Höhen/Neigungs-Mischung aktiviert")
    
    # Basis-Verbindung: Vertex Colors zu Base Color
    final_mix = vertex_color
    
    # Falls erweiterte Mischung: Kombiniere mit prozeduralen Texturen
    if use_advanced_mixing and biome in ['alpine', 'forest']:
        # Mix Node für Kombination von Vertex Colors und Prozedural
        mix_node = nodes.new(type='ShaderNodeMixRGB')
        mix_node.location = (200, 200)
        mix_node.blend_type = 'OVERLAY'
        mix_node.inputs['Fac'].default_value = 0.3  # 30% Prozedural-Einfluss
        
        links.new(vertex_color.outputs['Color'], mix_node.inputs['Color1'])
        # Color2 wird von biom-spezifischen Funktionen verbunden
        final_mix = mix_node
    
    # Finale Verbindungen
    links.new(final_mix.outputs['Color'], principled.inputs['Base Color'])
    links.new(principled.outputs['BSDF'], output_node.inputs['Surface'])
    
    print(f"✅ PBR-Material '{mat.name}' erstellt ({biome})")
    return principled, vertex_color

def setup_alpine_procedural_nodes(nodes, links, detail_scale):
    """
    Erstellt alpine/Gebirgs-spezifische prozedurale Texturen
    """
    
    # Fels-Basis-Textur (Voronoi für Felsstrukturen)
    rock_voronoi = nodes.new(type='ShaderNodeTexVoronoi')
    rock_voronoi.location = (-600, 100)
    rock_voronoi.inputs['Scale'].default_value = detail_scale * 3.0
    rock_voronoi.feature = 'F1'
    
    # Feine Fels-Details (Noise)
    rock_noise = nodes.new(type='ShaderNodeTexNoise')
    rock_noise.location = (-600, -100)
    rock_noise.inputs['Scale'].default_value = detail_scale * 8.0
    rock_noise.inputs['Detail'].default_value = 12.0
    rock_noise.inputs['Roughness'].default_value = 0.6
    
    # Schnee-Textur (für hohe Bereiche)
    snow_noise = nodes.new(type='ShaderNodeTexNoise')
    snow_noise.location = (-600, -300)
    snow_noise.inputs['Scale'].default_value = detail_scale * 15.0
    snow_noise.inputs['Detail'].default_value = 2.0
    snow_noise.inputs['Roughness'].default_value = 0.3
    
    # Color Ramps für Textur-Kontrast
    rock_ramp = nodes.new(type='ShaderNodeValToRGB')
    rock_ramp.location = (-400, 100)
    rock_ramp.color_ramp.elements[0].color = (0.3, 0.25, 0.2, 1.0)  # Dunkler Fels
    rock_ramp.color_ramp.elements[1].color = (0.6, 0.55, 0.5, 1.0)  # Heller Fels
    
    links.new(rock_voronoi.outputs['Distance'], rock_ramp.inputs['Fac'])
    
    print("🏔️ Alpine prozedurale Texturen erstellt")

def setup_forest_procedural_nodes(nodes, links, detail_scale):
    """
    Erstellt Wald-spezifische prozedurale Texturen
    """
    
    # Erd-Basis-Textur
    earth_noise = nodes.new(type='ShaderNodeTexNoise')
    earth_noise.location = (-600, 100)
    earth_noise.inputs['Scale'].default_value = detail_scale * 2.0
    earth_noise.inputs['Detail'].default_value = 8.0
    earth_noise.inputs['Roughness'].default_value = 0.5
    
    # Moos/Vegetation Details
    moss_noise = nodes.new(type='ShaderNodeTexNoise')
    moss_noise.location = (-600, -100)
    moss_noise.inputs['Scale'].default_value = detail_scale * 12.0
    moss_noise.inputs['Detail'].default_value = 6.0
    moss_noise.inputs['Roughness'].default_value = 0.7
    
    # Laub-/Humus-Textur
    organic_voronoi = nodes.new(type='ShaderNodeTexVoronoi')
    organic_voronoi.location = (-600, -300)
    organic_voronoi.inputs['Scale'].default_value = detail_scale * 5.0
    organic_voronoi.feature = 'SMOOTH_F1'
    
    print("🌲 Forest prozedurale Texturen erstellt")

def setup_desert_procedural_nodes(nodes, links, detail_scale):
    """
    Erstellt Wüsten-spezifische prozedurale Texturen
    """
    
    # Sand-Dünen (Wave Texture)
    sand_wave = nodes.new(type='ShaderNodeTexWave')
    sand_wave.location = (-600, 100)
    sand_wave.inputs['Scale'].default_value = detail_scale * 0.5
    sand_wave.inputs['Distortion'].default_value = 2.0
    sand_wave.wave_type = 'RINGS'
    
    # Feine Sand-Körnung
    sand_noise = nodes.new(type='ShaderNodeTexNoise')
    sand_noise.location = (-600, -100)
    sand_noise.inputs['Scale'].default_value = detail_scale * 20.0
    sand_noise.inputs['Detail'].default_value = 4.0
    sand_noise.inputs['Roughness'].default_value = 0.4
    
    # Fels-Aufschlüsse
    rock_voronoi = nodes.new(type='ShaderNodeTexVoronoi')
    rock_voronoi.location = (-600, -300)
    rock_voronoi.inputs['Scale'].default_value = detail_scale * 1.5
    rock_voronoi.feature = 'F2'
    
    print("🏜️ Desert prozedurale Texturen erstellt")

def setup_tropical_procedural_nodes(nodes, links, detail_scale):
    """
    Erstellt tropische/Regenwald-spezifische prozedurale Texturen
    """
    
    # Dichte Vegetation
    vegetation_noise = nodes.new(type='ShaderNodeTexNoise')
    vegetation_noise.location = (-600, 100)
    vegetation_noise.inputs['Scale'].default_value = detail_scale * 4.0
    vegetation_noise.inputs['Detail'].default_value = 10.0
    vegetation_noise.inputs['Roughness'].default_value = 0.8
    
    # Feuchtigkeit/Moos-Strukturen
    moss_voronoi = nodes.new(type='ShaderNodeTexVoronoi')
    moss_voronoi.location = (-600, -100)
    moss_voronoi.inputs['Scale'].default_value = detail_scale * 8.0
    moss_voronoi.feature = 'SMOOTH_F1'
    
    # Organische Strukturen
    organic_musgrave = nodes.new(type='ShaderNodeTexMusgrave')
    organic_musgrave.location = (-600, -300)
    organic_musgrave.inputs['Scale'].default_value = detail_scale * 6.0
    organic_musgrave.inputs['Detail'].default_value = 12.0
    organic_musgrave.musgrave_type = 'RIDGED_MULTIFRACTAL'
    
    print("🌺 Tropical prozedurale Texturen erstellt")

def toggle_shading_mode(obj_name=None, mode='smooth', auto_smooth_angle=45):
    """
    Wechselt zwischen Smooth und Flat Shading für Heightmap-Vergleiche
    
    Args:
        obj_name (str): Name des Mesh-Objekts (None = automatische Erkennung)
        mode (str): 'smooth' oder 'flat'
        auto_smooth_angle (float): Auto-Smooth Winkel in Grad (nur bei smooth)
    """
    
    # Automatische Objekterkennung
    if obj_name is None:
        obj_name = auto_detect_heightmap()
        if obj_name is None:
            print("❌ Fehler: Kein geeignetes Heightmap-Objekt gefunden.")
            return
    
    # Das Objekt finden
    obj = bpy.data.objects.get(obj_name)
    if not obj or obj.type != 'MESH':
        print(f"❌ Fehler: Objekt '{obj_name}' nicht gefunden oder ist kein Mesh-Objekt.")
        return
    
    print(f"🔄 Wechsle Shading Mode auf: {mode.upper()}")
    
    # Sicherstellen, dass wir im Object Mode sind
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    
    # Zu Edit Mode wechseln
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    
    # Alle Faces auswählen
    bpy.ops.mesh.select_all(action='SELECT')
    
    if mode.lower() == 'smooth':
        # Smooth Shading anwenden
        bpy.ops.mesh.faces_shade_smooth()
        
        # Zurück zu Object Mode für Auto-Smooth
        bpy.ops.object.mode_set(mode='OBJECT')
        obj.data.use_auto_smooth = True
        obj.data.auto_smooth_angle = math.radians(auto_smooth_angle)
        
        print(f"✅ SMOOTH Shading aktiviert (Auto-Smooth: {auto_smooth_angle}°)")
        print(f"   💡 Für natürliches Terrain-Rendering")
        
    elif mode.lower() == 'flat':
        # Flat Shading anwenden
        bpy.ops.mesh.faces_shade_flat()
        
        # Zurück zu Object Mode
        bpy.ops.object.mode_set(mode='OBJECT')
        obj.data.use_auto_smooth = False
        
        print(f"✅ FLAT Shading aktiviert")
        print(f"   💡 Für technische/geometrische Darstellung")
        
    else:
        print(f"❌ Unbekannter Modus: {mode}. Verwende 'smooth' oder 'flat'")

def apply_smooth_shading(obj_name=None, auto_smooth_angle=45):
    """
    Wendet Smooth Shading auf Heightmap-Mesh an (Anti-Minecraft-Effekt)
    
    Args:
        obj_name (str): Name des Mesh-Objekts (None = automatische Erkennung)
        auto_smooth_angle (float): Auto-Smooth Winkel in Grad (Standard: 45°)
    """
    
    # Automatische Objekterkennung
    if obj_name is None:
        obj_name = auto_detect_heightmap()
        if obj_name is None:
            print("❌ Fehler: Kein geeignetes Heightmap-Objekt gefunden.")
            return
    
    # Das Objekt finden
    obj = bpy.data.objects.get(obj_name)
    if not obj or obj.type != 'MESH':
        print(f"❌ Fehler: Objekt '{obj_name}' nicht gefunden oder ist kein Mesh-Objekt.")
        return
    
    print(f"🎨 Wende Smooth Shading an auf: {obj_name}")
    
    # Sicherstellen, dass wir im Object Mode sind
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    
    # Zu Edit Mode wechseln
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    
    # Alle Faces auswählen
    bpy.ops.mesh.select_all(action='SELECT')
    
    # Smooth Shading anwenden
    bpy.ops.mesh.faces_shade_smooth()
    
    # Zurück zu Object Mode
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # Auto Smooth aktivieren
    obj.data.use_auto_smooth = True
    obj.data.auto_smooth_angle = math.radians(auto_smooth_angle)
    
    print(f"✅ Smooth Shading angewendet:")
    print(f"   🎨 Face Shading: Smooth")
    print(f"   🔧 Auto-Smooth: {auto_smooth_angle}°")
    print(f"   💡 Tipp: Versuche verschiedene Winkel (30°-60°) für optimale Ergebnisse")

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

def create_pbr_terrain_material(obj_name=None, biome='forest', detail_scale=10.0, 
                               use_vertex_colors=True, use_advanced_mixing=True):
    """
    Erstellt photorealistisches PBR-Material für Terrain mit prozeduralen Texturen
    
    Args:
        obj_name (str): Name des Mesh-Objekts (None = automatische Erkennung)
        biome (str): Biom-Typ für spezifische Texturen
        detail_scale (float): Skalierung der prozeduralen Details (1.0-50.0)
        use_vertex_colors (bool): Bestehende Vertex Colors beibehalten
        use_advanced_mixing (bool): Erweiterte Höhen/Neigungs-basierte Mischung
    """
    
    # Automatische Objekterkennung
    if obj_name is None:
        obj_name = auto_detect_heightmap()
        if obj_name is None:
            print("❌ Fehler: Kein geeignetes Heightmap-Objekt gefunden.")
            return
    
    # Das Objekt finden
    obj = bpy.data.objects.get(obj_name)
    if not obj or obj.type != 'MESH':
        print(f"❌ Fehler: Objekt '{obj_name}' nicht gefunden oder ist kein Mesh-Objekt.")
        return
    
    print(f"🎨 Erstelle PBR-Material für '{obj_name}' (Biom: {biome})")
    
    # Falls noch keine Vertex Colors vorhanden: Standard-Färbung anwenden
    if use_vertex_colors and not obj.data.color_attributes:
        print("⚠️ Keine Vertex Colors gefunden - führe Standard-Färbung aus...")
        color_heightmap_faces_advanced(obj_name, biome=biome, use_slope=True)
    
    # PBR-Material erstellen
    mat_name = f"PBR_Terrain_{biome.title()}"
    if mat_name in bpy.data.materials:
        mat = bpy.data.materials[mat_name]
        print(f"🔄 Verwende existierendes Material: {mat_name}")
    else:
        mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True
        print(f"✨ Neues PBR-Material erstellt: {mat_name}")
    
    # Material dem Objekt zuweisen
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)
    
    # PBR-Shader-Setup
    principled, vertex_color = setup_procedural_pbr_material(
        mat, biome=biome, 
        detail_scale=detail_scale, 
        use_advanced_mixing=use_advanced_mixing
    )
    
    # Biom-spezifische PBR-Eigenschaften setzen
    setup_biome_pbr_properties(principled, biome)
    
    # Smooth Shading anwenden
    apply_smooth_shading(obj_name, auto_smooth_angle=45)
    
    print(f"✅ PBR-Terrain-Material abgeschlossen!")
    print(f"   🎨 Biom: {BIOME_PALETTES[biome]['name']}")
    print(f"   🔧 Detail-Scale: {detail_scale}")
    print(f"   📊 Erweiterte Mischung: {'✅' if use_advanced_mixing else '❌'}")
    print(f"   🎯 Empfehlung: Teste verschiedene Detail-Scales (5.0-25.0)")

def setup_biome_pbr_properties(principled, biome):
    """
    Setzt biom-spezifische PBR-Eigenschaften (Roughness, Metallic, etc.)
    """
    
    if biome == 'alpine':
        # Gebirge: Viel Fels = hohe Roughness, kein Metallic
        principled.inputs['Roughness'].default_value = 0.8
        principled.inputs['Metallic'].default_value = 0.0
        principled.inputs['Specular'].default_value = 0.3
        print("🏔️ Alpine PBR: Hohe Roughness für Fels-Oberflächen")
        
    elif biome == 'forest':
        # Wald: Organische Materialien = mittlere Roughness
        principled.inputs['Roughness'].default_value = 0.6
        principled.inputs['Metallic'].default_value = 0.0
        principled.inputs['Specular'].default_value = 0.4
        principled.inputs['Subsurface'].default_value = 0.1  # Leichte Subsurface für organische Materialien
        print("🌲 Forest PBR: Organische Oberflächen mit Subsurface")
        
    elif biome == 'desert':
        # Wüste: Sand = niedrige Roughness, hohe Specular
        principled.inputs['Roughness'].default_value = 0.4
        principled.inputs['Metallic'].default_value = 0.0
        principled.inputs['Specular'].default_value = 0.6
        print("🏜️ Desert PBR: Glatter Sand mit hoher Reflektion")
        
    elif biome == 'tropical':
        # Tropenwald: Feuchte Oberflächen = niedrige Roughness
        principled.inputs['Roughness'].default_value = 0.3
        principled.inputs['Metallic'].default_value = 0.0
        principled.inputs['Specular'].default_value = 0.7
        principled.inputs['Subsurface'].default_value = 0.15  # Mehr Subsurface für feuchte Vegetation
        print("🌺 Tropical PBR: Feuchte Oberflächen mit starker Subsurface")

def batch_create_pbr_materials(biome='forest', detail_scale=10.0):
    """
    Erstellt PBR-Materialien für alle erkannten Heightmaps in der Szene
    """
    processed_objects = []
    
    for obj in bpy.data.objects:
        if obj.type == 'MESH':
            name_lower = obj.name.lower()
            if any(keyword in name_lower for keyword in ['heightmap', 'terrain', 'displacement', 'hgt', 'dem']):
                print(f"\n🎨 Erstelle PBR-Material für {obj.name}...")
                create_pbr_terrain_material(obj.name, biome=biome, detail_scale=detail_scale)
                processed_objects.append(obj.name)
    
    if processed_objects:
        print(f"\n✅ PBR-Batch-Verarbeitung abgeschlossen: {len(processed_objects)} Objekte")
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

# ⛰️ MIT NEIGUNGSANALYSE:
color_heightmap_faces_advanced(biome='alpine', use_slope=True, slope_intensity=0.7)

# 🎨 SHADING-OPTIONEN (Anti-Minecraft-Effekt):
apply_smooth_shading()                               # Smooth Shading anwenden
toggle_shading_mode(mode='smooth', auto_smooth_angle=30)  # 30° Auto-Smooth
toggle_shading_mode(mode='flat')                     # Zurück zu Flat Shading

# � PBR-MATERIALIEN (Photorealistisch):
create_pbr_terrain_material()                        # Standard PBR mit Forest-Biom
create_pbr_terrain_material(biome='alpine', detail_scale=15.0)  # Alpine mit hohen Details
create_pbr_terrain_material(biome='desert', detail_scale=5.0)   # Desert mit groben Details

# �🔄 BATCH-PROCESSING:
batch_process_heightmaps(biome='forest', use_slope=True)
batch_create_pbr_materials(biome='alpine', detail_scale=12.0)   # PBR für alle Objekte

# 📊 ERWEITERTE OPTIONEN:
color_heightmap_faces_advanced(
    obj_name="Plane",           # Spezifisches Objekt
    biome='alpine',             # Biom-Typ
    use_slope=True,             # Neigungsanalyse
    use_metadata=True,          # JSON-Metadaten
    slope_intensity=0.5         # Neigungseffekt-Stärke
)

# 🎯 PBR-MATERIAL MIT ALLEN OPTIONEN:
create_pbr_terrain_material(
    obj_name="Plane",           # Objekt-Name
    biome='tropical',           # Biom für prozedurale Texturen
    detail_scale=20.0,          # Hohe Detail-Dichte
    use_vertex_colors=True,     # Bestehende Färbung beibehalten
    use_advanced_mixing=True    # Höhen/Neigungs-basierte Mischung
)

# 💡 DETAIL-SCALE TIPPS:
# - 3.0-8.0: Grobe, große Strukturen (Landschafts-Übersicht)
# - 10.0-15.0: Ausgewogene Details (Empfohlen)
# - 20.0-30.0: Sehr feine Details (Nahaufnahmen)
# - 40.0+: Extreme Details (Spezialanwendungen)

# 🎨 BIOM-EMPFEHLUNGEN:
# - Alpine: detail_scale=15.0 (scharfe Felsstrukturen)
# - Forest: detail_scale=12.0 (organische Texturen)
# - Desert: detail_scale=8.0 (große Dünenstrukturen)
# - Tropical: detail_scale=18.0 (dichte Vegetationsdetails)
""")

# ================================================================================
# HAUPTAUSFÜHRUNG  
# ================================================================================

if __name__ == "__main__":
    print("🎯 HGT2PNG BLENDER INTEGRATION v2.1 - PBR EDITION!")
    print("=" * 60)
    print("VERFÜGBARE FUNKTIONEN:")
    print("📊 STANDARD:")
    print("- color_heightmap_faces_advanced()          # Basis-Färbung + Smooth Shading")
    print("- apply_smooth_shading()                     # Nur Smooth Shading anwenden")
    print("")
    print("🚀 PBR-MATERIALIEN (NEU!):")
    print("- create_pbr_terrain_material()              # Photorealistisches PBR-Material")
    print("- batch_create_pbr_materials()               # PBR für alle Terrain-Objekte")
    print("")
    print("🔧 UTILITIES:")
    print("- toggle_shading_mode('smooth')              # Zwischen Smooth/Flat umschalten")
    print("- quick_setup_examples()                     # Alle Beispiele anzeigen")
    print("")
    print("💡 NEU: Prozedurale PBR-Texturen für fotorealistisches Terrain!")
    print("   🏔️ Alpine: Realistische Fels-/Schnee-Strukturen")
    print("   🌲 Forest: Organische Erd-/Vegetations-Texturen")  
    print("   🏜️ Desert: Sand-Dünen und Gesteins-Aufschlüsse")
    print("   🌺 Tropical: Feuchte Vegetation und Moos-Strukturen")
    print("")
    print("🎮 SCHNELLTEST:")
    print("Aktiviere eine der folgenden Zeilen zum Testen:")
    print("")
    
    # ===============================================================================
    # SCHNELLTESTS - AKTIVIERE EINE ZEILE ZUM TESTEN
    # ===============================================================================
    
    # 🚀 OPTION 1: Einfacher PBR-Test (empfohlen für ersten Test)
    # create_pbr_terrain_material(biome='alpine', detail_scale=12.0)
    
    # 🌲 OPTION 2: Verschiedene Biome testen
    # create_pbr_terrain_material(biome='forest', detail_scale=10.0)
    # create_pbr_terrain_material(biome='desert', detail_scale=8.0)
    # create_pbr_terrain_material(biome='tropical', detail_scale=15.0)
    
    # 📊 OPTION 3: Erst Standard, dann PBR (zum Vergleichen)
    # color_heightmap_faces_advanced(biome='alpine', use_slope=True)
    # input("Drücke Enter nach Betrachtung des Standard-Materials...")
    # create_pbr_terrain_material(biome='alpine', detail_scale=15.0)
    
    # 🔄 OPTION 4: Batch-Processing für mehrere Objekte
    # batch_create_pbr_materials(biome='alpine', detail_scale=12.0)
    
    print("💡 Tipp: Entkommentiere eine der Optionen oben zum Testen!")
    print("🎨 Empfehlung: Starte mit Option 1 für den ersten Test")

