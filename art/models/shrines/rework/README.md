# Woodland shrines — current runtime models

The Player Shrine is a split stone arch with a ruby heart, amethyst votive
tabs and crystals. The Building Shrine is an iron-banded oak portal with a
bronze gear and sapphire hub. Both use the original Kenney
colormap, the world's wood/stone/metal vocabulary and readable chunky forms.

The existing runtime GLBs in `assets/models/world_landmarks/` are replaced.
The older column design's editable source is retained one directory above.
No gameplay, shrine costs, rendering code or collision contract changed.

- `shrines.blend`: ordinary editable Blender file, active authoring scene,
  separate MODEL/STUDIO collections, packed original palette. The 53/59
  construction components remain selectable as named vertex groups.
- `manifest.json`: GLB coordinates, dimensions, mesh counts, palette and contract.
- `validation.json`: clean reimport, closed outward solids, finite UVs/unit
  normals, exact embedded palette, zero floor, footprint and emblem clearance.
- `runtime-validation.json`: focused real loader/render/raycast checks.
- `preview.png`, individual detail views, front/side/back/top,
  `family-comparison.png` and `before-after.png`: neutral Blender renders.
- `game-preview.png` / `game-back.png`: front/back world-shader renders; a flat diagnostic scene,
  not a screenshot of a manual gameplay session.
- `runtime-active.png` / `runtime-spent.png`: current loader's active/used tint.

Both models are authored at 2.8 m height, neutral floor root and GLB Y-up.
There are two mesh primitives and one packed palette material per shrine.
The original 2×2×2.8 m simulation blocker, precise mesh hover and surface-normal
alignment still apply. Player: 2,732 triangles; Building: 3,320 triangles.
Both assets mirror exactly across the left/right and front/back planes.
Steps, badges, fasteners and crystals are paired. There are no rear braces,
off-centre anvil or loose moss pads. Symbols float in the central plane and
clear the surrounding architecture. The validator checks reflected exported
vertices separately for the body and emblem, within 0.01 mm.
Studio shadows and lighting are not baked into runtime assets.

Rebuild in Blender (MCP can execute the same script in its isolated scene):

```sh
blender --factory-startup --background --python-exit-code 1 --python tools/refine_shrine_models.py
blender --factory-startup --background --python-exit-code 1 --python tools/render_refined_shrines.py
blender --factory-startup --background --python-exit-code 1 --python tools/validate_refined_shrines.py
```

Rebuilding replaces only these shrine runtime assets and this generator's
authoring scene/output. Preserve manual edits before rerunning the generator.
Geometry is original; the existing Kenney palette is CC0. The shipped models
passed automated loader/render/raycast checks on macOS; no Windows build or
native manual gameplay playtest is claimed.
