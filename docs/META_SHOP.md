# Permanent upgrade shop

SHOP opens a separate collection-style screen: five columns, item icons,
filled level squares, selected-item details, current/next bonuses, silver price
and BUY button. It uses the same colors and fonts as the quest/unlock screens.
Mouse or arrow keys select upgrades; Enter buys; Escape or X closes the screen.

## Initial catalog

| Upgrade | Maximum level | Bonus per level | Initial silver cost |
| --- | ---: | ---: | ---: |
| Silver | 20 | +5% quest silver rewards | 2 |
| Rerolls | 5 | +1 card reroll each new run | 4 |
| Luck | 10 | +2 percentage points of upgrade luck | 3 |
| Defense damage | 10 | +5% | 3 |
| Defense fire rate | 10 | +3% | 3 |
| Building health | 10 | +5% | 3 |
| Production speed | 10 | +5% | 3 |
| Resource yield | 10 | +5% | 3 |

Next cost is `ceil(initialCost * 1.5^ownedLevel)`. Purchases are atomic: a
locked, capped or unaffordable purchase leaves the profile unchanged. `App`
persists immediately after a successful purchase.

Reroll purchases require owning `meta.refresh` in UNLOCKS. That existing quest
unlock now grants one initial reroll per run; shop levels add up to five more.
The locked shop entry links directly to CARD REFRESH in the unlock collection.
Skip/banish are still future unlocks, not shop products with nonexistent effects.

## Runtime and saving

Gameplay bonuses start with the next new or restarted run. They multiply
class/run bonuses for defenses, structures, production and gathering. Permanent
luck adds to card luck. The level-up stats panel reflects the active bonuses.

A resumed run retains the bonuses captured in its checkpoint. It neither
applies subsequent purchases retroactively nor replenishes consumed rerolls.
Old checkpoints default to neutral meta bonuses. Sandbox uses the same starting
bonuses for testing but still awards no quest progress.

Silver upgrades apply when quest rewards are claimed. Integer hundredths of a
silver coin carry across rewards and reloads, avoiding repeated rounding of
small rewards. Wallet arithmetic saturates at `INT_MAX`. Silver bonus rewards
also use the existing coin-flight animation's actual credited amount.

Meta profile schema is now version 4, with shop schema 1: levels are stored by
stable catalog IDs, together with the fractional silver remainder. Versions
1–3 migrate with zero shop levels. Invalid levels or missing required unlocks
reject the profile without replacing live state. Existing quest state and
ownership remain intact.

## Public interfaces and verification

- `progression/MetaShop.hpp`: catalog, `MetaShopUpgrade`, `MetaRunBonuses`, level/
  price/availability queries, `purchaseMetaShopUpgrade`, `metaRunBonuses`,
  `validMetaRunBonuses`, `creditMetaSilver`.
- `MetaProgression`: `shopLevels`, `silverRewardRemainder`.
- `Simulation::configureNextRunBonuses`: validated setup for the next run.
- `SuspendedRunState::metaBonuses`: active run configuration.
- `ui/ShopMenu.hpp`: state/input/result structs, panel bounds and `drawShopMenu`.

`ian_meta_shop_tests` verifies prices, locks, caps, wallet arithmetic, migration,
round trips, corrupted profiles, quest credits, stacking with classes, next-run
application and checkpoint resume. `ian_shop_menu_smoke` exercises purchases,
keyboard input, locked-entry navigation, caps and EN/RU layout at 1080p, 720p and
16:10. An optional output directory exports visual previews.
