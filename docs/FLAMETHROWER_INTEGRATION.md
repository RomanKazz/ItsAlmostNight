# Flamethrower integration

Building save ID 15; acquisition/upgrade card ID 59. Stable IDs and checkpoint
version 7 are unchanged. Existing core limits, cards, shop, Shift+F5, placement,
rotation, sale, destruction, save/load and global modifiers are shared.
Cost: 35 wood / 25 stone / 10 crystal; base HP 160; footprint 2×2.

## Attack and progression

TowerSystem owns one smooth aim runtime. Finite 32° flame cone starts at the
actual muzzle. Heat pulses use FinalAttackStats, shared capsule contacts, crit,
knockback and damage attribution; no artificial projectiles or burst sequences.

| Level | Pulse damage | Range | Heat interval | Raw direct DPS |
|---|---:|---:|---:|---:|
| 1 | .60 | 4.0 | .2000 | 3.00 |
| 2 | .85 | 4.2 | .1923 | 4.42 |
| 3 | 1.10 | 4.4 | .1852 | 5.94 |
| 4 | 1.40 | 4.6 | .1786 | 7.84 |
| 5 | 1.70 | 4.8 | .1724 | 9.86 |
| 6 | 2.00 | 5.0 | .1667 | 12.00 |
| 7 | 2.35 | 5.2 | .1613 | 14.57 |
| 8 | 2.70 | 5.4 | .1563 | 17.28 |

Afterburn: strongest dose per enemy, refreshed for 2 seconds; half-second ticks
apply 25% of pulse damage. Duration extends afterburn; Size extends actual cone
and the white hover/placement range. Supported: Damage, Attack Speed, Duration,
Size, Crit Chance/Damage, Knockback, Range. Quantity, Projectile Speed, Bounce,
Pierce are inapplicable. Periodic burns cannot recursively trigger on-hit procs.
Burn state has a bounded enemy-slot lookup and is cleared on reset/wave cleanup.

## Model, effects and audio

The user's original GLB and colormap stay intact. Seven cumulative three-mesh
upgrade shells supply distinct levels. Base/yaw/pitch poses are shared by model,
shadow, selection, preview and raycast. Raised details count toward UI height and
precise targeting. Pitch stays within the safe −10°/+21° range.

Analytic cartoon fire heads move at 7.6 m/s. Smooth 0.16-second ignition propagates
from the muzzle; after stopping, the tail keeps travelling and dissolves. Runtime
retains ignition age and bounded tail time separately from heat damage events.
No particle containers or per-frame allocations. Global cap: 384 moving heads,
LOD 18/10/6, distance cull 32m, one triangle batch. Flames depth-test against the
world and cover ink behind them without receiving black world outlines.

One seamless shared audio loop fades by nearby active emitters; crowded jets do
not multiply identical gun/impact sounds. Visual hit feedback is coalesced per
enemy per presentation frame, without reducing real simulation damage events.

Public additions: `GraphicsResources::flamethrowerUpgradeModel`, extended
`Renderer::drawFlamethrower(..., pitch, headYaw, level)` and optional base yaw in
building raycast; snapshot blueprint levels / selected preview FinalAttackStats;
TowerRuntime flame age/tail; diagnostic `TowerSystem::activeFlameBurns`.

Defense hotbar order: gun turret, crossbow, cannon, catapult, flamethrower, spikes,
slow trap. Its new RGBA icon is shared across existing UI consumers.

## Validation

Independent flame tests cover levels, cone contacts, Attack Speed/Size/Duration,
crit, afterburn, target loss/tail expiry, reset and 32-jet/1000-enemy stress.
Release stress measured ~1.18 ms/tick for the simulation-only overlap fixture;
this is not a native game's FPS measurement. Graphics smoke covers animation,
world occlusion, white hover range, all eight model levels and raised-part raycast.
Blender round trip and 7,560-pose geometry sweep pass. Native combat playtest
remains necessary for damage balance and flame feel. Full suite results are
reported separately; unrelated UI/graphics tests can fail in this working tree.
