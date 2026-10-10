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

`Renderer::drawExperienceOctahedron` retains its existing calling convention.
Its historical name now draws the gemstone geometry.

Graphics smoke covers daytime/nighttime readability, attached highlights during
rotation, depth occlusion and invariance when world ink is enabled. To export
close-up and crowd previews:

```sh
IAN_CAPTURE_XP=/tmp/ian-xp/previews ctest --test-dir build/debug \
  -R '^ian_graphics_smoke$' --output-on-failure
```
