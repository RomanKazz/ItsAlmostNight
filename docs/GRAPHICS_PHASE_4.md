# Graphics Phase 4 — Sky and Atmospheric Perspective

Date: 2026-07-26

## Implemented

- Procedural full-screen sky rendered before the 3D world.
- Independent zenith, horizon, and lower-sky colors.
- Smooth elevation-based vertical gradient.
- Stylized sun/moon disc with a restrained halo.
- Camera-relative sky projection using the current FOV and aspect ratio.
- Shared horizon and distance-fog color.
- Day, sunset, night, and dawn interpolation.
- Shorter night fog range for stronger atmospheric perspective.
- Runtime sky fallback through `Shift+F12`.

No sky mesh, cubemap, external texture, volumetric fog, or additional
dependency is used.

## Render flow

1. Clear the active scene target to the current horizon color.
2. Draw the procedural sky as one full-screen pass.
3. Render opaque world geometry and distance fog.
4. Render wireframes, transparent preview, debug geometry, and particles
   without a second fog application.
5. Present the scene target, then draw native-resolution UI.

If the sky shader is disabled or unavailable, the horizon clear color remains
as a safe fallback.

## Sky projection

The fragment shader reconstructs a view direction from:

- camera forward, right, and up vectors;
- vertical FOV;
- render-target aspect ratio;
- fragment position.

The result remains stable through window resize and render-scale changes. The
sky is translation-independent and responds only to camera orientation.

## Atmosphere profiles

Presentation values interpolate through three authored profiles:

- day: blue zenith and bright desaturated horizon;
- sunset: violet upper sky and warm orange horizon;
- night: deep blue zenith and cool dark horizon.

The existing game phase determines the interpolation amount. Simulation state
and timing remain unchanged.

The celestial direction also drives directional world light and the shadow
camera. The visible disc, illumination, and shadows therefore share one
direction.

## Fog

The world shader continues to use `smoothstep(fogStart, fogEnd, distance)`.
Ranges now adapt to night amount:

- day: 28–60 metres;
- night: 20–48 metres.

Fog color exactly matches the procedural horizon color. `F4` still disables
fog without disabling the sky.

## Runtime controls

- `F4`: distance fog.
- `Shift+F12`: procedural sky.
- `F2`: diagnostics and sky shader readiness.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.

The application was not launched and no screenshots were taken, per the
project owner's integration-testing instructions. Driver-side GLSL and visual
continuity remain part of manual integration testing.

## Manual integration checklist

1. Look from horizon to zenith; confirm a smooth gradient without bands.
2. Rotate the camera; confirm the celestial disc remains world-directional.
3. Resize the window and cycle `F11`; confirm the sky keeps correct proportions.
4. Toggle `Shift+F12`; confirm fallback clear color appears.
5. Toggle `F4`; confirm only world fog changes.
6. Inspect the map boundary; confirm terrain fades into the same horizon color.
7. Advance through sunset, night, and dawn; confirm colors and disc transition.
8. Confirm transparent preview and particles do not receive doubled fog.

## Performance impact

The sky adds one inexpensive full-screen fragment pass. It uses no texture
samples and allocates no additional render target. Distance fog was already
part of the world shader.

## Known limitations

- Visual palette and fog ranges need owner-run integration tuning.
- The disc follows an authored phase interpolation rather than astronomical
  time.
- No stars, clouds, weather, or volumetric atmosphere are implemented.
- The dedicated environment/time-of-day system belongs to Phase 5.

