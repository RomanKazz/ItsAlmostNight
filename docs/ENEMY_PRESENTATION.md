# Enemy presentation

Enemies retain their imported models, textures and skeletal clips. This pass
adds character lighting and spring motion; combat stats, timing, collisions,
save data, loot and AI are unchanged.

## Lighting

`WorldMaterialState::enemyPresentationAmount` is an optional material flag,
defaulting to zero. Enemy rendering enables it and restores the previous state
afterward, including instanced crowds and their LOD proxies. Existing callers retain default behavior.

The world shader lifts pigment midtones while preserving black pupils and pale
eyes. Soft overhead fill and restrained cool rim light preserve readable body
colors at night. Cast shadows, environmental fog and status tint remain active.
Hit flashes use warm ivory. Other world materials retain their normal lighting.

## Motion

`EnemyPresentation.hpp` adds deterministic, render-only deformation around the
foot pivot. Walking has slight squash/stretch, alternating lean and short hops;
running leans forward. Attacks snap forward, settle, then wind up near the end of
the existing cooldown animation. Heavy enemies move less; flying enemies never
receive ground hops. Deformation preserves volume. Idle/frozen poses remain
still. No runtime animation state or new allocations are introduced.

Individual draws, instancing, shadows and selection use the same transform.
Distant crowds retain discrete pose reuse. Source clips and their interpolation
remain in the existing animation pipeline.

Death visuals briefly swell by up to 15%, then shrink during the existing fade.
Hit particles, elemental status effects and elite accents retain their current
presentation.

## Validation

Graphics smoke checks night pigment visibility, ivory hit flashes, depth
occlusion, all nine models, individual/crowd agreement, bounded continuous motion,
volume preservation and frozen foot pivots. Existing animation interpolation and
status-tint checks still run. `IAN_CAPTURE_ENEMIES=<directory>` exports previews
from the actual renderer.

This is the first lighting/motion pass. New enemy models, independent eye
tracking and new death-cloud particles are not included. Horde movement and
attack feel should also be reviewed in an interactive night encounter.

## Elite silhouettes

All three affixes now wear a thick gold crown, six pearl-tipped peaks and a
colored jewel. Berserkers add curved ivory horns with ruby mounts; Wardens add
blue enamel temple shields; Volatile elites carry orange bombs with cartoon
fuses. Combined affixes show their combined attachments. Existing affix tints,
ground accents and outline colors remain secondary cues.

Opaque procedural details follow the animated head skin matrix and the same
presentation transform as the body, including attacks, frozen poses and death
fade. Elites bypass ordinary crowd batches and draw their actual model once;
ordinary crowds retain their existing instancing and LOD. Details use the world
shader, depth and selection mask. Selection bounds include crown height.
No new textures, models, runtime containers or collision shapes are introduced.

`EnemyDrawInstance::eliteAffixes` and the last defaulted `drawEnemy` parameter
carry existing affix bits into rendering. `enemyWorldBounds` accepts a defaulted
`eliteDetails` flag to include ornaments in selection bounds.

Graphics smoke renders all nine archetypes and all three affixes, checks visible
gold at night, normal enemies remaining unadorned, occlusion, selection height,
individual/crowd parity and a second walking pose. Capture output also includes
`elite-type-0.png` through `elite-type-8.png`.
