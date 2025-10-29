import bpy

def color_heightmap_faces_five_colors_green(obj_name="Heightmap"):
    """
    Färbt die Faces eines Mesh-Objekts basierend auf ihrer Höhe (Z-Koordinate)
    mit fünf verschiedenen Farbbereichen: Beige und verschiedene Grüntöne.

    Args:
        obj_name (str): Der Name des Mesh-Objekts, das eingefärbt werden soll.
    """

    # Das Objekt finden
    obj = bpy.data.objects.get(obj_name)
    if not obj or obj.type != 'MESH':
        print(f"Fehler: Objekt '{obj_name}' nicht gefunden oder ist kein Mesh-Objekt.")
        return

    # Sicherstellen, dass wir im Object Mode sind
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # Material erstellen oder vorhandenes Material verwenden
    mat_name = "Heightmap_Material_BetterGreens" # Materialname angepasst
    if mat_name not in bpy.data.materials:
        mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True
    else:
        mat = bpy.data.materials[mat_name]

    # Material dem Objekt zuweisen, falls noch nicht geschehen
    # if mat not in obj.data.materials:
    obj.data.materials.append(mat)

    # Sicherstellen, dass das Material den "Color" Attributknoten für die Face-Farben hat
    if not mat.node_tree.nodes.get('Attribute') or mat.node_tree.nodes['Attribute'].attribute_name != "FaceColor":
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links

        principled_bsdf = None
        for node in nodes:
            if node.type == 'BSDF_PRINCIPLED':
                principled_bsdf = node
                break

        if principled_bsdf is None:
            print("Fehler: Principled BSDF-Knoten im Material nicht gefunden.")
            return

        attr_node = nodes.new(type='ShaderNodeAttribute')
        attr_node.attribute_name = "FaceColor"
        attr_node.location = (-300, 300)
        links.new(attr_node.outputs['Color'], principled_bsdf.inputs['Base Color'])
        print("Attribute-Knoten 'FaceColor' erstellt und mit Principled BSDF verbunden.")
    else:
        print("Attribute-Knoten 'FaceColor' existiert bereits.")


    # Z-Koordinaten aller Vertices ermitteln
    z_coords = [v.co.z for v in obj.data.vertices]
    min_z = min(z_coords)
    max_z = max(z_coords)

    print(f"Minimale Z-Koordinate: {min_z:.2f}")
    print(f"Maximale Z-Koordinate: {max_z:.2f}")

    if max_z == min_z:
        print("Alle Vertices haben die gleiche Z-Koordinate. Keine Höhenunterschiede zum Färben.")
        return

    # Farben definieren (RGB-Werte, 0-1 Bereich) - JETZT MIT BESSEREN GRÜNTÖNEN!
    # Beige (für die tiefsten Stellen, z.B. Talsohle oder Geröll)
    color_beige = (0.96, 0.96, 0.86, 1.0)      # Dezentes Beige

    # Helles Grasgrün (für niedrigere, grasbewachsene Hänge)
    color_light_green = (0.4, 0.8, 0.2, 1.0)   # Deutlich grünlicher

    # Mittleres Grün (für dichte Wiesen oder junge Wälder)
    color_green = (0.2, 0.65, 0.1, 1.0)        # Sattes Grün

    # Mitteldunkles Waldgrün (für dichtere Wälder)
    color_medium_dark_green = (0.1, 0.45, 0.05, 1.0) # Tiefes Waldgrün

    # Dunkelgrün / Fast Tannen-Grün (für die höchsten, dicht bewaldeten Regionen)
    color_dark_green = (0.05, 0.3, 0.02, 1.0)   # Sehr dunkles, sattes Grün

    # Face-Farben erstellen oder aktualisieren
    if "FaceColor" not in obj.data.color_attributes:
        color_attribute = obj.data.color_attributes.new(name="FaceColor", type='FLOAT_COLOR', domain='FACE')
    else:
        color_attribute = obj.data.color_attributes["FaceColor"]
        color_attribute.data.clear()


    # Faces durchlaufen und Farbe zuweisen
    for face in obj.data.polygons:
        face_z_sum = sum(obj.data.vertices[v_idx].co.z for v_idx in face.vertices)
        face_avg_z = face_z_sum / len(face.vertices)

        normalized_z = (face_avg_z - min_z) / (max_z - min_z)

        # Farbzuweisung basierend auf normalisierter Höhe in 5 Bereichen
        # Schwellenwerte: 0.2, 0.4, 0.6, 0.8 (können bei Bedarf angepasst werden)
#        if normalized_z < 0.4:
#            color_attribute.data[face.index].color = color_beige
#        elif normalized_z > 0.4 and normalized_z < 0.8:
#            color_attribute.data[face.index].color = color_green
#        else:
#            color_attribute.data[face.index].color = color_dark_green

        if normalized_z < 0.2:
            color_attribute.data[face.index].color = color_beige
        elif normalized_z < 0.4:
            color_attribute.data[face.index].color = color_light_green
        elif normalized_z < 0.6:
            color_attribute.data[face.index].color = color_green
        elif normalized_z < 0.8:
            color_attribute.data[face.index].color = color_medium_dark_green
        else:
            color_attribute.data[face.index].color = color_dark_green

    print(f"Faces von Objekt '{obj_name}' basierend auf Höhe mit verbesserten Grüntönen eingefärbt.")
    print("Stellen Sie sicher, dass Ihr Material einen 'Attribute'-Knoten namens 'FaceColor' verwendet, der mit der 'Base Color' des Principled BSDF verbunden ist.")


# --- SKRIPT AUSFÜHREN ---
# Ändern Sie den Namen des Objekts, falls Ihr Heightmap-Objekt anders heißt.
color_heightmap_faces_five_colors_green(obj_name="Plane") # Beispiel: Wenn dein Heightmap-Objekt "Plane" heißt