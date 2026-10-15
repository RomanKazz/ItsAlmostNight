# Gun turret: gold armored upgrade tiers

Runtime integration replaces only the existing `upgrade_*` pairs inside
`assets/models/buildings/gun_turret.glb`. The original base, twin-barrel housing,
collider, muzzle nodes, texture and their transforms remain byte-identical.
The immutable stripped original is `base_reference.glb` for regeneration/audit.

## Visual progression

1. Original turret.
2. Gold tapered pedestal belt and bevelled side armor.
3. Four gold foot braces and orange armor insets.
4. Reinforced roof, inset top panel and dark octagonal bearing caps.
5. Two rear magazines with gold lids; upper pedestal reinforcement.
6. Continuous hollow steel barrel guards with gold muzzle lips.
7. Raised armored roof and rear bridge into the magazine assembly.
8. Gold breech collars, warm barrel bands and stronger foot armor.

The model uses the original embedded Kenney colormap, not vertex-color
approximations. Its gold/orange/steel palette matches core, Tesla and
flamethrower upgrades. There are no extra barrels or gameplay implications.

## Runtime contract

- GLB Y-up, -Z front, Blender Z-up/+Y front; unchanged source scale and origin.
- 17 raw primitives; original collider removed by ModelResource, leaving 16.
- Render indices 0/1 are original base/housing, followed by complete base/head
  pairs for L2–L8. Draw only the selected pair: **two additional draws**.
- Base decorations follow placement yaw. Head armor/magazines/guards follow
  aim yaw and recoil with the original housing. No pitch or new rig logic.
- GunTurretModel.hpp contains generated exact local overlay bounds. The public
  iteration/height interfaces are unchanged. Raycast/outline use these bounds.
- Original muzzle coordinates remain unchanged. Hollow decorative forward
  collars allow a clear path from either original socket to the front exit.
- Footprint, blocking, HP, damage, speed, projectile stats, saves and IDs unchanged.
- Maximum overlay budget: 1780 triangles at L8, merged into two meshes/material 0.
- No per-frame mesh creation, additional lights, particles or runtime containers.

## Editable source and previews

`gun_turret_levels.blend` opens in GUN_TURRET_LEVELS with the original PNG packed.
Named vertex groups retain editable components; `turret_yaw_control` moves the
housing and all head upgrades together. Show only one base/head pair at a time.
`level_*_beauty.png` and L8 front/side/back/top use common neutral studio lighting.
`levels_1-8.png` renders all tiers together under the same neutral light. `family_comparison.png` shows
L8 between the upgraded flamethrower and Tesla under the same light.

Regenerate in a separate Blender process:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/generate_gun_turret_upgrades.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/validate_gun_turret_levels.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/render_gun_turret_levels.py
```

## Validation

Validator verifies unchanged source bytes, nodes/collider/sockets, original PNG,
mesh order, UV, unit normals, welded manifold geometry and editable source.
Full yaw at 1 degree intervals with three recoil offsets: **7560 poses**, no
moving/fixed intersections. Both muzzle exit paths stay open.
Native graphics smoke separately checks all eight levels, state restoration,
rotated armor raycast, visible heights and hollow muzzle aiming. Manual gameplay
playtest remains separate from these automated shader captures.

Kenney source/palette CC0; license at
`art/references/kenney_tower_defense/LICENSE.txt`. New upgrade geometry is project-owned.
