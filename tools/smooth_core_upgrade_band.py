"""Bake smooth shading into the reusable core upgrade ring, stdlib only."""
import json
import struct
from pathlib import Path

from upgrade_mesh import Geometry


path = Path('assets/models/buildings/core.glb')
data = bytearray(path.read_bytes())
json_size = struct.unpack_from('<I', data, 12)[0]
doc = json.loads(data[20:20 + json_size])
binary_start = 28 + json_size
mesh = next(mesh for mesh in doc['meshes'] if mesh.get('name') == 'core_upgrade_band')
primitive = mesh['primitives'][0]
assert primitive.get('mode', 4) == 4 and 'indices' not in primitive


def layout(attribute):
    accessor = doc['accessors'][primitive['attributes'][attribute]]
    view = doc['bufferViews'][accessor['bufferView']]
    assert accessor['componentType'] == 5126 and accessor['type'] == 'VEC3'
    offset = binary_start + view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
    return offset, view.get('byteStride', 12), accessor['count']


position_offset, position_stride, count = layout('POSITION')
normal_offset, normal_stride, normal_count = layout('NORMAL')
assert count == normal_count and count % 3 == 0
positions = [struct.unpack_from('<3f', data, position_offset + index * position_stride)
             for index in range(count)]
geometry = Geometry()
for index in range(0, count, 3):
    geometry.face(positions[index:index + 3], (255, 255, 255))
assert len(geometry.normals) == count * 3
normals = geometry.smoothed_normals()
for index in range(count):
    struct.pack_into('<3f', data, normal_offset + index * normal_stride,
                     *normals[index * 3:index * 3 + 3])
# Preserve every other byte, including the authored pedestal and crystals.
path.write_bytes(data)
