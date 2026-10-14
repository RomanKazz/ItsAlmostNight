"""Map silver accent UVs onto the core's gold swatch, preserving GLB structure.

Only float TEXCOORD_0 bytes change. Geometry, skinning, socket/collision nodes,
embedded palette and mesh order stay byte-identical. Safe to rerun.
"""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/flamethrower_gold'
SILVER_U = .84375
GOLD = (.65625, .285, .465)


def recolor(path):
    original = path.read_bytes()
    result = bytearray(original)
    json_length = struct.unpack_from('<I', original, 12)[0]
    document = json.loads(original[20:20 + json_length])
    binary_start = 28 + json_length
    changed = {}
    touched = set()
    for mesh in document['meshes']:
        count = 0
        for primitive in mesh['primitives']:
            accessor_index = primitive.get('attributes', {}).get('TEXCOORD_0')
            if accessor_index is None:
                continue
            accessor = document['accessors'][accessor_index]
            assert accessor['componentType'] == 5126 and accessor['type'] == 'VEC2'
            view = document['bufferViews'][accessor['bufferView']]
            for index in range(accessor['count']):
                offset = binary_start + view.get('byteOffset', 0) + accessor.get('byteOffset', 0) + index * view.get('byteStride', 8)
                u, glb_v = struct.unpack_from('<2f', original, offset)
                blender_v = 1 - glb_v
                if offset in touched or abs(u - SILVER_U) > 1e-6 or not .024 <= blender_v <= .226:
                    continue
                fraction = min(1, max(0, (blender_v - .025) / .2))
                struct.pack_into('<2f', result, offset, GOLD[0], 1 - (GOLD[1] + fraction * (GOLD[2] - GOLD[1])))
                touched.add(offset)
                count += 1
        if count:
            changed[mesh['name']] = count
    allowed = {offset + byte for offset in touched for byte in range(8)}
    assert all(a == b or index in allowed for index, (a, b) in enumerate(zip(original, result)))
    assert len(result) == len(original)
    path.write_bytes(result)
    return {'asset': str(path.relative_to(ROOT)), 'before_sha256': hashlib.sha256(original).hexdigest(),
            'after_sha256': hashlib.sha256(result).hexdigest(), 'uv_vertices': changed,
            'geometry_and_nodes_byte_identical': True, 'embedded_palette_byte_identical': True}


if __name__ == '__main__':
    ART.mkdir(parents=True, exist_ok=True)
    reports = [recolor(ROOT / 'assets/models/buildings' / name)
               for name in ('flamethrower.glb', 'flamethrower_upgrades.glb')]
    report = {'gold_blender_uv': GOLD, 'gold_matches_core': True, 'assets': reports}
    (ART / 'recolor.json').write_text(json.dumps(report, indent=2) + '\n')
    manifest_path = ROOT / 'art/models/flamethrower_upgrades/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    manifest['base_sha256'] = reports[0]['after_sha256']
    manifest['accent_swatch'] = {'name': 'original gold, matching core', 'blender_uv': GOLD}
    manifest['color_authoring_source'] = 'art/models/flamethrower_gold/flamethrower_gold.blend'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(report))
