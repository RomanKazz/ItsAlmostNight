# Flamethrower upgrade components

Base geometry and rig in `assets/models/buildings/flamethrower.glb` are preserved.
Silver accent UVs now use the core's gold swatch; the original PNG is unchanged.
Current recolored editable source/previews: `art/models/flamethrower_gold/`.
`flamethrower_upgrades.glb` supplies one cumulative three-mesh overlay for each
level 2–8: base, pitch, yaw. Only the chosen level is rendered: three additional
draws, one palette material. Level 1 uses the original model alone.

The exact original Kenney colormap PNG is embedded; rounded side walls are
smooth, caps and mechanical boundaries remain hard. Runtime axes: metres,
+Y up, −Z forward. Base pivot is the floor center; yaw pivot Y=.624;
pitch pivot Y=.924. Components in the runtime GLB are root-neutral; runtime
applies the existing pivots. The editable Blender file binds the same shells
to the imported original rig while retaining neutral world coordinates.
No additional collision meshes or sockets: the base model owns those.

Levels add straps, fuel clips, shoulder armor, heat guard, twin side fuel cells,
rear guard/pressure gauge, then the master collar/foot trim. Heights and exact
mesh indices are generated into `FlamethrowerUpgradeData.hpp` and `manifest.json`.

Rebuild in an isolated Blender process:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background --python-exit-code 1 --python tools/create_flamethrower_upgrades.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background --python-exit-code 1 --python tools/validate_flamethrower_upgrades.py
```

Validation: clean GLB reimport, finite unit normals, valid UVs, positive signed
volume, manifold geometry after temporary seam welding, palette byte equality,
base model preservation. 7,560 poses: all levels, yaw in 1° steps, pitch −10/0/21°;
zero contacts of moving upgrade components with fixed base geometry.
The generator preserves all visible source geometry. Runtime graphics smoke
loads all 21 meshes, renders all eight levels, checks their visible progression,
selects the raised pressure gauge by raycast and verifies the muzzle transform.
This is exported geometry and automated renderer validation; native combat
playtest has not been completed.
