# Fire wand attack presentation

Fire uses a dedicated cartoon comet instead of recoloring the ice magic sphere.
The projectile has an orange round silhouette, a gold interior, a cream hot core,
three animated flame lobes, a layered tapering wake and optional small embers.
The wake follows the recorded projectile path and interpolates every trail node.
A fresh/reused projectile uses only its current trail count.

The staff carries a small idle flame. Charging grows the core and draws embers
inward; release gives a short larger flash driven by the existing recoil timer.
Idle charge progress is explicitly zero for fire when no cast is charging.
Impacts scatter curling flame lobes with a hot core and soft warm glow, then fade
out. Existing cartoon explosions remain the bomb/cannon presentation.

## State and performance

`graphics/FireWandEffect.hpp` owns only procedural geometry. No textures,
allocations, random state, extra projectiles or lifetime bookkeeping. The app
batches all fire trails together, then all solid heads. Alpha wakes test world
depth without writing it; solid heads write depth and mask background ink.
Transparent passes preserve normal/AO attachments. Impact rendering is bounded
at 32 recent visible events within 40 world units. Extra embers respect the
particles setting. The existing `Renderer::drawIceWandProjectile` entry point
also dispatches fire to this appearance for other callers.

Damage, charge/cooldown timing, speed, collision radius, burn rules, pooling,
save IDs and audio remain governed by the existing elemental weapon system.
There are no new public Renderer signatures.

`ian_graphics_smoke` checks warm palette layers, movement, low detail,
foreground occlusion, head depth writes, charge/release size, inactive pool
slots and impact expiry. Set `IAN_CAPTURE_FIRE_WAND` to export previews.
