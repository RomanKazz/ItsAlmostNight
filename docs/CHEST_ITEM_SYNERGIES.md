# Chest items and fractional combat counts

Implemented 2026-10-14. The chest pool contains the nine selected combat items.
Legacy loot IDs 0–18 are retained for checkpoints, class starting items and debug
commands. New IDs are appended at 19–25. Ordinary chest/reroll/container pools no
longer offer the old utility items. Opening, prices and rerolls remain unchanged.

## Items

All applicable direct hits include player weapons, turrets, artillery and damaging
traps. Periodic burn/poison and item-generated hits do not reroll on-hit effects.

| Item | First copy | Further copies |
| --- | --- | --- |
| Fuel Jerrycan | 30% ignition chance; four-second burn based on hit damage. Natural fire also activates it. Repeat applications build up to eight burn doses. | +5 percentage points ignition chance, up to 75%; stronger burn. |
| Coal Bell | Burning deaths produce a 2.3 m fire burst and ignite survivors. Works with the fire wand and flamethrower without a canister. | More damage and radius, up to 4.5 m. |
| Powder Pouch | Critical direct hits explode in 1.8 m; nearby enemies take 35% of the triggering damage. The primary target is excluded. | More damage and radius. |
| Plague Mushroom | 15% poison chance; enables four doses per enemy, each extra dose adds 35% of the strongest base poison DPS. Combines with the Venom Tome chance. | More application chance, up to 75%; more allowed doses, up to sixteen. |
| Infected Pouch | Poisoned deaths leave a three-second, 2.4 m cloud. The cloud applies poison at 60% of the original base poison DPS. | Stronger/larger clouds. |
| Gas Mask | Each poisoned enemy within 12 m reduces incoming damage by 3%. Total reduction is capped at 35%. Does not apply to armor-piercing damage or falls. | Reaches the protection limit with fewer enemies. |
| Ice Prism | Frozen deaths release damaging shards in 2.8 m. Damage scales from the last direct hit, with a minimum of two. Slowed targets take 1.75 times shard damage. | Stronger/larger shatters. Requires a source of freezing. |
| Nail | Every fifth direct hit launches a two-jump lightning chain. Independent counter for the player and each building. | More frequent triggers, more jumps and stronger chains. |
| Targeting Lens | Player hits mark a surviving target for four seconds. Direct defense hits deal 25% more damage; targeting prefers valid marked enemies. | Longer marks and stronger bonus, up to 75%. |

Common pool: canister, mushroom, powder, nail, lens. Rare pool: coal, infected
pouch, mask, prism. Existing 40% exploration / 28% reward rare-roll thresholds
are retained. New SVG icons are rasterized once at UI startup. New world props
use small solid low-poly geometry; canister/nail retain their authored models.

## Runtime rules

- Original projectile travel, misses, bounce/pierce counters, crit flow and pools
  remain intact. Item damage uses `EnemySystem::damage` for mitigation and kills.
- Lens targets obey normal arc, range, minimum range and gun height restrictions.
  Retargeting stays on the normal search cadence; no target search overhead is
  added to runs without a lens. Existing attacks retain their snapshots.
- Item kills emit the normal attributed kill event, yielding normal XP/gold.
- On-hit processing uses a fixed event boundary. Death reactions use another
  fixed boundary and a per-entity generation/death guard. Events appended by a
  death reaction cannot recursively enter that same pass.
- Enemy effect state is allocated for 4096 reusable slots on construction.
  Poison clouds reuse reserved storage, bounded at 128; drawing is bounded at 32.
  Lightning/explosion/shatter reactions are bounded at 64 per simulation step.
  Visuals share depth-tested cartoon fire, ice and poison effects and existing
  lightning sound throttling. There is no new simultaneous explosion audio spam.
- Checkpoints keep item stacks. Combat actors/statuses/clouds restart with the
  replayed wave, following the existing suspended-run checkpoint semantics.
- JSON loads accept the legacy 19-entry item array or its longer appended form.

## Fractional counts

Count stats and persisted upgrade power use floating-point values. An awarded
upgrade never rounds its power. Rarity factors are 1, 1.2, 1.4, 1.6 and 2.
Fixed unlock/weapon/building level cards retain their existing one-level behavior.

`resolveAttackStats` preserves fractional Quantity, Bounces and Pierce. Only
actual instance creation, burst timing and projectile counters use
`effectiveAttackCount(value) = floor(value)` (with finite/range validation).
There is no stochastic rounding. UI/debug retains the floating total; debug also
shows the active whole count. Example: 1.6 extra projectiles give one extra now;
another 1.4 makes three extra. An authored three-bullet gun therefore emits four
and then six bullets per event.

Cluster payloads, shrapnel, nightly bombs and overkill use the same floor-total
rule through `runUpgradeDiscreteAmount`. Multiplying by count-per-power occurs
before flooring. Existing caps are preserved. Saves preserve unused fractions.
Normal Attack Speed, projectile speed, duration, size, crit chance and damage are
continuous and are not floored.

## Interfaces and validation

New public interfaces: `ChestItemCombatSystem`, `ItemPoisonCloud`,
`EnemySystem::markTarget/markedDefenseDamage`, optional poison dose bound,
`effectiveAttackCount`, and defense targeting-preference setters. Snapshot count
fields now retain floating totals and include a borrowed cloud span.

Automated checks cover fractional awards/consumption, projectile counters,
per-source nail chains, exclusion of secondary on-hit procs, coal propagation,
cloud poisoning, mask bounds, prism/slow interaction, lens damage and targeting
restrictions, save/load/reset, icons and solid world geometry. Debug and sanitizer
results are reported with the completed change.

Playtest priorities: fire spread density, maximum poison doses, clustered critical
explosions and lens focus against bosses. The listed numbers are a first balance
pass; they are not claimed to be Megabonk's values.

### Recorded validation

- Debug full suite: 22/23 initially passed. The remaining UI assertion still
  expected the old legendary poison factor; after updating that expectation,
  its targeted rerun passed. All 23 checks therefore pass with the new balance.
- ASan/UBSan: 7/7 related checks passed, including the full unit suite (454.63 s).
- Release: build succeeded; attack, chest-item and upgrade-UI checks passed 3/3.
- `git diff --check`, localization JSON and runtime/source icon-byte checks passed.
- Native previews: `art/ui/chest_items/contact-sheet.png`; all seven new world
  props are rendered and checked by `ChestItemGraphicsChecks.hpp`.

Main integration files: `LootChestSystem`, `SimulationLoot/Progression/Snapshot`,
`EnemySystem/EnemyCombat`, `AttackStats/AttackPattern/AttackProjectile`,
`TowerSystem/CannonSystem/TrapSystem`, `RunUpgrade`, `AppRunSave`,
`AppEventPresentation/AppEffects/AppRunUpgradeOverlay`, `GameUi`,
`RendererLootModels` and localization. Unrelated existing workspace changes are
preserved; the scoped commit contains the new module, assets, checks and this
record.
