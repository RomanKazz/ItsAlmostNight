# Utility shop and run slots

The permanent shop contains eight entries: Silver, Rerolls, Banish, Skip,
Equipment Slots, Tome Slots, Structure Slots and Toggler. Former Luck, damage,
fire rate, structure HP, production and yield ranks no longer affect new runs.
Their original purchase prices are refunded when an older profile is loaded.
An already suspended run retains its original bonuses until it ends.

Tomes and defense types start with three slots and can reach five through two
shop purchases (10 and 15 silver). Equipment retains four base slots and a
six-slot cap. Old third/fourth tome or defense slot purchases are refunded
(23 and 34 silver per category); collected contents of a legacy run remain.
The nested shop schema is now version 3. Saving after migration makes all
refunds idempotent; profile progress and unclaimed quest rewards remain intact.

Reroll replaces the current level-up offer. Skip consumes one level-up choice
without an upgrade. Banish consumes one choice and removes the selected card
type from the pool until the run ends, including future queued offers and
rerolls. Banish can target the paid fourth card without paying its coin cost.
The controls are R, X and Z respectively, with clickable buttons and remaining
uses below the cards. Shrine choices do not consume these tools. Budgets,
spent uses and banned card types survive suspension and night checkpoints.

Every purchased Reroll/Skip/Banish rank adds one use per new run. Unlocking the
Reroll milestone itself no longer grants a free use; the old five-silver gate
purchase is refunded. Reroll unlocks at 20 completed quests, Skip at 30 and
Banish at 40. These are free gates followed by paid shop ranks.

Toggler unlocks after eight discovery purchases and costs 50 silver once.
Owned, non-starter weapons, tomes, defenses and chest items gain an Enable /
Disable button in Unlocks. Changes apply to future runs; default content
always remains available. Disabled choices persist in the profile. Eight is
reachable with this game's twelve paid exotic discoveries.

Reference mechanics were checked against the [Megabonk shop catalog](https://megabonk.wiki/wiki/Shop),
[beginner guide](https://progameguides.com/megabonk/megabonk-beginners-guide-how-to-farm-silver-unlock-extra-slots/)
and the [Banish/Skip mod's explanation of vanilla behavior](https://thunderstore.io/c/megabonk/p/mike9k1/Better_Banishes_and_Skips/).
Prices and discovery thresholds are adapted to this game's economy.

Validation covers purchase limits/refunds, profile and run round trips,
queued choices, bans on rerolls, new-run reset, invalid budgets and both
languages at multiple UI aspect ratios.
