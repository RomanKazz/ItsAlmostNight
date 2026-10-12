# Physical enemy cylinders

Crowd collision uses upright finite cylinders for every archetype, independently
of the forgiving attack/raycast capsule. Radius encloses the native model at
its runtime scale, including wide mushroom caps, Wizard's brim and dragon wings.
The final envelope also covers animated walking, biting and hit reactions;
GPU silhouette tests check all nine models at four points in each clip. Dimensions live in one
EnemyType-indexed table in `EnemyCollision.cpp`; elites multiply radius/height
by their existing 1.08 visual scale.

`enemyCollisionCylinder(enemy)` returns world center, radius and half height.
The base follows `worldSurfaceHeight` (terrain plus reached platform height),
not the old spawn Y or only a platform offset. Flying bodies use their actual
rendered hover envelope. Ground and flying enemies can share X/Z when their
vertical intervals do not intersect; a dragon still avoids a tall overlapping
mushroom.

`enemyCylinderPenetration(left, right)` returns exact horizontal overlap of
finite vertical cylinders, or zero when side surfaces/caps merely touch,
vertical intervals are disjoint, or dimensions/coordinates are invalid. It is
symmetric. The solver adds a separate 0.025 m contact gap and ignores residuals
below 0.001 m, keeping movement grounded rather than pushing enemies vertically.

## Solver corrections

- Coincident centers retain their true zero distance. The old fallback replaced
  it with 1 m, giving the wrong penetration (even negative for small supports).
- Zero-distance pairs choose a stable pair-dependent horizontal direction,
  without advancing gameplay RNG or collapsing every spawn pile into a row.
- Broadphase search covers actual radii, including large elites two cells apart.
- Portal groups use radius-aware exit formations. Three bodies form a triangle
  inside the aperture instead of overlapping in a straight line. Larger groups
  use a wider ring; spawn counts, timing and wave stats stay unchanged.
- Up to 12 passes at every crowd size, with early exit after contact convergence.
  Large waves previously dropped to 3 passes, leaving more visible overlaps.
- Structure exclusion uses the same radius, including elite growth. Existing
  navigation uses a compatibility capsule with that physical radius.
- Production calls retain link-vector storage; pair tests allocate nothing.

Crowd packing changes intentionally: wide models need more room in gaps.
An impossibly packed spawn/blocked enclosure can require several ticks to settle;
this is a bounded contact solver, not teleportation to arbitrary free positions.
Attack capsule/raycast rules and save IDs are unchanged.

## Authoring / checks

`tools/measure_enemy_cylinders.py` measures imported native geometry in Blender,
excluding importer-created bone-display meshes. The reviewed source dimensions
are in `docs/enemy_cylinder_model_bounds.json`; update the table when models or
render scales change.

Tests cover exact side/cap contacts, coincident pairs for all 9 types,
deterministic separation, elite size, different floors, wider broadphase,
400-enemy convergence and a small collider at a structure's exact center.

A local Release synthetic test (120 moving frames, no structures) measured:
400 enemies 0.17 ms mean; 800 enemies 0.42 ms; 1600 enemies 0.86 ms.
Final cylinder overlap was zero in each fixture. These isolate the crowd solver,
not total game frame time or worst-case blocked crowds.
