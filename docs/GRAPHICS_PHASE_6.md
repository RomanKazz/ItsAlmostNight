# Graphics Phase 6 — Ground and Environment Variety

Date: 2026-07-26

## Implemented

- Dedicated terrain branch in the shared world shader.
- Three controlled terrain tints: grass, dry ground, and stone/soil.
- Texture-free world-space value noise.
- Two calm macro scales for broad color variation.
- Sparse low-frequency soil/stone patches.
- Height-driven dry-ground contribution.
- Slope-driven exposed-stone contribution.
- Material flag restricting terrain variation to ground geometry.
- Existing day/night ground tint, lighting, shadows, and fog remain active.

## Terrain material

`WorldMaterialState` now supplies:

- terrain amount;
- primary terrain tint;
- secondary terrain tint;
- patch tint.

The ground enables terrain amount before its draw call and immediately restores
the default material. Buildings, resources, enemies, effects, and previews do
not inherit terrain noise.

Disabling the world shader with `F12` keeps the previous solid-color fallback.

## Noise design

The shader uses smooth value noise generated from world XZ coordinates:

- broad variation at roughly 18-metre scale;
- secondary variation at roughly 7-metre scale;
- rare patches at roughly 26-metre scale.

No texture is sampled. No small high-contrast detail is generated. Variation
therefore stays stable in world space, avoids visible texture tiling, and is
less vulnerable to video-compression shimmer.

Height and surface slope are already included for future non-flat terrain.
Current graybox ground is flat, so visible variation primarily comes from the
two macro noise layers and sparse patches.

## Instancing decision

Instancing was not added in this phase. Current graybox props are heterogeneous
raylib primitives with per-object active, selection, hit-flash, and gameplay
state. There is no persistent static-prop mesh batch yet. Adding an instanced
path now would duplicate drawing architecture without measured benefit.

Instancing should be introduced with repeated GLB static props, when shared
meshes/materials and stable transform batches exist.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.
- Source whitespace check: passed.

The application was not launched and no screenshots were taken. No standalone
GLSL validator is installed in the environment. Shader compilation and visual
tuning remain part of owner-run integration testing.

## Manual integration checklist

1. Inspect open ground; confirm broad, soft color regions replace a solid plane.
2. Walk across the map; confirm variation remains fixed in world space.
3. Confirm no small repeating or shimmering pattern appears.
4. Confirm rare soil/stone patches remain subtle.
5. Confirm resources, enemies, and building silhouettes remain readable.
6. Advance through sunset and night; confirm variation does not become noisy or
   crush into black.
7. Toggle `F12`; confirm solid-color fallback still renders.
8. Toggle fog and shadows; confirm terrain remains compatible.

## Known limitations

- Current flat graybox cannot visibly demonstrate height or slope blending.
- Tint strengths need owner-run visual tuning.
- Terrain geometry displacement, splat textures, grass, decals, and static-prop
  instancing remain future work.
- Contact grounding and blob shadows belong to Phase 7.
