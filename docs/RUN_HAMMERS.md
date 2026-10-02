# Run hammers

Hammers are persistent stat items acquired through level-up cards. The first
selection acquires a hammer at level 1; further cards upgrade that same item.
Rarity scales each individual bonus using the existing Common-to-Legendary
multipliers. Most hammers accept five level selections under the existing power
caps; Salvage retains its three-level/three-power cap. There is currently no
limit on the number of distinct hammers owned;
they do not consume combat building slots.

The eighteen persisted types are defense damage, defense fire rate, building HP,
production speed, luck, build range, resource yield, maximum HP, shield,
movement speed, weapon/tool speed, player damage, health regeneration, salvage
refunds, day extension, and
production speed for each of the three economic building types.
Existing bonuses are represented by their original stable RunUpgradeEffect IDs,
with new hammer names and shared card/HUD glyphs. Old saves therefore retain
both the level and accumulated rarity power without adding the bonus again.
Blueprints, cannon styles and mechanical perks (lifesteal on kills, piercing,
ricochets, additional shots, cluster explosions and reactive shockwaves) remain
separate upgrade types. Fifteen hammers are currently offered: the three
producer-specific speed hammers are retired, with shared Industry remaining.
Already acquired producer hammers keep their power and HUD display in old runs.
The retired Quick Tools card stays out of rotation; Haste covers tool speed.
Daylight remains available. Reroll and Double Down cards are also retired;
meta progression still supplies rerolls and old runs keep earned tokens/picks.
Overkill is the damage-transfer card, distinct from projectile piercing.
The active rotation contains 37 card types. Stable IDs are never removed;
old pending offers containing retired cards regenerate on resume.

The new regeneration effect is appended to the ID list and provides +0.5 HP/s
per Common-card power unit. It heals during active gameplay, stacks with existing
healing, stops while paused/choosing cards or respawning, and clamps to maximum
HP. It does not recharge the shield. New runs clear all hammer levels and power.

Owned hammers wrap into rows of up to thirteen icons and appear beneath the
resource counters in the HUD and level-up
menu. Their type color is stable; card borders continue to indicate offered
rarity. Hovering an inventory glyph while the cursor is visible shows its name
and level. Level-up hover previews show projected stats, including fractional
regeneration. The glyphs use raylib shapes, with no new bitmap asset dependency.

## Interfaces

- `RunHammerDefinition`, `RunHammerDefinitions`, `runHammerDefinition(effect)`:
  stat-item metadata over stable upgrade IDs.
- `RunUpgradeStats::healthRegeneration`: passive HP/s from run hammers.
- `drawRunHammerIcon` and `drawRunHammerInventory` in `ui/RunHammerUi.hpp`:
  shared card/HUD presentation, accepting opacity and optional test pointer.
