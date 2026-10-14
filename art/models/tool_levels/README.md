# Axe and pickaxe: eight visual upgrade levels

Both tools now change appearance with their existing weapon upgrade cards, up to
level 8 and the current core-level limit. This pass changes presentation only.
Harvest power, attack speed, save IDs, hand position and both authored swing clips
retain their existing behavior.

## Assets and editing

- Runtime: `assets/models/tools/axe_levels.glb`, `pickaxe_levels.glb`.
- Original L1: `assets/models/tools/axe.glb`, `pickaxe.glb`, unchanged.
- Editable source: `tool_levels.blend`; `TOOLS_MODEL` contains both complete tools,
  named level overlays and material vertex groups. `TOOLS_STUDIO` contains only
  the neutral preview camera/lights. No new colliders or attack sockets required.
- `all_levels.png`: L1–L8 from left to right, axe above pickaxe.
- `comparison.png`: L1 / L4 / L8 of both tools, same light and camera scale.
- `family.png`: L8 tools alongside the actual cannon, core and flamethrower.
- Individual tier PNGs plus front / side / back / top acceptance views.
- `manifest.json`: source hashes, original hierarchy, local bounds and sizes,
  level-to-mesh mapping and triangle counts. Sizes are tool mesh local units;
  the original pickaxe child scale and swing-pivot transform still apply.

| Level | Axe | Pickaxe |
|---|---|---|
| 1 | Original steel head and wooden shaft | Original curved steel head and wooden shaft |
| 2 | Gold socket and top rivet | Gold socket and top rivet |
| 3 | Gold-edged orange cheek armor | Gold-edged orange center armor |
| 4 | Wider steel cutting edge, gold bolt | Longer steel tips, gold center bolt |
| 5 | Orange rear armor, extra shaft collar | Reinforced orange neck and extra shaft collar |
| 6 | Broader blade armor and stronger socket | Gold-edged orange shoulder armor |
| 7 | Red panels and gold rank bars | Red shoulders and gold rank bars |
| 8 | Heavy gold back plate, crown and second bolt | Heavy gold crown and broad shoulder rims |

The original KayKit tool atlas is embedded unchanged. Added parts use the exact
embedded Kenney defense colormap, also unchanged. Gold UV is U=0.65625,
Blender V=0.285–0.465, matching the core/cannon/flamethrower. Steel uses a separate
light-gray swatch; orange/red armor follows the same defense palette.

KayKit RPG Tools Bits and Kenney Tower Defense Kit are CC0. Original notices:
`assets/models/tools/LICENSE-KAYKIT.txt` and
`art/references/kenney_tower_defense/LICENSE.txt`.

## Runtime contract and cost

GLBs contain the original mesh plus seven cumulative overlays. Only the chosen
level overlay renders, never all seven. L1 uses the original asset; L2–L8 use two
draws. L8 totals: axe 1,516 triangles; pickaxe 1,452 triangles. Both assets load
at graphics initialization, with no model/texture loading on upgrade or swing.

Each pickaxe overlay inherits the exact original child scale and swing parent;
raylib bakes the same hierarchy into all meshes. The renderer applies the existing
rigid animation to both base and overlay. Composite bounds include only those two
meshes so inactive levels cannot enlarge the first-person render crop.

Public API additions: optional `int toolLevel = 1` at the end of
`Renderer::drawFirstPersonTool`; `GraphicsResources::axeLevelModel()` and
`pickaxeLevelModel()`. The app reads the level for the **displayed** tool, retaining
correct appearance during equipment swaps.

## Reproduce and verify

```sh
python3 tools/generate_tool_upgrades.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/render_tool_levels.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/validate_tool_levels.py
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`validate_tool_levels.py` verifies unchanged L1 bytes, atlas/animation/hierarchy,
self-contained GLBs, finite UV/normals, positive-volume manifold overlays after
seam welding, fresh Blender import and packed editable source. Its machine-readable
results are in `validation.json`.

`ToolLevelGraphicsChecks.hpp` exercises the real raylib loader and renderer:
all tiers must differ visibly; both swing clips must move every upgraded tier;
levels clamp to 8; switching back to L1 must restore its exact image. Captures:
`IAN_CAPTURE_TOOL_LEVELS=/tmp/tool-levels ctest --preset debug -R ian_graphics_smoke`.
The existing swing-trail attachment checks continue to pass.

Verified: complete Debug suite (23/23), subsequent graphics run at 16:10,
ASan/UBSan weapon progression and the complete graphics executable, Release build
and weapon progression. The first sanitizer CTest graphics invocation reached its
60-second timeout during concurrent background work; its separate executable
rerun completed successfully without sanitizer findings. This is automated native
render verification, not a manual gameplay playtest.
