# Experience crystal presentation

XP pickups use a compact cyan gemstone instead of the previous four-sided
wire-like diamond. Broad coloured faces and clipped corners give it volume;
short mint tips and small opaque lozenge highlights make it readable at pickup
size. Highlights are part of the model and rotate with it.

The XP world material is 82% unlit with no screen AO or world ink outline.
It still writes normal opaque depth and marks itself as an ink occluder, so
foreground XP can cover contours behind it without receiving black lines.
Other pickups and world objects keep their existing materials.

Each pickup includes its own seeded rotation phase. Existing pop, gentle bob,
magnet attraction and near-camera shrink remain; terrain clearance accounts for
the taller tilted crystal. XP values, spawn counts and collection rules are
unchanged. Geometry uses stack constants and raylib's existing triangle batch;
there are no per-pickup textures, particle emitters or allocations.

Value tiers use the XP stored in the individual pickup, not the total enemy
reward: 1–3 XP stays turquoise, 4–7 XP becomes blue at 1.18× size, and 8+ XP
becomes gold at 1.35× size. The central `experienceCrystalStyle(value)` query is
shared by rendering and terrain clearance. Existing reward splitting and count
are unchanged, so ordinary drops remain a turquoise scatter with occasional
richer crystals. Overflow-merged pickups also get the appropriate style.

`Renderer::drawExperienceOctahedron` accepts an optional final `experienceValue`
argument (default 1); existing three-argument calls remain valid. The renderer
applies value scaling once. Its historical name now draws gemstone geometry.

Graphics smoke covers daytime/nighttime readability, attached highlights during
rotation, blue/gold palette and size tiers, depth occlusion and invariance when world ink is enabled. To export
close-up and crowd previews:

```sh
IAN_CAPTURE_XP=/tmp/ian-xp/previews ctest --test-dir build/debug \
  -R '^ian_graphics_smoke$' --output-on-failure
```
