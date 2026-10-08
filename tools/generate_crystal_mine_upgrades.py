"""Bake compact mine housings while preserving its original crystal geometry.

The original two primitives remain untouched. Each higher level has one static
overlay, so preview, shadows, and selection use the same simple silhouette.
"""

import math
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset


asset = GlbAsset(Path("assets/models/buildings/mine.glb"))
STEEL = (131, 162, 184)
DARK_STEEL = (57, 91, 116)
ORANGE = (232, 78, 16)
GOLD = (242, 185, 34)


def drum(geometry, radius, bottom, top, color):
    polygon = [(math.cos(i * math.tau / 10) * radius,
                math.sin(i * math.tau / 10) * radius) for i in range(10)]
    geometry.prism(polygon, 1, bottom, top, color)


def bracket(geometry, side, height, color):
    """A wide side support, with a visible gap around the crystal cluster."""
    outer = side * .43
    inner = side * .32
    geometry.prism([(outer, .10), (outer, height - .08),
                    (inner, height), (inner, height - .07),
                    (side * .38, height - .13), (side * .38, .10)],
                   2, -.105, .105, color)


def end_bracket(geometry, side):
    outer = side * .43
    inner = side * .33
    geometry.prism([(.10, outer), (.275, outer),
                    (.335, inner), (.270, inner),
                    (.235, side * .38), (.10, side * .38)],
                   0, -.085, .085, DARK_STEEL)


for level in range(2, 9):
    part = Geometry()
    # One broad orange pedestal echoes the core's base. Keep the original
    # violet satellites fully visible instead of replacing them with spikes.
    part.ring(1, (0, .057, 0), .473, .407, .080, ORANGE, segments=10)
    part.ring(1, (0, .104, 0), .458, .409, .017, STEEL, segments=10)
    if level >= 3:
        # The collector cup hides the green lower stem, but stops below the
        # original floating shards and never grows into a second crystal.
        drum(part, .245, .100, .230, ORANGE)
        part.ring(1, (0, .226, 0), .266, .213, .026, DARK_STEEL, segments=10)
    if 4 <= level < 6:
        for side in (-1, 1):
            bracket(part, side, .310, STEEL)
    if level >= 5:
        part.ring(1, (0, .059, 0), .479, .463, .024, GOLD, segments=10)
        part.ring(1, (0, .238, 0), .271, .226, .025, GOLD, segments=10)
    if level >= 6:
        for side in (-1, 1):
            bracket(part, side, .390, ORANGE)
            part.box((side * .335, .365, 0), (.122, .025, .235), GOLD)
    if level >= 7:
        # Tapered front and rear clamps complete the mining frame.
        for side in (-1, 1):
            end_bracket(part, side)
    if level == 8:
        part.ring(1, (0, .096, 0), .485, .459, .025, GOLD, segments=10)
        for z in (-.40, .40):
            part.box((0, .328, z), (.180, .025, .090), GOLD)
    part.append(asset, f"upgrade_crystal_mine_l{level}")

print(f"Baked seven crystal-mine levels: {asset.write()} bytes; one extra draw per upgraded mine.")
