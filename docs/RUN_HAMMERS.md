# Run hammers

Hammers are persistent stat items acquired through level-up cards. The first
selection acquires a hammer at level 1; further cards upgrade that same item.
Rarity scales each individual bonus using the existing Common-to-Legendary
multipliers. A hammer accepts five level selections under the existing power
caps. There is currently no limit on the number of distinct hammers owned;
they do not consume combat building slots.

The thirteen types are defense damage, defense fire rate, building HP,
production speed, luck, build range, resource yield, maximum HP, shield,
movement speed, weapon/tool speed, player damage, and health regeneration.
Existing bonuses are represented by their original stable RunUpgradeEffect IDs,
with new hammer names and shared card/HUD glyphs. Old saves therefore retain
both the level and accumulated rarity power without adding the bonus again.
Blueprints, building-specific improvements, cannon styles and special cards
remain separate upgrade types.

The new regeneration effect is appended to the ID list and provides +0.5 HP/s
per Common-card power unit. It heals during active gameplay, stacks with existing
healing, stops while paused/choosing cards or respawning, and clamps to maximum
HP. It does not recharge the shield. New runs clear all hammer levels and power.

Owned hammers appear beneath the resource counters in the HUD and level-up
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
