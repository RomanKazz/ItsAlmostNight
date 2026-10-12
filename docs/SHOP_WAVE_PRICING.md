# Run shop wave pricing

The four-offer shop becomes more expensive each wave, rather than charging an
extra multiplier after every purchase in the same visit. All newly generated
cards and carried locked offers use the current completed-wave number.

Let `n = max(0, wave - 1)`:

`price = ceil(basePrice * (100 + 25*n + n*n) / 100)`

BasePrice remains 12 for stats/tomes or 18 for building/weapon blueprints, plus
8 per rarity tier. The arithmetic uses 64-bit integer intermediates and checks
before multiplying; unrepresentable late-run prices saturate at INT_MAX.

| Completed wave | Common tome | Rare tome | Common blueprint | First reroll |
| --- | --- | --- | --- | --- |
| 1 | 12 | 28 | 18 | 2 |
| 2 | 16 | 36 | 23 | 3 |
| 5 | 26 | 61 | 39 | 6 |
| 10 | 49 | 114 | 74 | 11 |

Reroll price is `2 + n + (1 + floor(n/4)) * paidRerollsThisVisit`.
The ladder resets for each new wave. Buying out all four offers still refreshes
for free without increasing this paid-reroll counter.

A lock reserves card identity and rarity, not its old price across waves.
Starting the next visit reprices locked cards before filling unlocked slots.
Rerolling or closing/reopening during the same wave preserves existing prices.
Loading a suspended run preserves stored offers and costs until the next wave;
no save fields/IDs were added or reordered. Godmode still bypasses charging.

Tests cover every rarity/blueprint getting dearer over 100 waves, exact early
price examples, extreme wave/reroll arithmetic, same-visit locked-price
preservation, next-wave repricing, RNG/save/load, free buy-out and godmode.

Public function signatures are unchanged: runShopPrice/runShopRerollPrice.
The growth curve is a balance starting point; evaluate coin income and purchases
in a full run before treating it as final progression tuning.
