# Cartoon bombs

The held bomb and thrown physical projectile share `graphics/BombDrawing.hpp`:

- Smooth radial normals on the round blue charcoal shell, collar and curved rope fuse.
- Two painted curved highlights, a gold collar and a cream/brown braided fuse.
- Continuous flame motion, arcing embers and three-lobed smoke puffs.
- Fuse length shortens continuously with its remaining lifetime; the shell warms subtly.
- World animation uses simulation time, so pausing stops it. Per-projectile phase offsets keep embers independent.
- Flame motion does not use brightness flashes. Disabling particles retains the small fuse flame while omitting embers and smoke.
- Opaque shells write depth and occlude rear world outlines. Fuse effects use the color-only world pass, test scene depth and do not write depth or normal/outline attachments.

Bomb physics, damage, fuse duration, inventory and the existing cartoon explosion cloud are unchanged. No asset dependencies or public simulation interfaces were added.

`tests/BombGraphicsChecks.hpp` checks day/night visibility, continuous motion, fuse progression, particle settings and foreground occlusion. Set `IAN_CAPTURE_BOMBS` to a directory when running `ian_graphics_smoke` to capture world and held-item previews.
