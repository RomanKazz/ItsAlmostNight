# Crossbow: gold armored upgrade tiers

The runtime GLB at `assets/models/weapons/crossbow.glb` now uses textured
upgrade pairs matching the core, gun turret, flamethrower and Tesla family.
Original geometry, rig, collider, muzzle nodes and embedded palette bytes remain
unchanged. `base_reference.glb` is the stripped original retained for regeneration.

## Visual progression

1. Original Kenney ballista.
2. Gold pedestal belt and rear stock binding.
3. Gold carriage side armor with visible orange inset panels.
4. Thin gold reinforcement along the front of the curved bow.
5. Four separate limb ferrules, leaving the wooden span visible.
6. Stock-mounted winch with gold shoulders/dark hubs; second pedestal trim.
7. Broad upper gold bow rail; original wood remains between the rails.
8. Orange front inlay, additional foot trim and open gold carriage frames.

The L8 outer side detail is a true closed frame mesh with a through-cut center,
not another filled plate. A regression ray/UV check from both sides verifies
that the orange inset is the first visible face through its center.
The launch channel remains unobstructed by new reinforcement.

## Runtime contract

- GLB Y-up/-Z front; Blender Z-up/+Y front. Existing model scale/origin retained.
- 17 raw primitives including the original collider; 16 after ModelResource removes it.
- Render indices 0/1: original pitched bow/yaw carriage. Remaining pairs are
  complete `base`/`bow` overlays for levels 2–8. Select only the current pair.
- **Two extra draws**, one shared original colormap material; no vertex-color
  approximations or additional textures, shader effects, lights or particles.
- Carriage overlays follow yaw. Bow/stock/winch overlays follow the original
  pitch pivot at GLB `(0, 0.149311006, 0)`, then yaw. No new animation system.
- CrossbowModel.hpp keeps the same public interface and generates exact bounds
  for hover/raycast/outline. Damage, firing pattern, footprint, IDs and saves retain
  their existing behavior. Placement footprint remains 2 × 2 m.
- Visible/collision bounds, per-level overlay bounds and triangle counts are in
  manifest.json. Maximum additional geometry: **1080 triangles** at level 8.
- No per-frame geometry allocation or new runtime containers.

## Editable source and regeneration

`crossbow_levels.blend` opens in CROSSBOW_LEVELS. The original PNG is packed;
components have named vertex groups. `pitch_pivot` controls the bow/stock and
selected bow overlay; `yaw_pivot.001` controls the whole ballista assembly.
Show only one base/bow pair at a time. Studio and model collections are separate.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/generate_crossbow_upgrades.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/validate_crossbow_levels.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/render_crossbow_levels.py
```

Previews include individual tiers, L8 front/side/back/top, `levels_1-8.png` under
common lighting, and `family_comparison.png` beside flamethrower/Tesla level 8.

## Verification and limits

Validator checks source byte preservation, original palette, node/mesh order,
closed manifold/outward geometry, finite unit normals, UV, packed editable source,
open launch channel and both orange side insets. **9086 sampled poses** cover a
complete yaw and a pitch sweep from -90 to +90 degrees for new bow and carriage
armor. The common -18 to +18 degree range additionally checks floor clearance
and every new fixed part against the original moving bow.

The original low pitch pivot is retained. At extreme pitch the original stock
can intersect the original pedestal; attached surface belts share that existing
contact. Full-range checks therefore distinguish carriage armor from pedestal
surface bands. This task does not change the original mechanical rig or aim rules.
Native graphics smoke separately verifies all eight rendered tiers, pitch/yaw
raycast, material restoration and model reinitialization. Automated shader captures
are not a manual combat playtest.

Kenney source/palette CC0: `art/references/kenney_tower_defense/LICENSE.txt`.
New upgrade geometry is project-owned.
