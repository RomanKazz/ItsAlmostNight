# Core: eight authored visual levels

Authored with the reproducible bpy generator in the isolated `IAN_CORE_LEVELS`
scene; available in Blender MCP without replacing the user's open file.
Runtime uses `assets/models/buildings/core_levels.glb`. Original `core.glb`,
crystal meshes, crystal animations and gameplay parameters remain unchanged.

## Visual progression

1. Original Kenney pedestal/crystal identity.
2. Wider bevelled gold cradle, recessed orange reactor and four armored buttresses.
3. Orange insets seated in the gold armor; six alternating orbiting crystals.
4. Four substantial corner pylons with gold caps and orange reactor cartridges.
5. Stepped lower armor belt ties the pylons into the taller casing.
6. Raised corner pylons, four outward crown clamps and eight orbiting crystals.
7. Dark cooling cartridges with gold collars; counter-rotating upper crystals.
8. Double stepped gold crown with an orange separator and six upper crystals.

The upgrade adds structural silhouette instead of stretching the same casing.
No decorative casing gems, floating bolts or coplanar overlays. Pillars and
clamps sit below the lowest moving crystal across the complete orbit.
Round sidewalls have smooth radial normals; caps and armor edges remain hard.
The embedded original Kenney PNG is byte-identical. No baked lighting or new
materials. Component names are preserved as editable vertex groups.

## Runtime contract

- Authoring Blender Z-up; exported GLB Y-up. Symmetric, no weapon-facing axis.
- Neutral `core_asset_root`; meshes store authored coordinates with identity
  child transforms. Root floor is 0. The preserved original L1 extends down
  0.002238 authored units, as before.
- Runtime scale remains **1.76**. Visible diameter **1.76 m**, height about
  **1.44–2.17 m**. The build footprint/blocking are unchanged.
- Exactly **13 primitives**, **one textured material**. Mesh and node order
  both normalized, because raylib enumerates render primitives in node order.
- Mesh 0: original central crystal. Meshes 1–4: original satellites with node
  offsets baked. Meshes 5–12: complete pedestals L1–L8; select exactly one.
- `CoreCrystalModel.hpp::MeshCount` is now 13;
  `levelBounds(int level)` returns the selected level's animation envelope.
  All core passes use the same `forEachLevelPart` selection. Raycast uses that
  envelope; context/feedback height uses selected transformed meshes.
- Existing 0.28 rad/s orbit and six-second bob remain. Growth/lift matches the
  authored body. Small upper shards return at L7/L8, counter-rotating at -0.20 rad/s.
  Lower shards alternate full/72% sizes from L3, with small shards lifted 0.04 m.
  Gold accents sample the original palette at U=0.65625. No decorative casing
  crystals; fourfold symmetric structural upgrades.
- No new collision meshes, attack rig, sockets or gameplay stats. Core movement
  blocking uses existing game rules; hover uses the level envelope, not decorative
  geometry. HP, costs, level IDs, saves and upgrades retain existing behavior.
- 6/6/8/8/8/10/14/16 draws, 688–3208 triangles per visible level. No geometry,
  buffers or runtime containers created per frame.

## Files and regeneration

- `core_levels.blend`: editable source, packed original PNG; eight linked mesh
  previews plus named vertex groups for each pedestal component.
- `levels.png`, `front.png`, `side.png`, `back.png`, `top.png`, `perspective.png`:
  neutral studio views; `family_comparison.png`: L1/L8 at actual game scale beside
  base flamethrower/crossbow under identical light.
- `manifest.json`: mesh mapping, exact hashes, per-level bounds and parameters.
- `validation.json`: clean-import geometry/UV/normal checks and complete orbit sweep.

Run `tools/create_core_levels.py` through Blender MCP's `execute_blender_code`
with `__file__` set to its absolute path. It only replaces its own authoring scene
and output files. Or run it in an isolated background Blender for regeneration.
Then run `tools/validate_core_levels.py`. Finally run `tools/render_core_levels.py`
in a separate background Blender to save an ordinary source project and previews.
The generator's intermediate `.blend` is a scene library; the render script
normalizes the active scene and saves the final editable project.

Source geometry and original palette: Kenney tower-defense kit, CC0, see
`art/references/kenney_tower_defense/LICENSE.txt`. New pedestal geometry authored
for this project. Studio lights/AO are not baked into game textures.

## Verification

Clean import: all 13 meshes have finite unit normals, UV, closed manifold topology,
consistent outward winding. Explicit smooth-shoe/flat-cap normal checks pass.
Original file and palette hashes match. **2880 sampled orbit poses**, no satellite
contacts with body, central crystal or other satellites; envelopes contain motion.
Pedestal footprint, fourfold symmetry and central-crystal clearance are checked.
Game graphics smoke verifies real loading, selected level height/bounds, orbit,
distinct rendered levels and shutdown/reinitialize. Native manual playtest is
still separate from these automated shader captures.
