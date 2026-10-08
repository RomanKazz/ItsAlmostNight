"""Bake eight clean collector housings for the crystal mine; stdlib only.

Original geometry remains in the asset for recovery. Runtime draws the authored
crystal cluster, one replacement housing and copies of the original large shard.
"""

import math
import struct
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset

asset = GlbAsset(Path("assets/models/buildings/mine.glb"))
STEEL = (142, 170, 187)
DARK_STEEL = (49, 78, 98)
ORANGE = (242, 81, 17)
GOLD = (247, 190, 40)
SEGMENTS = 8


def section(geometry, rings, color, cap_top=True):
    """A solid octagonal lathed profile with outward-facing flat facets."""
    def point(radius, y, index):
        angle = (index + .5) * math.tau / SEGMENTS
        return (math.cos(angle) * radius, y, math.sin(angle) * radius)
    for (r0, y0), (r1, y1) in zip(rings, rings[1:]):
        for i in range(SEGMENTS):
            geometry.face((point(r0, y0, i), point(r1, y1, i),
                           point(r1, y1, i + 1), point(r0, y0, i + 1)), color)
    radius, height = rings[0]
    geometry.face([point(radius, height, i) for i in range(SEGMENTS)], color)
    if cap_top:
        radius, height = rings[-1]
        geometry.face([point(radius, height, i) for i in reversed(range(SEGMENTS))], color)


def rim(geometry, outer, inner, bottom, top, color):
    # Match the eight flat sides of the housing rather than alternating facets.
    start = len(geometry.positions)
    geometry.ring(1, (0, (bottom + top) * .5, 0), outer, inner,
                  top - bottom, color, segments=SEGMENTS)
    cosine, sine = math.cos(math.pi / SEGMENTS), math.sin(math.pi / SEGMENTS)
    for values in (geometry.positions, geometry.normals):
        for index in range(start, len(values), 3):
            x, z = values[index], values[index + 2]
            values[index], values[index + 2] = x * cosine - z * sine, x * sine + z * cosine



for level in range(1, 9):
    part = Geometry()
    progress = level - 1
    # A broad orange body and just two readable metal edges, like the core.
    foot_radius = .435 + progress * .006
    shoulder = .10 + progress * .011
    cup_top = .18 + progress * .023
    metal = GOLD if level >= 5 else STEEL
    section(part, [(foot_radius - .025, .008), (foot_radius, .025),
                   (foot_radius, .045)], DARK_STEEL)
    section(part, [(foot_radius, .045), (foot_radius - .005, .068),
                   (foot_radius - .035, shoulder)], ORANGE)
    if level >= 3:
        rim(part, foot_radius - .027, foot_radius - .080,
            shoulder - .014, shoulder + .012, STEEL if level < 7 else GOLD)
    # The collector narrows above its foot and opens around the crystal cluster.
    cup_radius = .363 + progress * .008
    section(part, [(cup_radius - .030, shoulder - .004),
                   (cup_radius, cup_top - .032),
                   (cup_radius, cup_top)], ORANGE, cap_top=False)
    rim(part, cup_radius + .014, cup_radius - .042,
        cup_top - .010, cup_top + (.026 if level == 8 else .017), metal)
    # Dark recessed tray: crystals remain the focal point above the metal lip.
    section(part, [(cup_radius - .042, cup_top - .028),
                   (cup_radius - .042, cup_top - .020)], DARK_STEEL)
    if level == 8:
        rim(part, foot_radius + .003, foot_radius - .024, .034, .052, GOLD)
    part.append(asset, f"upgrade_crystal_mine_l{level}")

# Copy the original large shard, including its texture coordinates and normals.
source = asset.doc['meshes'][0]['primitives'][1]
def read_accessor(index):
    entry = asset.doc['accessors'][index]
    view = asset.doc['bufferViews'][entry['bufferView']]
    width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3}[entry['type']]
    fmt = {5126: 'f', 5123: 'H', 5125: 'I'}[entry['componentType']]
    return struct.unpack_from('<' + fmt * entry['count'] * width, asset.binary,
                              view.get('byteOffset', 0) + entry.get('byteOffset', 0))
positions = read_accessor(source['attributes']['POSITION'])
indices = read_accessor(source['indices'])
selected = [index for offset in range(0, len(indices), 3)
            if all(positions[index * 3] > .08 for index in indices[offset:offset + 3])
            for index in indices[offset:offset + 3]]
attributes = {}
for semantic, index in source['attributes'].items():
    original = asset.doc['accessors'][index]
    width = {'VEC2': 2, 'VEC3': 3}[original['type']]
    values = read_accessor(index)
    copied = [value for vertex in selected for value in values[vertex * width:(vertex + 1) * width]]
    if semantic == 'POSITION':
        for offset in range(0, len(copied), 3):
            copied[offset] -= .21811
            copied[offset + 1] -= .12005
            copied[offset + 2] -= .071995
    asset.binary.extend(b'\0' * (-len(asset.binary) % 4))
    start = len(asset.binary)
    asset.binary.extend(struct.pack('<' + 'f' * len(copied), *copied))
    view = len(asset.doc['bufferViews'])
    asset.doc['bufferViews'].append({'buffer': 0, 'byteOffset': start,
        'byteLength': len(copied) * 4, 'target': 34962})
    entry = {'bufferView': view, 'componentType': 5126,
             'count': len(selected), 'type': original['type']}
    if semantic == 'POSITION':
        entry.update(min=[min(copied[i::3]) for i in range(3)],
                     max=[max(copied[i::3]) for i in range(3)])
    attributes[semantic] = len(asset.doc['accessors'])
    asset.doc['accessors'].append(entry)
mesh = len(asset.doc['meshes'])
asset.doc['meshes'].append({'name': 'upgrade_crystal_mine_original_shard',
    'primitives': [{'attributes': attributes, 'material': source['material']}]})
node = len(asset.doc['nodes'])
asset.doc['nodes'].append({'mesh': mesh, 'name': 'upgrade_crystal_mine_original_shard'})
asset.doc['scenes'][asset.doc.get('scene', 0)]['nodes'].append(node)
print(f"Baked eight crystal-mine housings and original shard copies: {asset.write()} bytes.")
