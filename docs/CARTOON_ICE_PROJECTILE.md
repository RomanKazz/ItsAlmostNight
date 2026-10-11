# Cartoon ice projectile

`graphics/IceWandEffect.hpp` replaces the old ice projectile's noisy additive sphere, orbiting wire circles, ribbon and particles with an opaque cartoon orb matching the fire projectile's layered silhouette:

- Blue outer rim, cyan face, milky white heart and a painted glint.
- Gentle size motion and drifting core, with an independent phase per projectile.
- Interpolated physical position and existing radius scaling.
- The app batches ice projectiles in one pass using simulation time.
- Unlit material keeps the cold palette legible at night. Solid layers write depth and occlude outlines from geometry behind them.

The held staff's charge effect, freeze impacts, status effect, projectile physics, damage and upgrades are unchanged. The existing Renderer projectile entry point shares the same geometry as the app's batch path.

GPU checks cover palette, motion, batch parity, foreground/rear depth, nighttime visibility and inactive projectiles. Use `IAN_CAPTURE_ICE_PROJECTILE` with `ian_graphics_smoke` for previews.
