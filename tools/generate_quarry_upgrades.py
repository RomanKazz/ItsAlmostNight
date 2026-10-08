"""Bake compact quarry levels using the game's authored faceted rock geometry."""
import json
import math
import struct
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset

asset = GlbAsset(Path('assets/models/buildings/quarry.glb'))
ORANGE = (242, 81, 17)
STEEL = (142, 170, 187)
DARK_STEEL = (49, 78, 98)
GOLD = (247, 190, 40)

# Use the same four small decorative boulders seen throughout the world.
def load_rock(variant):
    data = Path(f'assets/models/environment/decor/rock_small_{variant}.glb').read_bytes()
    json_size = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + json_size])
    binary = data[28 + json_size:]
    primitive = doc['meshes'][0]['primitives'][0]
    def read_accessor(index):
        accessor = doc['accessors'][index]
        view = doc['bufferViews'][accessor['bufferView']]
        width = {'SCALAR': 1, 'VEC3': 3}[accessor['type']]
        component = {5126: 'f', 5123: 'H', 5125: 'I'}[accessor['componentType']]
        return struct.unpack_from('<' + component * width * accessor['count'], binary,
                                  view.get('byteOffset', 0) + accessor.get('byteOffset', 0))
    positions = read_accessor(primitive['attributes']['POSITION'])
    indices = read_accessor(primitive['indices'])
    minimum = [min(positions[axis::3]) for axis in range(3)]
    maximum = [max(positions[axis::3]) for axis in range(3)]
    return positions, indices, minimum, maximum


rocks = [load_rock(variant) for variant in 'abcd']


def boulder(part, center, size, yaw, color, variant=2):
    rock_positions, rock_indices, rock_min, rock_max = rocks[variant]
    cosine, sine = math.cos(yaw), math.sin(yaw)
    def vertex(index):
        value = rock_positions[index * 3:index * 3 + 3]
        x = ((value[0] - rock_min[0]) / (rock_max[0] - rock_min[0]) - .5) * size[0]
        y = (value[1] - rock_min[1]) / (rock_max[1] - rock_min[1]) * size[1]
        z = ((value[2] - rock_min[2]) / (rock_max[2] - rock_min[2]) - .5) * size[2]
        return (center[0] + x * cosine - z * sine,
                center[1] + y - size[1] * .08, center[2] + x * sine + z * cosine)
    for offset in range(0, len(rock_indices), 3):
        part.face([vertex(index) for index in rock_indices[offset:offset + 3]], color)


def outline(x, z):
    bevel = .22
    return [(-x + bevel, -z), (x - bevel, -z), (x, -z + bevel),
            (x, z - bevel), (x - bevel, z), (-x + bevel, z),
            (-x, z - bevel), (-x, -z + bevel)]


def profile(part, rings, color, cap_top=True):
    for (x0, z0, y0), (x1, z1, y1) in zip(rings, rings[1:]):
        low, high = outline(x0, z0), outline(x1, z1)
        for i in range(8):
            j = (i + 1) % 8
            part.face([(low[i][0], y0, low[i][1]), (high[i][0], y1, high[i][1]),
                       (high[j][0], y1, high[j][1]), (low[j][0], y0, low[j][1])], color)
    x, z, y = rings[0]
    part.face([(px, y, pz) for px, pz in outline(x, z)], color)
    if cap_top:
        x, z, y = rings[-1]
        part.face([(px, y, pz) for px, pz in reversed(outline(x, z))], color)


def rim(part, x, z, bottom, top, color):
    outer, inner = outline(x, z), outline(x - .065, z - .065)
    for i in range(8):
        j = (i + 1) % 8
        def point(p, y):
            return p[0], y, p[1]
        part.face([point(outer[i], bottom), point(outer[i], top),
                   point(outer[j], top), point(outer[j], bottom)], color)
        part.face([point(inner[j], bottom), point(inner[j], top),
                   point(inner[i], top), point(inner[i], bottom)], color)
        part.face([point(inner[i], top), point(inner[j], top),
                   point(outer[j], top), point(outer[i], top)], color)
        part.face([point(inner[j], bottom), point(inner[i], bottom),
                   point(outer[i], bottom), point(outer[j], bottom)], color)


for level in range(1, 9):
    part = Geometry()
    progress = level - 1
    x, z = .74 + progress * .007, .67 + progress * .007
    shoulder = .13 + progress * .014
    top = .28 + progress * .035
    metal = GOLD if level >= 5 else STEEL
    profile(part, [(x - .025, z - .025, .008), (x, z, .028),
                   (x, z, .058)], DARK_STEEL)
    profile(part, [(x, z, .058), (x - .008, z - .008, .085),
                   (x - .045, z - .045, shoulder)], ORANGE)
    if level >= 3:
        rim(part, x - .032, z - .032, shoulder - .016, shoulder + .018, metal)
    profile(part, [(x - .105, z - .105, shoulder - .005),
                   (x - .09, z - .09, top - .055),
                   (x - .09, z - .09, top)], ORANGE, cap_top=False)
    rim(part, x - .075, z - .075, top - .018, top + .038, metal)
    floor = top - .04
    profile(part, [(x - .14, z - .14, floor - .008),
                   (x - .14, z - .14, floor)], DARK_STEEL)
    # A dominant irregular boulder, rooted in the basin, with smaller satellites.
    boulder(part, (-.025, floor - .015, .08),
            (.60 + progress * .024, .66 + progress * .04, .56 + progress * .021), .20,
            (143, 160, 177))
    boulder(part, (-.39, floor - .018, -.13),
            (.30 + progress * .010, .31 + progress * .018, .28 + progress * .008), -.42,
            (121, 143, 161), variant=0)
    boulder(part, (.38, floor - .018, -.10),
            (.31 + progress * .008, .36 + progress * .020, .29 + progress * .008), .73,
            (158, 177, 190), variant=1)
    if level >= 3:
        boulder(part, (-.18, floor - .018, -.39), (.30, .27 + progress * .012, .26), 1.05,
                (127, 147, 163), variant=3)
    if level >= 5:
        boulder(part, (.32, floor - .018, .30), (.29, .32 + progress * .012, .27), -.82,
                (119, 140, 157), variant=0)
    if level >= 7:
        boulder(part, (.22, floor - .018, -.39), (.24, .25, .24), .42, (159, 176, 189), variant=1)
    part.append(asset, f'upgrade_quarry_l{level}')

print(f'Baked eight boulder quarry levels: {asset.write()} bytes; one draw per quarry.')
