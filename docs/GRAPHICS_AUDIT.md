# Graphics Phase 0 — Audit and Baseline

Date: 2026-07-26

## Scope

This document records the graphics state before implementation of the new
pipeline described in `GRAPHICS.md`. Phase 0 intentionally changes no runtime
rendering.

The executable was not launched and screenshots were not taken, per the
project owner's testing instructions. Consequently, runtime FPS and actual GPU
draw-call counts are explicitly left unmeasured instead of being estimated as
facts.

## Environment and build

- Language: C++20.
- Build system: CMake 3.25+ with Ninja presets.
- raylib: 6.0 exactly. The installed package is used when available; CMake has a
  FetchContent fallback pinned to the same `6.0` tag.
- JSON: nlohmann_json 3.11.3.
- Window: resizable, VSync requested, initial client size 1280×720, target FPS
  144.
- Builds verified: Debug, Release, and Debug with Address/Undefined Behavior
  sanitizers.

The installed raylib 6.0 headers expose the APIs expected by later phases:
`GetRenderWidth`, `GetRenderHeight`, `LoadShader`, `LoadRenderTexture`,
`BeginTextureMode`, `IsRenderTextureValid`, and `DrawMeshInstanced`.

## Initialization, loop, and shutdown

- Entry point: `src/app/main.cpp`.
- Window initialization and main loop: `App::run()` in `src/app/App.cpp`.
- Frame order: input, update, render.
- Simulation advances through a fixed 60 Hz accumulator. Presentation effects
  use frame time and remain outside simulation state.
- Current shutdown only calls `CloseWindow()`. This is correct today because the
  application owns no Shader, Texture, RenderTexture, Model, or custom
  framebuffer resources.
- Future GPU resources must be released before `CloseWindow()`.

## Camera

- raylib `Camera3D`.
- Perspective projection, vertical FOV 75 degrees.
- Position follows the player snapshot.
- Target is derived from yaw and pitch.
- Presentation-only camera shake modifies the camera position.

## Current render path

Rendering is a single immediate-mode function, `App::render()`:

1. `BeginDrawing()`.
2. Resolve menu or gameplay state.
3. Calculate basic day/sunset/night colors.
4. Clear the backbuffer.
5. `BeginMode3D(camera)`.
6. Draw world primitives, preview, debug geometry, and presentation effects.
7. `EndMode3D()`.
8. Draw damage overlay, HUD, menus, and debug text.
9. `EndDrawing()`.

There is no off-screen scene pass, render graph, world shader, lighting pass,
shadow pass, fog, post-processing, or render-scale path.

## Current scene representation

- Ground: one plane plus a one-metre debug grid.
- Obstacles: map-defined boxes, currently 2.
- Resources: map-defined primitives, currently 4 trees and 3 stones.
- Buildings: primitive combinations selected by building type. Walls may submit
  up to five cube sections plus matching wireframes.
- Enemies: one cube and one sphere each, with an optional wireframe.
- Projectiles: spheres.
- Building preview: translucent cube plus wireframe.
- Effects: spheres/wire spheres or six small cubes.
- Player: first-person camera only; no visible body, hands, or weapon model.
- UI: raylib text and 2D rectangles drawn directly to the backbuffer.

Simulation exposes read-only spans and plain game data through
`SimulationSnapshot`; raylib types do not leak into simulation. This is a good
boundary for extracting rendering from `App`.

## Assets and GPU resources

- `assets/models`, `assets/textures`, `assets/shaders`, and `assets/audio`
  contain no runtime assets.
- No shader files exist.
- No Shader, Texture, RenderTexture, Model, or custom framebuffer is loaded.
- No corresponding unload path exists because none is needed yet.

## Scale baseline

Configured upper bounds:

- Buildings: 581 total if every per-type limit were reached (1 core, 256 walls,
  64 turrets, 4 mines, 64 cannons, 64 traps, 128 gates).
- Enemies: 200 pooled slots.
- Cannon projectiles: 300 pooled slots.
- Bomb projectiles: 32 pooled slots.
- Presentation effects: 128 retained effects.
- Map resources: 7 nodes.

These are capacity limits, not a claim that all objects can be active and
visible simultaneously in normal play.

The current renderer performs no frustum or distance culling. CPU-side
`Draw*()` submissions scale approximately linearly with visible simulation
items. Primitive buildings produce roughly 2–10 submissions each, enemies 2–3
each, and debris/resource effects up to 6 each. raylib may batch compatible
submissions, so these figures are not actual GPU draw-call counts.

Runtime baseline:

- Average FPS: not measured; launching the application is delegated to the
  project owner.
- Actual draw calls: not measured; no counter or graphics debugger is currently
  integrated.
- Initial resolution: 1280×720; window is resizable.

## Visual and architectural issues

1. `App::render()` owns all world, effects, debug, and UI rendering and is the
   primary integration bottleneck.
2. Flat primitive colors provide no directional light, material response,
   normal-based volume, or contact with the ground.
3. Day/night currently changes only clear and ground colors, plus a small core
   sphere at night.
4. No shadows, fog, atmospheric perspective, sky geometry, bloom, or
   post-processing exist.
5. No render target exists, so render scale and post-processing cannot be added
   cleanly yet.
6. Resize works only because rendering targets the window directly. Future
   render targets need explicit recreation.
7. Effects and camera shake lack centralized graphics settings.
8. There is no graphics diagnostics panel, runtime graphics toggle set, draw
   submission counter, or culling.
9. Placeholder drawing is type-specific and embedded in `App`, which will make
   later GLB substitution unnecessarily difficult unless it is extracted.

## Phase 1 integration plan

Use the existing simulation boundary and incrementally extract presentation:

1. Add a `GraphicsSettings` value type with the required feature toggles,
   shadow-map size, render scale, and shadow distance.
2. Add a non-copyable graphics resource owner. It will initialize after
   `InitWindow()`, expose explicit load/resize/unload operations, and release all
   GPU resources before `CloseWindow()`.
3. Add a `Renderer` owned by `App`. It will accept `SimulationSnapshot`, camera,
   presentation effects, and debug flags; simulation remains raylib-independent.
4. Move the existing placeholder world drawing into the renderer without
   changing appearance or mechanics. Keep HUD/input orchestration in `App`
   initially.
5. Add a scene `RenderTexture2D`, recreated only when framebuffer dimensions or
   render scale change. Use render dimensions rather than logical window size
   for high-DPI correctness.
6. Keep direct-backbuffer rendering as a safe fallback when the scene target is
   invalid or post-processing is disabled.
7. Introduce move-only wrappers or explicit single ownership for Shader,
   Texture, RenderTexture, Model, and shadow framebuffer handles; never copy
   owning raylib structs.
8. Add a compact graphics debug panel showing framebuffer size, render scale,
   feature toggles, resource validity, and CPU-side render submission counts.
9. Keep the system package requirement and FetchContent fallback pinned to the
   same raylib release when upgrading the dependency.

Phase 1 must preserve the current simulation, controls, placeholder geometry,
and fixed-step behavior.

## Phase 0 acceptance

- Initialization, loop, camera, drawing paths, assets, and resource ownership
  are identified.
- Installed raylib API availability is checked.
- Object capacities and missing runtime measurements are recorded.
- Debug, Release, and sanitizer builds pass.
- Unit tests pass in all three build configurations.
- Concrete Phase 1 integration points are documented.
