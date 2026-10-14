# Player death, respawn and hit audio

Death now shows a chamfered card matching the menus: cartoon skull, short headline,
whole-second countdown, progress bar and icons for resources actually lost. There
is no duplicate status-message toast. English and Russian are supported. The
existing respawn duration and resource-loss fraction remain unchanged.

The view settles downward after death. Physics and controls cannot keep moving
or dropping the dead player. The final 0.30 seconds fade to black; relocation
occurs behind that cover and the new location fades in over 0.65 seconds. Camera
inertia, hit flash, queued attacks and recoil reset, and the tool raises again.

Respawn searches near the core, then around the map spawn if necessary. Six
concentric rings with 24 samples use the actual movement broadphase, resource
cylinders, vertical collider intervals, supporting surfaces, ceiling checks,
terrain boundaries and water depth. Living enemy cylinders contribute clearance;
a point with 2.5 meters of clearance is preferred. If a completely sealed map has
no legal point, the existing authored map-spawn fallback remains.

Respawn restores permanent HP and recoverable shield capacity, clears temporary
class abilities/dash/flight state, and grants 2.5 seconds of protection. This is a
separate simulation timer, not a toggle of persistent debug god mode. It freezes
with simulation pause and clears on restart or suspended-run restoration. Damage
and boss impact knockback cannot immediately kill or launch a respawning player.
A small shield badge shows the remaining protection after arrival.

Pending upgrade/shop UI cannot hide the death card or stall the respawn timer.
Queued upgrades remain available after the player returns.

## Audio

- `PlayerDamaged` now plays the existing `player_hit.ogg` for enemy hits as well as
  falls. Gain 0.60, small pitch variation, minimum onset interval 0.14 seconds.
  An occupied voice is allowed to finish; a crowd cannot repeatedly truncate it.
- `ItemExplosion` (Coal/burn-death reaction) now uses `explosion.wav` through the
  existing shared explosion budget: maximum three concurrent voices, at least
  0.14 seconds between onsets, spatial attenuation and mix headroom. Visual
  explosions and damage are not reduced.
- Sound/visual resources are loaded at initialization, not on death or first hit.

## Interfaces and verification

Added `CollisionWorld::canOccupyCircle`, `findPlayerRespawn`,
`SimulationSnapshot::playerRespawnProtectionRemaining`, `drawPlayerDeathOverlay`,
and `flushSdfUiTextBatch`. The text flush preserves the caller's batch while
ensuring labels render before the blackout, preventing text from appearing over
what should be a fully covered relocation frame. No save IDs changed.

`PlayerRespawnTests` covers expanded core search, actual enemy clearance, resource
cylinders and vertical intervals, boundaries, pre-core spawning, immobility while
dead, full HP/shield restoration, protection expiry/pause and clean restart.
`PlayerDeathOverlaySmokeTests` renders both languages and checks both sides of the
black transition. Captures are generated in `/tmp/ian-death-card`.
`AudioSystemSmokeTests` starts real sound voices with master volume zero and
checks mob-hit admission and bounded Coal explosions.

The native menu test's Retina capture now requests `FLAG_WINDOW_HIGHDPI`, matching
the application: without it, the resized hidden window used a logical-size
viewport while readback produced physical-size pixels, placing the sampled title
in an unrendered part of the image. Its original title visibility assertion is
retained; failures now export a diagnostic capture.

Validation: full Debug suite, focused audio/death/respawn checks, full simulation
unit suite with ASan/UBSan plus audio/death/respawn checks, and Release build.
The Debug menu test initially failed its Retina capture and passed after fixing
its window flags. Manual gameplay playtest remains separate from these checks.
