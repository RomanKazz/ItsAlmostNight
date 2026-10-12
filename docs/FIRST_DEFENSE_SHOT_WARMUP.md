# First defense shot: pipeline warmup

The old startup pass warmed only the scene-color and scene-color/normal layouts.
Transparent trails and muzzle flashes now use sparse three-output routing
(color / none / effect coverage); opaque effect occluders can use all three
attachments. Those driver variants were missing from warmup.

`Renderer::warmCombatRendering()` now runs beside the actual framebuffer routing
helper, and exercises all valid layouts during initialization:

- color only;
- color + normals;
- color + effect coverage, leaving normals intact;
- color + normals + effect coverage.

It warms alpha/additive blending, depth-write enabled/disabled, and both ink-occluder
states. The effect-coverage target is prepared during loading, not lazily when the
first selected object is covered by a shot. World Shader disabled still gets a
basic default-shader warmup. macOS finishes the submitted GPU work during loading.
No shots, effects, sounds or gameplay events are generated.

`firstCombatDraw()` in the renderer smoke test checks the real sparse/full routing
on the first world frames and prints GPU-complete first/repeated draw timings.
Measured here: roughly 0.98 / 0.89 / 0.63 ms in Debug. This measures the rendering
path, not the whole user run; it is not proof that all reported hitches are gone.

Performance CSV appends `defense_shot_events` (TowerFired + CannonFired events from
this frame's simulation ticks) and `active_defense_projectiles` (live tower and
ballistic projectiles). Existing columns retain their order. First-shot frames can
now be compared with update/render/present timings and window-focus markers.

Validation: renderer/performance GPU smoke tests, full gameplay and presentation
suites, and a CSV schema/value check. For a remaining gameplay hitch, record a run
from before the first defense shot; correlate the new counters with the timing
columns rather than assuming the cause is always shader compilation.
