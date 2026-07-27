# Graphics Phase 1 — Pipeline Foundation

Date: 2026-07-26

## Implemented

- `GraphicsSettings` with feature flags, quality, shadow-map size, render scale,
  and shadow distance.
- A dedicated `ian_graphics` library.
- Non-copyable owners for raylib Shader, Texture, Model, RenderTexture, and rlgl
  framebuffer handles.
- Central `GraphicsResources` lifetime and resize management.
- A `Renderer` that controls world-pass loading, updating, drawing, presentation,
  and shutdown.
- A resizable off-screen scene target with direct-backbuffer fallback.
- Native-resolution UI composition after the world target is presented.
- Runtime graphics controls and a diagnostics panel.
- Particle rendering connected to `GraphicsSettings::particles`.

The simulation remains independent from raylib and continues to expose
render-only data through `SimulationSnapshot`.

## Lifetime

`App` creates `Renderer` only after `InitWindow()`. Before `CloseWindow()`, it
calls `Renderer::shutdown()` and destroys the renderer. GPU objects therefore
cannot outlive the graphics context.

All owning wrapper types delete copy and move operations. Releasing an already
empty resource is safe.

## Render flow

Gameplay:

1. Synchronize the scene target with framebuffer size and render scale.
2. Render the placeholder 3D world into the scene target when the pipeline is
   enabled and the target is valid.
3. Otherwise render the world directly to the backbuffer.
4. Present the vertically corrected scene texture to the full window.
5. Draw HUD and diagnostics at native window resolution.

Main menu uses a UI-only direct frame.

Resize is detected by comparing desired framebuffer dimensions every frame.
The old scene target is released before a replacement is loaded. A failed load
automatically selects direct rendering and does not block the game.

## Runtime controls

- `F2`: diagnostics panel.
- `F3`: shadows setting.
- `F4`: fog setting.
- `F5`: off-screen pipeline.
- `F6`: particles.
- `F7`: blob shadows setting.
- `F8`: bloom setting.
- `F9`: SSAO setting.
- `F10`: quality preset.
- `F11`: render scale `1.0 → 0.75 → 0.5`.

Settings for effects introduced by later phases are stored and displayed now;
they become visually active in their respective phases.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.

The application was not launched and no screenshots were taken, per the
project owner's integration-testing instructions.

## Manual integration checklist

1. Open the diagnostics panel with `F2`.
2. Resize the window and confirm `Scene target` follows the framebuffer.
3. Toggle `F5` and confirm world and HUD remain visible in both modes.
4. Cycle `F11` and confirm the world continues filling the window while the HUD
   remains sharp.
5. Trigger a hit or explosion, toggle `F6`, and confirm particles obey the
   setting.
6. Close the game normally and check for graphics-resource warnings.

## Performance impact

At default settings the world uses one full-resolution RenderTexture and one
fullscreen texture presentation per frame. Render scale can reduce world fill
cost. `F5` removes the off-screen pass entirely. No shadow, fog, bloom, or SSAO
passes are allocated in this phase.

## Known limitations

- Placeholder drawing still resides in `App::render()`; Phase 2 can move opaque
  world submission behind `Renderer` while introducing the world shader.
- The shadow framebuffer owner exists but no shadow framebuffer is allocated
  before Phase 3.
- Shader, Texture, and Model owners are ready, but no corresponding assets exist
  yet.
- Runtime appearance, FPS, and resize behavior await owner-run integration
  testing.

