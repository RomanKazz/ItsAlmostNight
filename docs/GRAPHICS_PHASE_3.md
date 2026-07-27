# Graphics Phase 3 — Directional Shadows

Date: 2026-07-26

## Implemented

- One directional shadow map.
- Dedicated caster/depth pass.
- Initial shadow-map resolution of 2048×2048.
- Orthographic light camera following the player area.
- Configurable constant and slope-scaled depth bias.
- Configurable shadow strength and distance.
- 3×3 percentage-closer filtering in the world shader.
- Distance culling for shadow casters.
- Runtime shadow disable through `F3`.
- Runtime depth-map preview through `F1`.
- Safe recreation when the quality preset changes shadow-map size.

## GPU resources

`ShadowMapResource` exclusively owns:

- framebuffer;
- color compatibility attachment;
- sampleable depth texture.

The color attachment keeps the framebuffer portable through raylib's public
rlgl abstraction. Only depth is sampled by the world shader. Framebuffer unload
releases its attached depth texture; the color attachment is released exactly
once by `ShadowMapResource`.

The resource is created after `InitWindow()`, recreated when
`shadowMapSize` changes, unloaded when shadows are disabled, and destroyed
before `CloseWindow()`.

## Shadow pass

The pass renders:

- ground;
- static obstacles;
- active resources;
- buildings;
- active enemies.

It excludes:

- projectiles;
- particles;
- UI;
- wireframes;
- debug geometry;
- transparent building preview;
- decorative core glow.

Casters outside `shadowDistance` from the player-centered shadow focus are
discarded before submitting geometry. The orthographic projection covers the
same bounded local region.

## Filtering and bias

Defaults:

- `shadowMapSize = 2048`;
- `shadowDistance = 80`;
- `constantBias = 0.00003`;
- `slopeBias = 0.0002`;
- `shadowStrength = 0.78`.

The receiver shader calculates slope bias from the surface/light angle and
samples a 3×3 texel neighborhood. Samples outside the light projection return
fully lit.

Shadow depth uses a dedicated texture unit outside raylib's transient material
batch slots, so per-object material flushes cannot silently unbind it.

## Runtime controls

- `F1`: show/hide shadow-map depth preview.
- `F2`: graphics diagnostics, including target validity and bias values.
- `F3`: enable/disable shadows and their GPU target.
- `F10`: cycle quality; changes shadow-map size and shadow distance.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.
- raylib 6.0 framebuffer, depth-texture, orthographic-camera, and shader APIs
  compile successfully.

The application was not launched and no screenshots were taken, per the
project owner's integration-testing instructions. Driver-side framebuffer and
GLSL validation therefore remains part of manual integration testing.

## Manual integration checklist

1. Start a run and open `F1`; confirm a depth image contains ground and casters.
2. Toggle `F3`; confirm shadows disappear and return without restarting.
3. Confirm walls, towers, resources, and enemies cast grounded shadows.
4. Inspect large cubes and the ground for acne.
5. Inspect thin walls and moving enemies for detached shadows.
6. Change sun direction through sunset/night and confirm shadows move with it.
7. Cycle `F10`; confirm the depth target size changes in `F2`.
8. Compare FPS with `F3` on and off during a large wave.

## Performance impact

Shadows add one bounded caster pass and nine depth samples per shaded fragment.
The pass performs distance culling and omits transient, transparent, UI, and
debug geometry. Disabling `F3` removes the pass, sampling, and shadow-map GPU
allocation.

## Known limitations

- Visual bias tuning and runtime performance need owner-run integration tests.
- One map covers the full configured local radius, so texel density decreases
  when `shadowDistance` grows.
- No cascaded shadow maps are used.
- The compatibility color attachment consumes GPU memory although only depth is
  sampled.
- Shadow focus follows the player without texel snapping, so slow camera
  movement may reveal shimmering; stabilization can be added after visual
  evaluation.
