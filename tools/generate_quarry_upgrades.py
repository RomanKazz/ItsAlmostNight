"""Bake eight compact stone collector bodies; preserve the textured stone pile."""
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset

asset = GlbAsset(Path('assets/models/buildings/quarry.glb'))
ORANGE = (242, 81, 17)
STEEL = (142, 170, 187)
DARK_STEEL = (49, 78, 98)
GOLD = (247, 190, 40)


def outline(x, z):
    bevel = .12
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
    x, z = .81 + progress * .009, .67 + progress * .009
    floor = .18 + progress * .025
    metal = GOLD if level >= 5 else STEEL
    profile(part, [(x, z, .008), (x + .025, z + .025, .025),
                   (x + .025, z + .025, .055)], DARK_STEEL)
    body = [(x + .015, z + .015, .055), (x, z, .08)]
    if level >= 7: body.append((x, z, .13))
    body.append((x - .06, z - .06, floor - .012))
    profile(part, body, ORANGE, cap_top=False)
    # Low steel basin surrounds the stones; its floor is intentionally recessed.
    profile(part, [(x - .07, z - .07, floor - .018),
                   (x - .07, z - .07, floor - .012)], DARK_STEEL)
    rim(part, x - .045, z - .045, floor - .022, floor + .033, metal)
    if level >= 3:
        rim(part, x + .008, z + .008, .072, .093, STEEL if level < 7 else GOLD)
    if level >= 7:
        # A broad lower tier rather than bolts, brackets or small decoration.
        rim(part, x + .008, z + .008, .125, .146, GOLD)
    part.append(asset, f'upgrade_quarry_l{level}')

print(f'Baked eight quarry bodies: {asset.write()} bytes; two draws per quarry.')
