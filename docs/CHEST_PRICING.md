# Chest prices (2026-10-30)

The first exploration chest still costs 20 coins. The next price is based on
all opened exploration chests in the run, with 35% exponential growth instead
of 15%, rounded upward to five. Expected prices without discounts:

| Chest | 1 | 2 | 3 | 5 | 10 | 14 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Coins | 20 | 30 | 40 | 70 | 300 | 990 | 5,990 |

The initial fourteen exploration chests cost 3,790 coins in total. A minimum
increase of five prevents repeated prices under heavy legacy discounts.
The price saturates safely at one million. Reward chests remain free.
Failed or repeated interactions do not advance the counter; opening an
exploration chest with the legacy free key retains its existing counter rule.

HUD affordability and actual purchases share `scaledChestOpeningCost`. The
existing saved opening counter remains authoritative across loading; no new
save fields or wave-based surcharge are introduced.
