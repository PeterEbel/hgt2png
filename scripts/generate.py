import bpy
import os

# ----------------------- Benutzereinstellungen -----------------------
INPUT_DIR = "/home/peter/Projekte/Topographie/Spanien/maps"
OUTPUT_DIR = "/home/peter/Projekte/Topographie/Spanien/render"

# Biom der Vegetationsmasken (Dateiname: <Kachel>_vegetation_<Biom>.png)
VEGETATION_BIOME = "temperate"

# Rampenposition (0..1) für niedrigste Flächen ohne Vegetation (Maske = 0); steigt dort mit der Höhe bis 1.0
BARE_GROUND_VALUE = 0.2
# ---------------------------------------------------------------------

NODE_VEGETATION = "VegetationMask"
NODE_MIX_COLOR = "VegetationMix"
NODE_BARE_GROUND = "BareGroundRamp"
FALLBACK_IMAGE = "vegetation_fallback_white"


def safe_remove_image(img):
    """Löscht ein Image sicher, nur wenn es existiert und nicht mehr genutzt wird"""
    if img and img in bpy.data.images.values():
        try:
            bpy.data.images.remove(img, do_unlink=True)
        except:
            pass

def safe_remove_texture(tex):
    """Löscht eine Texture sicher"""
    if tex and tex in bpy.data.textures.values():
        try:
            bpy.data.textures.remove(tex, do_unlink=True)
        except:
            pass

def first_node(nodes, node_type):
    for node in nodes:
        if node.type == node_type:
            return node
    return None

def get_or_create(nodes, name, node_type, location):
    node = nodes.get(name)
    if node is None:
        node = nodes.new(node_type)
        node.name = name
        node.label = name
        node.location = location
    return node

def link(links, output_socket, input_socket):
    """Verbindet zwei Sockets und ersetzt eine vorhandene Verbindung am Eingang"""
    for existing in list(input_socket.links):
        links.remove(existing)
    links.new(output_socket, input_socket)

def ensure_vegetation_setup(material, node_heightmap):
    """Ergänzt das Material um die Vegetationsmaske (idempotent), gibt den Masken-Node zurück.
    Meer-Shader (Diffuse/Mix Shader) und Principled-Shader bleiben unverändert."""
    nodes = material.node_tree.nodes
    links = material.node_tree.links

    tex_coord = first_node(nodes, 'TEX_COORD')
    color_ramp = first_node(nodes, 'VALTORGB')
    missing = [n for n, v in (("Texture Coordinate", tex_coord), ("ColorRamp", color_ramp)) if v is None]
    if missing:
        raise RuntimeError(f"Nodes im Material nicht gefunden: {', '.join(missing)}")

    x, y = node_heightmap.location
    node_vegetation = get_or_create(nodes, NODE_VEGETATION, 'ShaderNodeTexImage', (x, y - 320))
    node_vegetation.interpolation = node_heightmap.interpolation
    node_vegetation.extension = node_heightmap.extension

    mix_color = get_or_create(nodes, NODE_MIX_COLOR, 'ShaderNodeMix', (x + 300, y - 120))
    mix_color.data_type = 'RGBA'
    mix_color.blend_type = 'MIX'
    mix_color.factor_mode = 'UNIFORM'
    mix_color.clamp_factor = True

    # Kahler Boden: Höhe auf [BARE_GROUND_VALUE, 1] gestreckt, damit Hochlagen ohne Vegetation braun statt konstant ocker werden
    bare = get_or_create(nodes, NODE_BARE_GROUND, 'ShaderNodeMath', (x + 150, y - 240))
    bare.operation = 'MULTIPLY_ADD'
    bare.use_clamp = True
    bare.inputs[1].default_value = 1.0 - BARE_GROUND_VALUE
    bare.inputs[2].default_value = BARE_GROUND_VALUE
    link(links, node_heightmap.outputs["Color"], bare.inputs[0])

    # Mix-Color-Sockets über Index (Namen A/B existieren je Datentyp mehrfach): 0 Factor, 6 A (Color), 7 B (Color), Ausgang 2 Result (Color)
    link(links, bare.outputs[0], mix_color.inputs[6])

    link(links, tex_coord.outputs["Generated"], node_vegetation.inputs["Vector"])
    link(links, node_vegetation.outputs["Color"], mix_color.inputs[0])
    link(links, node_heightmap.outputs["Color"], mix_color.inputs[7])
    link(links, mix_color.outputs[2], color_ramp.inputs["Fac"])

    return node_vegetation

