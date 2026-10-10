# Cartoon freeze presentation

Freeze now uses a cyan translucent ice collar fitted to the enemy bounds,
chunky six-sided crystals, thick snowflakes and soft round cold mist. Faces,
weapons and silhouettes remain readable. The model tint follows the freeze
visual amount through thaw; the gameplay freeze/slow rules are unchanged.

Ice wand impact and crack effects share the same crystal geometry and palette.
Impacts scatter shards along short arcs with a brief snowflake flash. They no
longer use wire circles, line shards or generic destruction cubes.

## Rendering and cost

- Two batched geometry passes: ice, then atmosphere; alpha blend and depth test.
- Depth writes disabled only during a batch; state restored afterward.
- Color-only world attachment keeps transparent ice out of normal/AO data.
- Nearest 72 visible frozen enemies within 40 world units; nearest 24 receive
  snowflakes/mist when particles are enabled. Every frozen enemy retains tint.
- Up to 48 recent visible ice impacts; newest 16 receive mist.
- Fixed-size local arrays and bounded heap selection; no per-particle objects,
  allocations, textures or retained state. Enemy bounds computed only for the
  selected visual candidates. Alpha effects drawn back to front.
- Particles disabled preserves the collar, crystals and impact shards.
- Existing freeze status timer, thaw amount, event IDs and damage untouched.

## Verification

`ian_graphics_smoke` checks the cyan/white palette, animation, timer seam
continuity, thaw fade, low-detail visibility, occlusion, impact/crack differences
and complete impact disappearance. Set `IAN_CAPTURE_FREEZE` to a directory to
export diagnostic captures and day/night previews on three actual enemy models.
These generated previews are not runtime assets.
