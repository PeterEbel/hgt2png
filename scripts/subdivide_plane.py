import bpy

plane = bpy.data.objects.get("Plane")
if plane is None:
    raise RuntimeError("Objekt 'Plane' nicht gefunden")

if bpy.context.object and bpy.context.object.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

old_mesh = plane.data

bpy.ops.mesh.primitive_grid_add(
    x_subdivisions=3600,
    y_subdivisions=3600,
    size=2.0,
)
grid_object = bpy.context.object
new_mesh = grid_object.data

expected_vertices = 3601 * 3601
if len(new_mesh.vertices) != expected_vertices:
    bpy.data.objects.remove(grid_object, do_unlink=True)
    raise RuntimeError(
        f"Grid hat {len(new_mesh.vertices)} Vertices; "
        f"erwartet wurden {expected_vertices}"
    )

plane.data = new_mesh
new_mesh.name = "Plane_SRTM1_3601"

bpy.data.objects.remove(grid_object, do_unlink=True)
if old_mesh.users == 0:
    bpy.data.meshes.remove(old_mesh)