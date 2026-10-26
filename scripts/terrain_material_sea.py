import bpy

# -------------------------------------------------------------------
# Funktion zum Erstellen eines Terrain-Materials
# -------------------------------------------------------------------
def create_terrain_material(name, height_image, veg_image, min_height, max_height, sea_level_norm=0.05):
    """
    Erstellt ein Terrain-Material mit:
    - Heightmap für Topographie
    - Vegetationsmap für Grünflächen
    - Slope-basierte Vegetationsmaske
    - Höhenabhängige Farbvariation
    - Rock-Farbe
    - Meeresschwelle in Dunkelblau
    
    Parameter:
    - name: Name des Materials
    - height_image: bpy.types.Image für die Heightmap (Graustufen)
    - veg_image: bpy.types.Image für die Vegetationsmap (Graustufen)
    - min_height: niedrigster Höhenwert (z.B. Meer = 0)
    - max_height: höchster Höhenwert (z.B. Gipfel)
    - sea_level_norm: Normierter Wert für Meeresschwelle (0..1)
    """
    
    def new_node(tree, type, label, x, y):
        node = tree.nodes.new(type)
        node.label = label
        node.location = (x, y)
        return node
    
    # Material erstellen
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    tree = mat.node_tree
    
    # Bestehende Nodes löschen
    for n in tree.nodes:
        tree.nodes.remove(n)
    
    # --- Ausgangs-Shaders ---
    principled = new_node(tree, "ShaderNodeBsdfPrincipled", "Principled", 1200, 0)
    out = new_node(tree, "ShaderNodeOutputMaterial", "Output", 1400, 0)
    tree.links.new(principled.outputs["BSDF"], out.inputs["Surface"])
    
    # --- Image Textures ---
    height_tex = new_node(tree, "ShaderNodeTexImage", "Heightmap", -1000, 400)
    height_tex.image = height_image
    if height_image:
        height_tex.image.colorspace_settings.name = 'Non-Color'
    
    sep = new_node(tree, "ShaderNodeSeparateRGB", "SeparateHeight", -850, 400)
    tree.links.new(height_tex.outputs["Color"], sep.inputs["Image"])
    
    veg_tex = new_node(tree, "ShaderNodeTexImage", "VegetationMap", -1000, 0)
    veg_tex.image = veg_image
    if veg_image:
        veg_tex.image.colorspace_settings.name = 'Non-Color'
    
    # --- Height Normierung 0..1 ---
    h_norm_sub = new_node(tree, "ShaderNodeMath", "Norm Height Sub", -800, 200)
    h_norm_sub.operation = 'SUBTRACT'
    h_norm_sub.inputs[1].default_value = min_height
    
    h_norm_div = new_node(tree, "ShaderNodeMath", "Norm Height Div", -650, 200)
    h_norm_div.operation = 'DIVIDE'
    h_norm_div.inputs[1].default_value = max_height - min_height
    
    h_norm_clamp = new_node(tree, "ShaderNodeMath", "Norm Clamp", -500, 200)
    h_norm_clamp.operation = 'ADD'
    h_norm_clamp.use_clamp = True
    h_norm_clamp.inputs[1].default_value = 0.0
    
    tree.links.new(sep.outputs["R"], h_norm_sub.inputs[0])
    tree.links.new(h_norm_sub.outputs["Value"], h_norm_div.inputs[0])
    tree.links.new(h_norm_div.outputs["Value"], h_norm_clamp.inputs[0])
    
    # --- Slope-Berechnung ---
    h_mul = new_node(tree, "ShaderNodeMath", "Height Multiply", -650, 400)
    h_mul.operation = 'MULTIPLY'
    h_mul.inputs[1].default_value = 1.0
    tree.links.new(sep.outputs["R"], h_mul.inputs[0])
    
    bump = new_node(tree, "ShaderNodeBump", "Bump", -500, 400)
    tree.links.new(h_mul.outputs["Value"], bump.inputs["Height"])
    
    normal = new_node(tree, "ShaderNodeVectorMath", "Dot Normal", -300, 400)
    normal.operation = 'DOT_PRODUCT'
    normal.inputs[1].default_value = (0, 0, 1)
    tree.links.new(bump.outputs["Normal"], normal.inputs[0])
    
    slope_invert = new_node(tree, "ShaderNodeMath", "Slope", -150, 400)
    slope_invert.operation = 'SUBTRACT'
    slope_invert.inputs[0].default_value = 1.0
    tree.links.new(normal.outputs["Value"], slope_invert.inputs[1])
    
    # --- Vegetationsmaske ---
    veg_mask_mul = new_node(tree, "ShaderNodeMath", "VegMask", 50, 0)
    veg_mask_mul.operation = 'MULTIPLY'
    tree.links.new(veg_tex.outputs["Color"], veg_mask_mul.inputs[0])
    tree.links.new(slope_invert.outputs["Value"], veg_mask_mul.inputs[1])
    
    # --- Vegetations ColorRamp ---
    ramp = new_node(tree, "ShaderNodeValToRGB", "VegetationRamp", 300, 0)
    # Dunkelgrün
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.05, 0.20, 0.05, 1)
    # Hellgrün
    elem1 = ramp.color_ramp.elements.new(0.4)
    elem1.color = (0.25, 0.50, 0.20, 1)
    # Beige
    elem2 = ramp.color_ramp.elements.new(0.8)
    elem2.color = (0.75, 0.68, 0.48, 1)
    tree.links.new(veg_mask_mul.outputs["Value"], ramp.inputs["Fac"])
    
    # --- HSV Node (Sättigung/Helligkeit) ---
    hsv = new_node(tree, "ShaderNodeHueSaturation", "HSV", 600, 0)
    tree.links.new(ramp.outputs["Color"], hsv.inputs["Color"])
    
    # Sättigung = 1 - Höhe * 0.6
    sat_mul = new_node(tree, "ShaderNodeMath", "Sat Mul", 350, 200)
    sat_mul.operation = 'MULTIPLY'
    sat_mul.inputs[1].default_value = -0.6
    sat_add = new_node(tree, "ShaderNodeMath", "Sat Add", 450, 200)
    sat_add.operation = 'ADD'
    sat_add.inputs[1].default_value = 1.0
    tree.links.new(h_norm_clamp.outputs["Value"], sat_mul.inputs[0])
    tree.links.new(sat_mul.outputs["Value"], sat_add.inputs[0])
    tree.links.new(sat_add.outputs["Value"], hsv.inputs["Saturation"])
    
    # Helligkeit = 1 + (1 - Höhe) * 0.2
    v_invert = new_node(tree, "ShaderNodeMath", "V Invert", 350, -200)
    v_invert.operation = 'SUBTRACT'
    v_invert.inputs[0].default_value = 1.0
    v_mul = new_node(tree, "ShaderNodeMath", "V Mul", 450, -200)
    v_mul.operation = 'MULTIPLY'
    v_mul.inputs[1].default_value = 0.2
    v_add = new_node(tree, "ShaderNodeMath", "V Add", 550, -200)
    v_add.operation = 'ADD'
    v_add.inputs[1].default_value = 1.0
    tree.links.new(h_norm_clamp.outputs["Value"], v_invert.inputs[1])
    tree.links.new(v_invert.outputs["Value"], v_mul.inputs[0])
    tree.links.new(v_mul.outputs["Value"], v_add.inputs[0])
    tree.links.new(v_add.outputs["Value"], hsv.inputs["Value"])
    
    # --- Rock ColorRamp ---
    rock_ramp = new_node(tree, "ShaderNodeValToRGB", "RockRamp", 300, -400)
    rock_ramp.color_ramp.elements[0].color = (0.4, 0.4, 0.4, 1)
    rock_ramp.color_ramp.elements[1].color = (0.6, 0.6, 0.6, 1)
    tree.links.new(h_mul.outputs["Value"], rock_ramp.inputs["Fac"])
    
    # --- Vegetation vs Rock Mix ---
    mix = new_node(tree, "ShaderNodeMixRGB", "MixVegRock", 900, 0)
    mix.inputs["Fac"].default_value = 1.0
    tree.links.new(veg_mask_mul.outputs["Value"], mix.inputs["Fac"])
    tree.links.new(hsv.outputs["Color"], mix.inputs["Color1"])
    tree.links.new(rock_ramp.outputs["Color"], mix.inputs["Color2"])
    
    # --- Meeresschwelle ---
    sea_compare = new_node(tree, "ShaderNodeMath", "Sea Compare", 200, -200)
    sea_compare.operation = 'LESS_THAN'
    sea_compare.inputs[1].default_value = sea_level_norm
    tree.links.new(h_norm_clamp.outputs["Value"], sea_compare.inputs[0])
    
    sea_color = new_node(tree, "ShaderNodeRGB", "Sea Color", 200, -350)
    sea_color.outputs["Color"].default_value = (0.0, 0.05, 0.2, 1.0)
    
    sea_mix = new_node(tree, "ShaderNodeMixRGB", "Sea Mix", 450, -250)
    sea_mix.blend_type = 'MIX'
    sea_mix.inputs[0].default_value = 1.0
    tree.links.new(sea_color.outputs["Color"], sea_mix.inputs["Color1"])
    tree.links.new(mix.outputs["Color"], sea_mix.inputs["Color2"])
    tree.links.new(sea_compare.outputs["Value"], sea_mix.inputs["Fac"])
    
    # --- Output ---
    tree.links.new(sea_mix.outputs["Color"], principled.inputs["Base Color"])
    
    return mat

# -------------------------------------------------------------------
# Beispiel-Aufruf:
# -------------------------------------------------------------------
obj = bpy.context.active_object
height_img = bpy.data.images.get("heightmap.png")
veg_img = bpy.data.images.get("vegetationmap.png")
mat = create_terrain_material("TerrainMaterial", height_img, veg_img, 0, 2635, sea_level_norm=0.05)

if obj and obj.type == 'MESH':
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

print("TerrainMaterial erstellt und auf aktives Objekt gesetzt.")
