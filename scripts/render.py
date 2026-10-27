"""Rendert alle Heightmaps eines Ordners; das TerrainMaterial wird vollständig hier erzeugt.

Aufruf:
    blender spanien.blend --background --python render.py -- --samples 1000 --resolution 7202
    blender spanien.blend --background --python render.py -- --file N37W006

Die .blend-Datei muss nur World, Kamera und die Plane (subdivided, Modifier "Displace") enthalten.
"""
import argparse
import os
import sys

import bpy

# ----------------------- Standardwerte (per Argument überschreibbar) -----------------------
DEFAULT_INPUT_DIR = "/home/peter/Projekte/Topographie/Spanien/maps"
DEFAULT_OUTPUT_DIR = "/home/peter/Projekte/Topographie/Spanien/render"
DEFAULT_BIOME = "temperate"
DEFAULT_SAMPLES = 1000
DEFAULT_RESOLUTION = 7202
# Rampenposition (0..1) für niedrigste Flächen ohne Vegetation (Maske = 0); steigt dort mit der Höhe bis 1.0
DEFAULT_BARE_GROUND = 0.2
# -------------------------------------------------------------------------------------------

MATERIAL_NAME = "TerrainMaterial"
FALLBACK_IMAGE = "vegetation_fallback_white"


def parse_args():
    # Blender reicht nur Argumente nach "--" an das Skript weiter
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser(description="Rendert Heightmap-Kacheln mit generiertem TerrainMaterial")
    p.add_argument("--input-dir", default=DEFAULT_INPUT_DIR)
    p.add_argument("--file", default=None,
                   help="Einzelne Heightmap (Name mit/ohne .png, relativ zu --input-dir); ohne Angabe werden alle gerendert")
    p.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    p.add_argument("--biome", default=DEFAULT_BIOME, help="Biom der Vegetationsmasken")
    p.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    p.add_argument("--resolution", type=int, default=DEFAULT_RESOLUTION, help="Kantenlänge in Pixeln (quadratisch)")
    p.add_argument("--bare-ground", type=float, default=DEFAULT_BARE_GROUND,
                   help="Rampenposition (0..1) für Flächen ohne Vegetation")
    return p.parse_args(argv)


def link(links, output_socket, input_socket):
    links.new(output_socket, input_socket)


def create_terrain_material(bare_ground):
    """Erzeugt das TerrainMaterial; gibt (Material, Heightmap-Node, Vegetations-Node) zurück."""
    old = bpy.data.materials.get(MATERIAL_NAME)
    if old:
        bpy.data.materials.remove(old)

    material = bpy.data.materials.new(name=MATERIAL_NAME)
    if bpy.app.version < (5, 0, 0):
        material.use_nodes = True
    material.displacement_method = 'BUMP'

    tree = material.node_tree
    nodes = tree.nodes
    links = tree.links
    nodes.clear()

    # --- Ausgabe und Shader ---
    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (1166, 245)

    principled = nodes.new("ShaderNodeBsdfPrincipled")
    principled.location = (547, -80)
    principled.inputs["Roughness"].default_value = 0.8

    sea = nodes.new("ShaderNodeBsdfDiffuse")
    sea.location = (549, 98)
    sea.inputs["Color"].default_value = (0.0, 0.002124662743881345, 0.07618541270494461, 1.0)

    mix_shader = nodes.new("ShaderNodeMixShader")
    mix_shader.label = "TerrainOcean"
    mix_shader.location = (943, 227)

    # --- Heightmap und Texturkoordinaten ---
    tex_coord = nodes.new("ShaderNodeTexCoord")
    tex_coord.location = (-754, -49)

    heightmap = nodes.new("ShaderNodeTexImage")
    heightmap.label = "Heightmap_color"
    heightmap.location = (-429, -10)
    heightmap.extension = 'REPEAT'
    heightmap.interpolation = 'Linear'
    heightmap.projection = 'FLAT'

    # --- Vegetationsmaske ---
    vegetation = nodes.new("ShaderNodeTexImage")
    vegetation.label = "VegetationMask"
    vegetation.location = (-429, -330)
    vegetation.interpolation = heightmap.interpolation
    vegetation.extension = heightmap.extension

    # Kahler Boden: Höhe auf [bare_ground, 1] gestreckt, damit Hochlagen ohne Vegetation braun statt konstant ocker werden
    bare = nodes.new("ShaderNodeMath")
    bare.label = "BareGroundRamp"
    bare.location = (-280, -250)
    bare.operation = 'MULTIPLY_ADD'
    bare.use_clamp = True
    bare.inputs[1].default_value = 1.0 - bare_ground
    bare.inputs[2].default_value = bare_ground

    mix_color = nodes.new("ShaderNodeMix")
    mix_color.label = "VegetationMix"
    mix_color.location = (-130, -130)
    mix_color.data_type = 'RGBA'
    mix_color.blend_type = 'MIX'
    mix_color.factor_mode = 'UNIFORM'
    mix_color.clamp_factor = True

    # --- Farbverlauf ---
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.location = (76, -46)
    cr = ramp.color_ramp
    cr.color_mode = 'RGB'
    cr.interpolation = 'LINEAR'
    cr.elements.remove(cr.elements[0])
    stops = (
        (0.2227272242307663, (0.17818601429462433, 0.3309002220630646, 0.10261856019496918, 1.0)),
        (0.3227272033691406, (0.7076643705368042, 0.6198279857635498, 0.2310725450515747, 1.0)),
        (0.5409092307090759, (0.7427607774734497, 0.518953800201416, 0.1612369865179062, 1.0)),
        (1.0, (0.29288288950920105, 0.10079766064882278, 0.0, 1.0)),
    )
    cr.elements[0].position, cr.elements[0].color = stops[0][0], stops[0][1]
    for pos, color in stops[1:]:
        el = cr.elements.new(pos)
        el.color = color

    # --- Verbindungen ---
    link(links, tex_coord.outputs["Generated"], heightmap.inputs["Vector"])
    link(links, tex_coord.outputs["Generated"], vegetation.inputs["Vector"])

    link(links, heightmap.outputs["Color"], bare.inputs[0])
    # Mix-Sockets über Index (A/B existieren je Datentyp mehrfach): 0 Factor, 6 A (Color), 7 B (Color), Ausgang 2 Result (Color)
    link(links, vegetation.outputs["Color"], mix_color.inputs[0])
    link(links, bare.outputs[0], mix_color.inputs[6])
    link(links, heightmap.outputs["Color"], mix_color.inputs[7])
    link(links, mix_color.outputs[2], ramp.inputs["Fac"])

    link(links, ramp.outputs["Color"], principled.inputs["Base Color"])
    link(links, sea.outputs["BSDF"], mix_shader.inputs[1])
    link(links, principled.outputs["BSDF"], mix_shader.inputs[2])
    link(links, heightmap.outputs["Alpha"], mix_shader.inputs[0])
    link(links, mix_shader.outputs["Shader"], output.inputs["Surface"])

    return material, heightmap, vegetation


