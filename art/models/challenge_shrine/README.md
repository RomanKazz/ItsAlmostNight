# Challenge Shrine

Current replacement for `assets/models/challenges/skull_column.glb`. A tall
oak-and-stone altar with iron bands, a carved ivory skull, crossed blades,
red offering pool and paired ruby crystals. Matches the Player/Building shrine
family's original Kenney colormap and chunky proportions. Left/right and
front/back geometry are mirrored; no rear braces or one-sided loose parts.

The skull is original geometry. Its eyes and nose are boolean-cut sockets with
recessed dark bottoms on both faces. The sword blades meet behind the skull as
an intentional assembly; the emblem clears the supporting architecture.

- `challenge_shrine.blend`: normal editable Blender file; separate MODEL and
  STUDIO collections, packed palette, named construction vertex groups.
- `manifest.json`: dimensions in GLB coordinates, neutral root, counts,
  integration, palette identity and license.
- `validation.json`: clean import, closed outward solids, normals, UVs,
  footprint, emblem clearance and symmetry within 0.01 mm.
- `runtime-validation.json`: actual game `ModelResource` and `Renderer` probe.
- `preview.png`, front/back/side/top and `family-comparison.png`: neutral
  Blender views. `game-preview.png`: world-shader diagnostic render.
- `runtime-front.png`, `runtime-back.png`, `runtime-active.png`,
  `runtime-shrinking.png`: native rendering of both faces, active tint and
  completion scale.

Authored and runtime height: **3.25 m**. Bounds in GLB X/Y/Z:
`[-0.72, 0, -0.74]` through `[0.72, 3.25, 0.74]`. Fits the current challenge
outline box (`±0.75 m`, height `3.35 m`). One metre per Blender unit, floor
root at zero, Blender Z-up → GLB Y-up. Two meshes / two primitives, one
embedded palette material; **3,824 triangles** (2,592 body + 1,232 emblem).

No new physics/collision meshes or sockets: gameplay keeps its existing
challenge interaction sphere, arena collision, orientation, activation tint
and completion shrink. This asset does not alter challenge rules or terrain
alignment. Studio geometry and shadows are excluded from the runtime GLB.

Generator and checks:

```sh
blender --factory-startup --background --python-exit-code 1 --python tools/create_challenge_shrine.py
blender --factory-startup --background --python-exit-code 1 --python tools/render_challenge_shrine.py
blender --factory-startup --background --python-exit-code 1 --python tools/validate_challenge_shrine.py
```

The generator reuses only `Geo` and `bolt` authoring definitions from
`tools/refine_shrine_models.py`; it never runs the other shrine exports.
`tools/validate_challenge_shrine_loader.cpp` links against the game's existing
graphics libraries and checks loading, embedded texture, existing selection
bounds, front/back projections, active tint and completion scaling.

Original geometry with the existing Kenney CC0 palette. Old third-party skull
geometry/textures are not reused. Export, clean import and focused native
loader/render checks passed on macOS; manual gameplay and Windows testing are
not claimed. Preserve manual source edits before rerunning the generator.
