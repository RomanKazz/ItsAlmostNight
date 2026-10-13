# Overtime pressure and continuous production

Megabonk's Final Swarm adds escalating survival pressure after the stage timer.
The developer's [v1.0.12 patch](https://store.steampowered.com/news/app/3405340/view/534363696967712871)
explicitly hardened that phase to prevent unintended five-hour runs. This game
adapts the time-pressure concept, with its own balance curves and existing enemies;
it does not reproduce undocumented Megabonk formulas or invulnerable ghosts.

## Clock

Starts at wave 11, accumulating **active combat seconds**, including prolonged boss
fights after the dawn countdown and Final Night. Preparation, sunset, dawn, pause,
shop, level-up selection, stage-clear choice and challenge arenas do not advance it.
The ordinary wave/day/shop loop and wave-12 stage-clear choice remain available.

The HUD shows `OVERTIME mm:ss` and HP/damage multipliers. The clock is retained
between nights. Combat saves still replay the pre-wave checkpoint without retaining
combat rewards, but preserve the current pressure clock so reloading cannot rewind
difficulty. Old saves without the optional `resume.overtimeSeconds` key start at zero.
A new run resets the clock. Sandbox has no automatic overtime.

## Balance

Configured in `assets/data/gameplay.json`, under `overtime`:

```text
health = 2^(combatSeconds / 60)
damage = 2^(combatSeconds / 120)
speed = min(1.75, 1 + 0.12 * combatMinutes)
spawnRate = min(2, 1 + 0.20 * combatMinutes)
```

| Combat time | HP | Damage | Movement | Spawn pressure |
|---|---:|---:|---:|---:|
| 0:00 | 1 | 1 | 1 | 1 |
| 1:00 | 2 | 1.41 | 1.12 | 1.2 |
| 3:00 | 8 | 2.83 | 1.36 | 1.6 |
| 5:00 | 32 | 5.66 | 1.6 | 2 |
| 10:00 | 1024 | 32 | 1.75 | 2 |

These multiply normal wave growth, elite bonuses and Final Night scaling. HP and
damage are numerically bounded at 40 doublings, not a gameplay population limit.
Spawn pressure affects both group interval and target active population, retaining
the existing 2048-enemy limit and bounded group debt. No new per-frame containers.

Live enemies preserve their health fraction, status effects and AI state as their
stats grow. Dead actors are not revived. New spawns receive the current multiplier.
Split children and royal summons inherit ordinary scaling and get overtime exactly
once. In-flight hostile projectiles receive damage scaling too. Boss storm interruption
keeps both its threshold and earned damage fraction consistent during growth. Numeric
HUD/event counters saturate safely rather than wrapping at extreme damage/health.

Public additions: `OvertimeConfig`, `OvertimeDifficulty`, `overtimeDifficulty()`,
`GameplayBalanceDefinition::overtime`, `EnemySystem::setOvertimeDifficulty()`,
snapshot `overtimeActive/overtimeSeconds/overtime`, saved `overtimeSeconds`.
Stable enemy/upgrade/event IDs and physical projectile behavior stay compatible.

## Producers

Crystal Mine, Quarry and Lumber Mill now produce equally by day and night. Their
base speed is two-thirds of the previous speed, preserving per-cycle yield and all
multiplicative production/health/level bonuses. Base cycles: crystals 18 seconds,
stone 15 seconds, wood 12 seconds. Cycle times still round to quarter-seconds.
Activated world mines and mills also work at night, with 15/12-second base cycles.
Challenge arenas retain their existing production suspension.

Starting a regular night early still resolves the skipped daytime/sunset production.
Local storage limits, collection, damage efficiency and saved production progress
continue to apply. God mode no longer determines whether night production is enabled.

## Validation / playtest

`OvertimeChecks.hpp` covers wave threshold, live/new scaling, delayed splits,
finite extreme values, storage reuse at the population limit, clock persistence,
pause/shop/day/stage-clear, boss stalling, Final Night reinforcements and real day/night
producer output without god mode. Existing mine and balance checks use the new rates;
the responsive HUD smoke was also used to inspect the pressure badge across its
aspect ratios. Release system checks pass; full sanitizer testing passes 19/20,
with the existing `ian_run_menus_smoke` SDF/scissor name-rendering failure remaining.

Playtest weak and strong builds on waves 11–12 and 3–5 minutes into Final Night.
The 60-second HP doubling is deliberately aggressive; adjust the JSON doubling
interval if late-stage bosses become a damage wall too early. This system increases
pressure, not a scripted guaranteed defeat or a claim of exact Megabonk balance.
