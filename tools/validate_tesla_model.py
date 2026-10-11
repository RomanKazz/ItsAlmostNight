"""Validate the exported Tesla in a fresh Blender scene, without the source blend."""
import hashlib
import json
import math
from pathlib import Path
import struct

import bpy

root = Path(__file__).resolve().parents[1]
path = root / 'assets/models/buildings/tesla.glb'
data = path.read_bytes()
assert struct.unpack_from('<III', data) == (0x46546C67, 2, len(data))
size = struct.unpack_from('<I', data, 12)[0]
document = json.loads(data[20:20 + size])
binary = data[28 + size:]
assert len(document['images']) == 1
assert 'uri' not in document['images'][0], 'GLB requires an external texture'
view = document['bufferViews'][document['images'][0]['bufferView']]
offset = view.get('byteOffset', 0)
embedded = binary[offset:offset + view['byteLength']]
assert embedded == (root / 'assets/models/buildings/Textures/colormap.png').read_bytes()
assert embedded == (root / 'art/references/kenney_tower_defense/Textures/colormap.png').read_bytes()

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(path))
meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH']
assert len(meshes) == 23
points = {tuple(round(c, 5) for c in obj.matrix_world @ vertex.co)
          for obj in meshes for vertex in obj.data.vertices}
for x, y, z in points:
    for transformed in [(-y, x, z), (-x, y, z), (x, -y, z)]:
        assert tuple(round(c, 5) for c in transformed) in points, 'C4/mirror symmetry failure'
for obj in meshes:
    assert obj.data.uv_layers.active is not None
    assert all(all(math.isfinite(c) for c in normal.vector) and normal.vector.length > .9
               for normal in obj.data.corner_normals)
triangles = sum(sum(len(p.vertices) - 2 for p in obj.data.polygons) for obj in meshes)
assert triangles == 1904
assert any(obj.name == 'socket_muzzle' for obj in bpy.data.objects)
assert any(obj.name == 'socket_effect_energy' for obj in bpy.data.objects)
print('TESLA_VALIDATED', len(meshes), triangles, 'C4/mirror symmetry; UV; normals; original embedded PNG',
      hashlib.sha256(embedded).hexdigest())
