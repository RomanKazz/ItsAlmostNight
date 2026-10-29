# Fence card rarity

Fence card HP uses accumulated card power instead of only the blueprint level.
Common / Uncommon / Rare / Epic / Legendary cards add 1 / 1.2 / 1.4 / 1.6 / 2
power. The common-only HP curve remains unchanged; fractional power interpolates
that curve and final base HP rounds to five. Example with 100 base HP: the first
upgrade grants 130 / 135 / 140 / 150 / 160 maximum HP respectively.

The integer blueprint level still advances once per card. Existing fences,
doors, future copies, global HP bonuses and the card preview all use the same
health calculation. Accumulated power is already stored in run saves; older
saves with integer power retain their previous common-level strength.
