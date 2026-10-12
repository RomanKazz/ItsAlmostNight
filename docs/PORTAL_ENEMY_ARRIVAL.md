# Portal enemy emergence

Wave routing now supplies the actual `arrivalPortal` and `portalExitYaw` in each
EnemySpawn, including redirected surge groups and the late boss. Spawned actors
receive a 0.70 s `portalArrivalRemaining`. The timer advances with simulation
seconds, pauses with the game and resets on reused enemy slots. Splitlings,
debug enemies and royal summons retain their own existing arrival behaviour.
There is no new spawn queue, invulnerability or save ID. Ordinary enemies commit
to a forward exit for 0.70 s before their usual AI; they remain physically
targetable. Boss arrival keeps its existing crown-shield sequence.

The gate aperture opens over 0.65 s at night start. Each arriving group makes
its vortex contract and rebound, with a front-facing burst of curved violet
ribbons, layered cloud lobes and cream/cyan stars. Multiple enemies from one
gate share one pulse instead of stacking identical portal effects. Final night
keeps its aperture open even though its phase countdown does not advance.

Actors have a short 0.65 m visual forward slip, foot-pivot squash/stretch,
small hop and damped roll. The same pose is used by the visible model and
selection mask. Distant LOD is deferred during emergence. Spawn formations translate intact to the front side, retaining pair spacing.
Cylinders, HP, group counts and wave timing are unchanged.

Rear visibility is explicitly controlled by the shared
`presentation::portalArrivalVisible(enemy, observer)` predicate:

- Actors behind their gate's forward plane remain hidden until crossing it.
- Rear observers cannot see a birth animation, exit cloud or portal emission.
- Elite outlines/auras, linked wardens, status effects and world health bars use
  the same rule, so they cannot reveal a hidden birth.
- A completed forward exit is visible normally. Expired actor metadata clears
  unconditionally, preventing blocked geometry or later movement from hiding
  an old enemy indefinitely.

The portal's vortex remains visible from both sides. The physical spawn
formation still uses its radius-aware spacing, avoiding overlap and preserving
wave counts. The shared emergence rule is presentational, not a new obstacle.

All effect geometry uses the existing unlit, depth-tested color-only triangle
batch. No new atlas, shader, runtime effect container or per-spawn sound is
created. Trees/buildings occlude the energy. Low particles / reduced flashes
use fewer cloud lobes and omit ribbons/stars; there is no screen flash.

CPU tests verify tagged vs ordinary spawning, timer lifetime, unchanged physical
radius, reused actor cleanup, reset, front/rear visibility and wave-origin tags.
GPU checks verify aperture stages, animated gate/exit energy, expiry, night
readability, obstacle occlusion, low detail and a settled normal actor pose.
`IAN_CAPTURE_PORTAL_ARRIVAL=/tmp/portal-preview` exports diagnostic frames from
the graphics smoke test; captures are not game assets.

Public additions: EnemySpawn/EnemyInstance portal metadata;
EnemyPortalArrivalDuration; the presentation visibility predicate;
optional Renderer::drawEnemyPortal arrival/opening/detail arguments. Existing
callers keep a fully open idle portal through the default arguments.
