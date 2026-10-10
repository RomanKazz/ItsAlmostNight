# Fire wand presentation

The projectile keeps the original layered cartoon core: an orange/red edge,
gold interior, cream hot center and its original restrained pulse. Its palette,
size, highlight offset and per-projectile phase are unchanged. It interpolates
its real position and writes world depth/ink occlusion. There is no wake, halo, flying flame petal or ember around
the projectile. The existing elemental-projectile entry point and the app pool
batch use the same core renderer.

Fire impacts once again use the existing `emitCartoonExplosion` path: a hot
flash followed by rounded expanding smoke puffs, dust and embers. The same
bounded 128-event explosion batch handles fire, bombs and artillery. Extra
detail remains limited to the four newest visible blasts and respects particles.
The fire-specific radial petals have been removed.

The staff retains its small idle flame, charge fire and recoil flash. Enemy
burning retains its existing animated flames. Gameplay damage, charge timing,
velocity, physical radius, burn rules, pooling, save IDs and audio are unchanged.

## Selection occlusion

While selection is active, world effects write coverage and coverage-weighted
inverse depth into a separate color attachment. Sparse OpenGL 3.3 MRT routing
preserves normals/AO for transparent color-only effects. Explosion clouds keep
their authored ink-occluder normal output, also recording selection coverage.
The mask is cleared each selected frame, resized with the scene and unloaded
at Renderer shutdown. The white selection contour fades under foreground
coverage and remains visible if an effect is behind the selected object. Sky
alpha is not used as effect coverage. Tactical sectors retain color-only routing.

Graphics regressions cover original cartoon core palette/motion, foreground occlusion,
opaque depth, inactive pool slots and staff charge/release. Selection captures
cover front/back effect depth, burning, sky, cleared/reselected frames,
resolution changes and SSAO disabled. `IAN_CAPTURE_FIRE_WAND` exports previews.
There are no new public Renderer signatures.
