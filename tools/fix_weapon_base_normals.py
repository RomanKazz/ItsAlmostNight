"""Split the shared lower-cap normals from round weapon pedestal walls.

Only the three authored pedestal attributes/indices change. Repack their
existing views in place so the upgrade bakers can still trim/rebuild overlays.
Geometry, UVs, bounds, materials, nodes and triangle counts stay the same.
"""
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
PATHS = ('assets/models/buildings/cannon.glb', 'assets/models/buildings/catapult.glb',
         'assets/models/weapons/crossbow.glb')


def fix(path):
    raw = path.read_bytes()
    json_size = struct.unpack_from('<I', raw, 12)[0]
    doc = json.loads(raw[20:20 + json_size])
    binary = bytearray(raw[28 + json_size:28 + json_size + doc['buffers'][0]['byteLength']])
    mesh = next(m for m in doc['meshes'] if m['name'].startswith('weapon-'))
    primitive = mesh['primitives'][0]
    assert set(primitive['attributes']) == {'POSITION', 'NORMAL', 'TEXCOORD_0'}

    def read(index):
        accessor = doc['accessors'][index]
        view = doc['bufferViews'][accessor['bufferView']]
        assert accessor.get('byteOffset', 0) == 0 and 'byteStride' not in view
        assert sum(a.get('bufferView') == accessor['bufferView'] for a in doc['accessors']) == 1
        component = {5126: 'f', 5123: 'H', 5125: 'I'}[accessor['componentType']]
        size = {'VEC3': 3, 'VEC2': 2, 'SCALAR': 1}[accessor['type']]
        step = struct.calcsize(component) * size
        return [list(struct.unpack_from('<' + component * size, binary,
                    view['byteOffset'] + i * step)) for i in range(accessor['count'])]

    positions = read(primitive['attributes']['POSITION'])
    normals = read(primitive['attributes']['NORMAL'])
    uvs = read(primitive['attributes']['TEXCOORD_0'])
    indices = [v[0] for v in read(primitive['indices'])]
    sides, bottom = set(), []
    for offset in range(0, len(indices), 3):
        ids = indices[offset:offset + 3]
        points = [positions[i] for i in ids]
        ys = [p[1] for p in points]
        radii = [math.hypot(p[0], p[2]) for p in points]
        if max(abs(y) for y in ys) < 1e-5:
            bottom.append(offset)
        elif max(ys) < .251 and max(ys) - min(ys) > 1e-5 and min(radii) > .5999 and max(radii) < .6001:
            sides.update(ids)
    assert len(sides) == 24 and len(bottom) == 12, 'Unexpected authored pedestal topology'
    shared = sides.intersection(indices[i + j] for i in bottom for j in range(3))
    incorrect = any(abs(normals[i][1]) > 1e-5 for i in sides)
    if not shared and not incorrect:
        return 0
    assert len(shared) == 12, 'Unexpected cap/side seam'
    assert all(positions[i][1] < 1e-5 for i in shared)
    for i in sides:
        x, _, z = positions[i]
        radius = math.hypot(x, z)
        normals[i] = [x / radius, 0, z / radius]
    duplicates = {}
    for i in sorted(shared):
        duplicates[i] = len(positions)
        positions.append(positions[i][:])
        normals.append([0, -1, 0])
        uvs.append(uvs[i][:])
    for offset in bottom:
        for corner in range(3):
            i = indices[offset + corner]
            indices[offset + corner] = duplicates.get(i, i)
    assert len(positions) < 65536

    replacements = {}
    for name, values in [('POSITION', positions), ('NORMAL', normals), ('TEXCOORD_0', uvs),
                         ('indices', [[i] for i in indices])]:
        index = primitive['indices'] if name == 'indices' else primitive['attributes'][name]
        accessor = doc['accessors'][index]
        component = {5126: 'f', 5123: 'H', 5125: 'I'}[accessor['componentType']]
        blob = struct.pack('<' + component * sum(len(v) for v in values),
                           *(value for entry in values for value in entry))
        replacements[accessor['bufferView']] = blob
        accessor['count'] = len(values)

    # Every untouched view must retain its original contents, including the
    # embedded PNG and all level overlays. Moving offsets does not alter IDs.
    untouched = {i: bytes(binary[v.get('byteOffset', 0):v.get('byteOffset', 0) + v['byteLength']])
                 for i, v in enumerate(doc['bufferViews']) if i not in replacements}
    for view_index in sorted(replacements, key=lambda i: doc['bufferViews'][i]['byteOffset'], reverse=True):
        view = doc['bufferViews'][view_index]
        start, old_size = view['byteOffset'], view['byteLength']
        blob = replacements[view_index]
        assert len(blob) % 4 == 0
        binary[start:start + old_size] = blob
        difference = len(blob) - old_size
        view['byteLength'] = len(blob)
        for i, other in enumerate(doc['bufferViews']):
            if i != view_index and other.get('byteOffset', 0) >= start + old_size:
                other['byteOffset'] += difference
    for i, original in untouched.items():
        view = doc['bufferViews'][i]
        assert original == bytes(binary[view.get('byteOffset', 0):view.get('byteOffset', 0) + view['byteLength']])
    doc['buffers'][0]['byteLength'] = len(binary)
    encoded = json.dumps(doc, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    binary += b'\0' * (-len(binary) % 4)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    path.write_bytes(struct.pack('<III', 0x46546C67, 2, total) +
        struct.pack('<II', len(encoded), 0x4E4F534A) + encoded +
        struct.pack('<II', len(binary), 0x004E4942) + binary)
    return len(duplicates)


if __name__ == '__main__':
    for relative in PATHS:
        path = ROOT / relative
        print(relative, 'split cap vertices:', fix(path))