def load_vegetation_mask(path):
    """Lädt die Maske als Non-Color; ohne Datei ein weißes Bild (= reine Höhenfärbung wie bisher)"""
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

def process_all_tiles(input_dir, output_dir):

    # --- Input scannen (Vegetationsmasken und frühere Renderings sind keine Heightmaps) ---
    files = [f for f in os.listdir(input_dir)
             if f.endswith(".png") and "_vegetation_" not in f and not f.endswith("_render.png")]
    files.sort()

    if not files:
        print("ERROR: Keine *.png Heightmap-Dateien  gefunden!")
        return

    # --- Material holen ---
    material = bpy.data.materials.get("TerrainMaterial")
    if not material:
        print("ERROR: Material 'TerrainMaterial' nicht gefunden!")
        return

    material_nodes = material.node_tree.nodes

    # --- Node Handles (namenssicher) ---
    try:
        node_heightmap = material_nodes["Image Texture.001"]
    except KeyError as e:
        print(f"ERROR: Image Texture Node nicht gefunden: {e}")
        return

    try:
        node_vegetation = ensure_vegetation_setup(material, node_heightmap)
    except RuntimeError as e:
        print(f"ERROR: {e}")
        return

    # --- Szene Setup ---
    scene = bpy.context.scene
    scene.render.resolution_x = 7202
    scene.render.resolution_y = 7202
    scene.render.resolution_percentage = 100
    scene.cycles.samples = 1000

    plane = bpy.data.objects.get("Plane")
    if not plane:
        print("ERROR: Objekt 'Plane' nicht gefunden!")
        return

    displace = plane.modifiers.get("Displace")
    if not displace:
        print("ERROR: Modifier 'Displace' nicht gefunden!")
        return

    total = len(files)
    print(f"\n=== Batch Start: {total} Tiles ===\n")

    # --- Batch Loop ---
    for idx, fname in enumerate(files, start=1):

        base_name = fname[:-4]
        heightmap_path = os.path.join(input_dir, fname)
        vegetation_path = os.path.join(input_dir, f"{base_name}_vegetation_{VEGETATION_BIOME}.png")
        output_path = os.path.join(output_dir, f"{base_name}_render.png")

        print(f"\n[{idx}/{total}] Tile: {base_name}")

        # --- Heightmap laden ---
        try:
            heightmap_img = bpy.data.images.load(heightmap_path, check_existing=True)
            heightmap_img.colorspace_settings.name = 'Non-Color'
        except Exception as e:
            print(f"ERROR: Konnte Heightmap nicht laden: {e}")
            continue

        # --- Vegetationsmaske laden ---
        try:
            vegetation_img, vegetation_loaded = load_vegetation_mask(vegetation_path)
            vegetation_img.colorspace_settings.name = 'Non-Color'
        except Exception as e:
            print(f"ERROR: Konnte Vegetationsmaske nicht laden: {e}")
            continue

        # --- Displacement Texture vorbereiten ---
        texture_name = f"disp_{base_name}"
        old_texture = bpy.data.textures.get(texture_name)
        if old_texture:
            safe_remove_texture(old_texture)

        disp_texture = bpy.data.textures.new(texture_name, type='IMAGE')
        disp_texture.image = heightmap_img
        disp_texture.image.colorspace_settings.name = 'Non-Color'

        displace.texture = disp_texture

        # --- Material Nodes aktualisieren ---
        node_heightmap.image = heightmap_img
        node_vegetation.image = vegetation_img

        # --- Render ---
        try:
            scene.render.filepath = output_path
            print(f"Render: {output_path}")
            bpy.ops.render.render(write_still=True)
        except Exception as e:
            print(f"RENDER ERROR: {e}")
            continue

        # --- Cleanup ---
        displace.texture = None
        bpy.context.view_layer.update()

        safe_remove_texture(disp_texture)
        safe_remove_image(heightmap_img)
        if vegetation_loaded:
            safe_remove_image(vegetation_img)

    print("\n=== Batch fertig ===")


# ----------------------- Ausführen -----------------------------------
if __name__ == "__main__":
    process_all_tiles(INPUT_DIR, OUTPUT_DIR)
