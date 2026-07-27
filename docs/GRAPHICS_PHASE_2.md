# Graphics Phase 2 — Unified World Shader

Date: 2026-07-26

## Implemented

- One GLSL 330 shader pair for opaque world geometry.
- Vertex color multiplied by a configurable base color.
- World position and world normal varyings.
- raylib model and inverse-transpose normal matrices.
- Directional Lambert lighting with a mild stylized response curve.
- Sky/ground hemispheric ambient lighting.
- Baked-AO multiplier.
- Distance fog.
- Day/night tint.
- Hit-flash and selection tint.
- Shadow-map uniforms and sampling path prepared for Phase 3.
- Runtime shader fallback through `F12`.

Ground, obstacles, resources, buildings, projectiles, and enemies use the world
shader. Grid lines, wireframes, transparent building preview, debug geometry,
and particles stay on the default shader.

## Lighting state

`App` derives a presentation-only `WorldLighting` value from the current camera
and existing day/night transition. It does not modify simulation:

- warm daylight uses a descending directional light;
- night uses a weaker cool light from a different direction;
- ambient sky and ground colors transition independently;
- fog color follows the current clear color;
- night tint cools the final shaded result.

`Renderer` owns uniform locations and uploads lighting once per opaque pass.
Per-object material changes flush the raylib batch only when values actually
change.

## Material state

`WorldMaterialState` provides:

- base color;
- baked AO;
- hit-flash amount;
- selection amount;
- selection tint.

Existing aimed resources, buildings, and enemies use selection tint. Active hit
presentation events drive hit flash for nearby resources and enemies.

## Normal handling

The vertex shader declares raylib's standard `matModel` and `matNormal`
uniforms. `Renderer` explicitly registers both locations. `matNormal` is the
inverse-transpose transform supplied by raylib, preserving correct normals for
future non-uniformly scaled meshes and GLB models.

Current immediate-mode placeholders arrive through raylib's transformed batch;
their existing cube, sphere, cylinder, and plane geometry remains unchanged.

## Shadow preparation

The shader already accepts:

- `shadowMap`;
- `lightViewProjection`;
- `shadowsEnabled`;
- `shadowBias`.

Shadow sampling is forced off during Phase 2. No shadow map or shadow pass is
allocated yet.

## Runtime controls

- `F2`: graphics diagnostics.
- `F4`: distance fog.
- `F12`: unified world shader.

When the shader file cannot load, opaque drawing automatically uses raylib's
default shader. Diagnostics report `FALLBACK`.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.
- Installed raylib 6.0 shader, uniform, model-matrix, and normal-matrix APIs were
  verified at compile time.

The application was not launched and no screenshots were taken, per the
project owner's integration-testing instructions. No standalone GLSL validator
is installed, so final driver-side shader compilation is part of manual
integration testing.

## Manual integration checklist

1. Start a run and use `F12` to compare shaded and fallback rendering.
2. Confirm top-facing surfaces are brighter than downward/side-facing surfaces.
3. Aim at a resource, building, and enemy; confirm the selection tint appears.
4. Hit a resource or enemy; confirm a short warm hit flash.
5. Toggle `F4`; confirm distant fog appears and disappears.
6. Advance through sunset/night; confirm light direction, intensity, ambient,
   and tint transition without affecting gameplay timing.
7. Open `F2`; confirm `World shader` reports `READY`.

## Performance impact

The world remains one opaque pass. Lighting and fog are evaluated per fragment.
Uniform changes flush the raylib batch only for material-state transitions such
as selection and hit flash. Shadows remain disabled, so Phase 2 adds no shadow
pass or shadow texture.

## Known limitations

- Shader compilation and visual tuning require owner-run integration testing.
- Placeholder wireframes remain unlit by design.
- Shadow uniforms exist, but real visibility arrives in Phase 3.
- Baked AO defaults to a scalar because placeholder geometry has no AO vertex
  channel or texture.
- The current day/night values are presentation constants; the dedicated
  environment system belongs to Phase 5.

