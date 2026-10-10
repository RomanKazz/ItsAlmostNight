# Venom / Яд

`RunUpgradeEffect::PoisonChance` is appended at stable save ID 56. Previous IDs
remain unchanged; checkpoint loading zero-fills the appended array slot.
Public API additions: `StatusEffectType::Poison`, `EnemyPoisonHit`,
`EnemySystem::applyPoison` and `EnemySystem::tickPoison`.
The normal reward catalog and Shift+F5 grant catalog both include this card.
Its own green vial icon and localized description use the normal UI.

A common card adds 10 percentage points of on-hit chance. Rarity multiplies
this bonus using the existing 1 / 1.5 / 2 / 2.5 / 3 power scale. Chance caps at
100%; normal power gains also stop at that cap. Debug bulk grants remain valid.

Successful friendly damage events can poison: melee/tools, rifle/bombs, ice,
fire direct impacts, chain lightning, overkill, towers/crossbow, cannon/catapult
and damaging traps. Environmental/enemy volatile blasts and burn/poison DoT
cannot roll poison. Zero damage and already dead targets cannot acquire it.
The deterministic proc hash uses gameplay tick, target and hit index.

Poison lasts **4 seconds**, dealing **25% of actual triggering damage per
second** (100% extra damage over its complete duration). It pulses every half
second, flushing a partial final pulse. Refreshes preserve the strongest dose,
update duration and never reset the pulse timer or stack extra independent DoTs.
Stronger doses take over kill attribution. EnemySystem owns the fixed status
slot and uses a pre-reserved hit buffer, shared enemy damage/elite mitigation,
kill/split flow and normal reward events. No new projectile or hit system.
It continues while frozen and while the player is respawning, but game pause,
reward selection and run end stop gameplay normally.

Presentation reads the actual enemy status: soft green puffs around the lower
silhouette and rising cel-shaded lime bubbles with cream highlights. Fade,
phase and sway are deterministic per enemy. Rendering respects world depth and
selection effect coverage. Nearest 64 visible enemies receive poison effects;
the nearest 24 get the richer bubble pattern. The particle toggle preserves a
small readable status cue. Geometry is batched and no textures or per-frame
particle allocations are needed. Restart/death clear or hide stale effects.

Balance playtest: 10% base chance and 100% extra hit damage over four seconds
are initial values. Dense bursts and splash weapons roll per actual hit; a
single enemy still has one strongest poison dose. This favors sustained area
control without infinitely stacking DoT from a high projectile count.
