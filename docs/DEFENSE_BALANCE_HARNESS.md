# Defensive building balance harness

From the repository root:

```sh
python3 tools/defense_balance_report.py
```

This builds `ian_defense_balance`, runs 12 Level-1 defenses for 60 seconds at
60 Hz with three fixed layout seeds, then writes `raw.csv`, `support.csv`,
`summary.json`, `report.md`, and a console table to
`output/defense_balance/current/`. A faster smoke run is:

```sh
python3 tools/defense_balance_report.py --seconds 10 --seeds 1
```

To compare a controlled patch and local-card scaling:

```sh
python3 tools/defense_balance_report.py --upgrades --output output/defense_balance/before
# Apply the intended combat-value patch; use the same seeds and duration.
python3 tools/defense_balance_report.py --upgrades --output output/defense_balance/after
python3 tools/defense_balance_compare.py
```

The comparator rejects mismatched seeds, scenarios, profiles, run length, tick
rate, or game-data JSON hashes. It writes `comparison.md`, `comparison.csv`,
`comparison.json`, and `scaling.csv` under `output/defense_balance/comparison/`.
The optional `upgrades.csv` records exact final stats applied through the
production local-building-upgrade path. Named profiles isolate and combine
count, pierce, bounces, size, duration, and crit. `mid` and `high` use only
eligible stats for their building.

The C++ executable also accepts `--output`, `--upgrades-output`, `--data`, `--seconds`, `--seeds`,
and `--self-test`. It reads the six current game balance JSON files. No save or
configuration is written. The older multi-level/card `ian_balance_lab` remains
available separately.

Combat uses the real `EnemySystem`, `TowerSystem`, `CannonSystem`, and
`TrapSystem`. Their defaults have no Tome, card, character, passive, or meta
modifier. A single structure has Level 1 and the first-copy price. The
stand removes leaking enemies and replenishes the active population, so faster
kills permit more kills over the same interval. The core is far away and the
defense does not body-block the lane. Intrinsic Slow Trap wear is applied.

Scenarios are `single_infinite`, `single_infinite_heavy`, `single_normal`,
`single_fast`, `single_heavy`, `single_boss`, `dense_line`, `dense_cluster`,
`spread_group`, `fast`, `flying`, `mixed`, and `passing`. Infinite singles have
very high HP. Finite singles have real base archetype HP and damage mitigation,
but no enemy AI movement/summons; Boss is thus a stationary single target,
without wave multipliers. They stop on death or after 6000 seconds and record
TTK, raw/effective/overkill damage, event and projectile counts, acquisition
delay, and Laser focus milestones. Dense ideal layouts use stationary enemies but
still respond to real knockback. The ranged ideal lane is 6.5 m from the
building; only Flamethrower and Wind Blower are closer to stay in their
authored range. Passing uses the same path 2 m to the side of all ranged
buildings. Ground traps sit on the passing path. The mixed spawn cycle is
50% Normal, 25% Fast, 20% Tank, and 5% Ranged, permuted by seed. Range, arc, projectile flight,
target search, pierce, chain bounces, explosions, burn, crit, slow, and
knockback all come from production combat code. The actual pre-HP-clamp
damage is observed through disabled-by-default EnemySystem sampling; the
difference from effective damage is overkill. UI damage equals simulation
damage ×10.

`summary.json` includes mean/min/max for each scenario and the per-scenario
ratio to Machine Gun Turret, as well as DPS divided separately by each nonzero
resource cost and damage per occupied defense slot. `N/A` means a weapon
cannot attack the target category, rather than a failed attack. No resource
exchange rate or combined building score is assumed.

Control seconds count each affected enemy once per tick. Weighted slow
seconds additionally multiply by slow strength. Added exposure estimates
delay from slow and backward knockback using actual status/velocity values;
it is reported in enemy-seconds and never converted into damage.
`slow_presence_fraction` is the fraction of frames with at least one slowed
enemy; `slow_uptime` in JSON is the fraction of all available enemy-time.
Repair uses
`repairStationDose` and `BuildingSystem::restoreHealth` on sustained damaged
targets. The Decoy test uses the real taunt/navigation and enemy attacks; it
starts just before a normal pulse and reports survival and absorbed damage.
The isolated Decoy test cannot reliably convert taunts into prevented attacks
on the core.

`attack_event_tick_fraction` is the fraction of frames starting an attack;
`target_engagement_fraction` is the fraction with an acquired target or active
trap. They have different meanings. `first_attack_delay_seconds = -1` means no
attack occurred. `secondary_damage_ui` identifies pierce, bounce, or splash
damage for the weapon that produced it. Laser focus extra damage is an estimate
from effective hits and the real ramp multiplier. Per-projectile misses and
late-arrival waste are not observable through current public combat events;
the report does not label a derived hit/event ratio as a physical miss rate.

Regression checks:

```sh
ctest --test-dir build/release -R 'ian_defense_balance_.*checks' --output-on-failure
```

The checks cover air restrictions, the Mortar dead zone, Laser focus reset on
target switch, finite-HP TTK, exact card-stat application, fractional count
storage and flooring, and paired seed validation.

The benchmark is diagnostic. It does not alter buildings, waves, cards,
Tomes, the Core, economy, or enemies.
