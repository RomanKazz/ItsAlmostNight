# Powerup pickups

`powerupDropChance(elite, boss)` centralizes the halved probability, preserving
deterministic roll seeds and equal type
selection: ordinary enemies **0.5%**, elites **2.5%**, bosses **10%**. The previous
values were 1% / 5% / 20%. Durations, buff strengths, collection rules, debug grants
and save IDs stay unchanged.

Three standalone GLBs replace the camera-facing icon cards:

| Type / stable index | Model | Silhouette |
| --- | --- | --- |
| Magnet / 0 | `assets/models/powerups/magnet.glb` | Cyan horseshoe, pale metal poles |
| Fury / 1 | `assets/models/powerups/fury.glb` | Thick violet bolt with pale inset |
| Gold Rush / 2 | `assets/models/powerups/gold-rush.glb` | Golden pouch with two star coins |

The shared 128×128 atlas is embedded in each GLB. Palette UV shading is baked
from smoothed corner normals; the props remain readable at night without the world
outline. Bevels and weighted normals preserve soft edges and hard planar faces.
Each runtime model has one mesh/material, limiting draw calls. Editable parts live
in `art/models/powerups.blend`; rendering previews in `art/models/powerup_views/`.

Rebuild with:

```
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python tools/create_powerup_models.py
```

The generator includes correct outward face winding, explicit UV assignment for
Blender primitives and a single merged export mesh. It produces the GLBs, palette,
editable source and triangle-count report. Do not overwrite manual .blend edits
without backing them up before rerunning.

`GraphicsResources::powerupPickupModel(index)` owns loading/unloading.
`Renderer::drawPowerupPickup(index, position, yawRadians, scale, tint)` fits the
model to 0.85 m, centered on the supplied position. It returns false for invalid
indices, invisible/nonpositive sizes or unavailable model assets. Solid geometry
writes depth and draws before the small additive glow and two sparse glints.
Rotation, gentle floating, birth scaling and expiry fading follow pickup age.
Reduce Flashes retains continuous motion with quieter glints.

Tests: deterministic drop distribution, pickup/save/reset/buff tests; GPU checks
for all three model loads, front/back readability, rotation and occlusion by a
solid blocker. `IAN_CAPTURE_POWERUP_MODELS=/path` writes GPU previews when running
`ian_graphics_smoke`. Full Debug and sanitizer gameplay/presentation suites also
run; pre-existing unrelated UI failures must be reported separately.
