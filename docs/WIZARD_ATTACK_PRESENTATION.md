# Wizard and cartoon enemy magic

The Ranged archetype now uses `assets/models/enemies/ultimate/wizard.gltf`,
imported unchanged from `/Users/roman/Downloads/Blob/glTF/Wizard.gltf`.
The embedded atlas, buffers and native animations are self-contained.
The model belongs to the Quaternius Ultimate Monsters pack (CC0; see the
existing `assets/models/enemies/ultimate/LICENSE.txt`).

Native `Walk`, `Idle`, `Bite_Front` and `HitRecieve` clips replace the former
humanoid animation remapping. Individual rendering, shadows, selection and
GPU crowd animation share the same source. The distant crowd color is violet.

## Attack presentation

A violet comet has a plum silhouette, lilac inner volume, warm white core,
small gold sparkles and an animated tapered wake. Solid heads write depth;
transparent wakes retain depth testing without writing it. Both bypass world
ink outlines and use unlit colors. The solid head and impact cloud also mark
selection-effect coverage, preventing selection outlines showing through them.

Casting produces a quick puff/flash slightly ahead of the face, so the
body does not hide it (visual anchor only; launch collision position unchanged); impact produces expanding scalloped
cloud lobes and drifting four-point stars. Effects are procedural triangles,
with no runtime texture loading or per-particle allocation. Particle settings
reduce wake detail and remove sparkles; the projectile remains visible.

`EnemySystem::magicEvents()` exposes a borrowed per-tick span of cast/impact
notifications, backed by retained reserved storage. Simulation converts these
to the appended `EnemyMagicFired` / `EnemyMagicImpact` GameEvent IDs. Physical
player/structure hits and terrain contact produce one impact; lifetime expiry
produces a smaller dissipation. Target deletion simply removes the projectile.
The queue clears each tick, on reset and when projectiles are cleared.

Damage, attack interval, 7.2 m/s travel, radius, aim prediction and physical
misses are unchanged. `EnemyProjectile::previousPosition` smooths visual motion
without changing collision calculations. Birth wake length follows actual
travel age rather than appearing at full length on the launch frame.

## Checks

Enemy tests cover cast/hit timing, one-shot notifications, phase cleanup and
reset. Graphics smoke tests load/animate Wizard and check effect motion,
night color stability, obstacle occlusion, inactive suppression and low detail.
`IAN_CAPTURE_WIZARD=/directory` saves optional model/effect previews.
