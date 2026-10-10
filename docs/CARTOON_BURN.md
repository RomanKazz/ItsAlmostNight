# Cartoon enemy burning

`AppBurnDrawing.hpp` replaces the old additive triangle flames with animated
Bezier silhouettes. Four nested color layers form a red-orange rim, orange
body, yellow middle and cream core. Each tongue sways and stretches independently.
Rising rounded embers, scalloped smoke puffs and a restrained additive halo finish
the effect. The enemy receives a lighter warm tint so its original colors and
face remain readable.

All existing `EnemyBurn` presentation events use this renderer, including fire
wand hits and incendiary explosions. Damage, duration and status mechanics remain
in the simulation. Positions follow interpolated enemies and terrain height;
surface placement uses model bounds, with accessory extents clamped. Colors are
unlit so night lighting does not turn flames brown.

## Rendering and cost

- At most 72 visible burning enemies within 40 metres; nearest 24 get full detail.
- Detailed: five tongues, five embers and two three-lobed smoke clouds.
- Reduced detail / particles disabled: three tongues, two embers, no smoke.
- Three shared geometry batches: additive halo, alpha smoke, alpha flame.
- Fixed local geometry and sizing arrays; no textures, per-particle allocations
  or new persistent particle objects.
- Back-to-front enemy ordering; depth testing stays enabled, depth writes stay
  disabled during transparent passes and are restored afterward.
- Color-only world effects preserve the normal/AO attachment. Scene alpha masks
  rear geometry ink underneath foreground flames.

## Validation

`ian_graphics_smoke` checks warm color layers, animation, fading, reduced detail,
size scaling and foreground occlusion. Optional captures include real enemy
models in daylight and night lighting:

```sh
IAN_CAPTURE_BURN=/tmp/ian-burn-preview build/debug/ian_graphics_smoke
```

The change does not add simulation, save or gameplay interfaces.