def load_vegetation_mask(path):
    """Lädt die Maske als Non-Color; ohne Datei ein weißes Bild (= reine Höhenfärbung)"""
    if os.path.isfile(path):
        img = bpy.data.images.load(path, check_existing=True)
        img.colorspace_settings.name = 'Non-Color'
        return img, True

    print(f"WARNUNG: Keine Vegetationsmaske gefunden: {path} (Fallback: weiß)")
    img = bpy.data.images.get(FALLBACK_IMAGE)
    if img is None:
        img = bpy.data.images.new(FALLBACK_IMAGE, 4, 4, alpha=False)
        img.generated_color = (1.0, 1.0, 1.0, 1.0)
        img.colorspace_settings.name = 'Non-Color'
    return img, False


def safe_remove(collection, item):
    if item and item in collection.values():
        try:
            collection.remove(item, do_unlink=True)
        except Exception:
            pass


def process_all_tiles(args):
    if args.file:
        name = os.path.basename(args.file)
        files = [name if name.endswith(".png") else f"{name}.png"]
        if not os.path.isfile(os.path.join(args.input_dir, files[0])):
            print(f"ERROR: Heightmap nicht gefunden: {os.path.join(args.input_dir, files[0])}")
            return
    else:
        files = sorted(f for f in os.listdir(args.input_dir)
                       if f.endswith(".png") and "_vegetation_" not in f and not f.endswith("_render.png"))
    if not files:
        print("ERROR: Keine *.png Heightmap-Dateien gefunden!")
        return

    plane = bpy.data.objects.get("Plane")
    if not plane:
        print("ERROR: Objekt 'Plane' nicht gefunden!")
        return

    displace = plane.modifiers.get("Displace")
    if not displace:
        print("ERROR: Modifier 'Displace' nicht gefunden!")
        return

    material, node_heightmap, node_vegetation = create_terrain_material(args.bare_ground)
    plane.data.materials.clear()
    plane.data.materials.append(material)

    scene = bpy.context.scene
    scene.render.resolution_x = args.resolution
    scene.render.resolution_y = args.resolution
    scene.render.resolution_percentage = 100
    scene.cycles.samples = args.samples

    os.makedirs(args.output_dir, exist_ok=True)

    total = len(files)
    print(f"\n=== Batch Start: {total} Tiles ===\n")

    for idx, fname in enumerate(files, start=1):
        base_name = fname[:-4]
        heightmap_path = os.path.join(args.input_dir, fname)
        vegetation_path = os.path.join(args.input_dir, f"{base_name}_vegetation_{args.biome}.png")
        output_path = os.path.join(args.output_dir, f"{base_name}_render.png")

        print(f"\n[{idx}/{total}] Tile: {base_name}")

        try:
            heightmap_img = bpy.data.images.load(heightmap_path, check_existing=True)
            heightmap_img.colorspace_settings.name = 'Non-Color'
        except Exception as e:
            print(f"ERROR: Konnte Heightmap nicht laden: {e}")
            continue

        try:
            vegetation_img, vegetation_loaded = load_vegetation_mask(vegetation_path)
        except Exception as e:
            print(f"ERROR: Konnte Vegetationsmaske nicht laden: {e}")
            continue

        texture_name = f"disp_{base_name}"
        safe_remove(bpy.data.textures, bpy.data.textures.get(texture_name))
        disp_texture = bpy.data.textures.new(texture_name, type='IMAGE')
        disp_texture.image = heightmap_img
        displace.texture = disp_texture

        node_heightmap.image = heightmap_img
        node_vegetation.image = vegetation_img

        try:
            scene.render.filepath = output_path
            print(f"Render: {output_path}")
            bpy.ops.render.render(write_still=True)
        except Exception as e:
            print(f"RENDER ERROR: {e}")
            continue
        finally:
            displace.texture = None
            bpy.context.view_layer.update()
            safe_remove(bpy.data.textures, disp_texture)
            safe_remove(bpy.data.images, heightmap_img)
            if vegetation_loaded:
                safe_remove(bpy.data.images, vegetation_img)

    print("\n=== Batch fertig ===")


if __name__ == "__main__":
    process_all_tiles(parse_args())
