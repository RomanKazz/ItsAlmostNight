# Four test characters — 28 October 2026

The selectable roster contains four characters, all available on a new profile.
Their icons reuse the axe, rifle, wrench and pickaxe. No new models, attack
animations or active abilities are introduced.

| Character | Passive | Each earned level after level 1 | Starting gear | Drawback |
|---|---|---|---|---|
| Lumberjack / Дровосек | +20% melee damage, +20% wood from manual harvesting | +2 percentage points of base melee damage; +3 maximum HP | Club, spikes blueprint | −10% movement speed |
| Sharpshooter / Стрелок | +15% ranged attack speed | +2 percentage points of base ranged damage; +2 maximum HP | Rifle, crossbow blueprint | −15% base maximum HP |
| Mechanic / Механик | +20% building/platform HP; manual repairs cost 30% less | +2 percentage points of base defense damage; +2 maximum HP | Club, cannon blueprint | −15% player weapon damage |
| Miner / Старатель | +20% manual resource yield | +2 percentage points of base producer speed; +2 maximum HP | Gun turret blueprint, Crystal Pan | −10% defense damage |

All characters retain the axe and pickaxe. A starting blueprint permits its
ordinary run cards even if its global discovery has not been purchased.
Ranged bonuses cover the rifle, both wands and bombs, without affecting melee
speed. Sharpshooter has no standing-still reload restriction. Lumberjack does
not charge roots; Miner does not chain-harvest; Mechanic has no legacy Engineer
cooldown or fortification-duration bonus.

Growth is derived from total earned levels. HP is a flat addition after the
class base multiplier: a level-11 character has +30 HP (Lumberjack) or +20 HP
(others). Gaining a level restores only the newly added HP, not full health.
Refreshing equipment, loading the same progression twice and resuming do not
accumulate the bonus again. Restart resets earned growth.

The repair discount applies before rounding resource costs upward, to both
ordinary buildings and modular structures. Validation and payment use the
same discounted cost. Existing free-repair relics still work; the Core still
cannot be manually repaired.

## Compatibility and quests

New class IDs are appended to `PlayerClass`. All previous numeric IDs and
class definitions remain readable for already suspended runs; they do not
appear in selection or the collection. Continuing an old run keeps its old
class and abilities. New-run selection normalizes a retired selection to
Miner, which is also selected on application startup and after a progress reset.
No profile, silver, purchases or quest completion is erased.

The four retired class-unlock quests keep their IDs, conditions and silver
rewards as general goals. They no longer sell invisible characters. Already
owned legacy unlock IDs remain in profiles. The six-night Engineer challenge
is presented as Mechanic; its stable quest role ID `engineer` is shared with
legacy Engineer saves. The JSON and compiled fallback catalog are generated
together by `tools/generate_meta_quests.py`.

## Interfaces and checks

- `PlayerClassDefinitions` is the active four-entry roster;
  `LegacyPlayerClassDefinitions` supplies suspended-run compatibility;
  `isSelectablePlayerClass` separates selection from loading.
- `PlayerClassLevelBonuses` derives flat HP, melee/ranged damage, ranged speed,
  gathering and production growth.
- Building and foundation repair APIs accept an optional `costMultiplier`;
  existing callers default to the original price.
- Bomb modifiers now support class damage and attack speed; cooldown save
  validation uses the saved class rather than the previous runtime's class.
- `ian_basic_classes_tests` exercises real damage, resource yields, producer
  cadence, repair affordability, loadouts, HP, restarts and all legacy saves.
  Menu smoke tests cover selection, keyboard navigation, localized details,
  collection and aspect ratios with exactly four cards.
