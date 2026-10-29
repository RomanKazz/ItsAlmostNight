# Expanded first-run pool (revision 5)

Starter means freely eligible for normal run pools and the unlock catalog, not automatically equipped. Existing in-run acquisition, rarity, slot limits and building placement rules still apply.

| Category | Starter | Total gated entries | Remaining discoveries |
| --- | ---: | ---: | --- |
| Buildings | 13 | 18 | Mortar, repair station, slow trap, laser tower, wind blower |
| Tomes | 14 | 15 | Duration tome |
| Chest items | 14 | 18 | Ghost candle, Core fuse, lucky die, upgrade mirror |
| Weapons | 3 | 5 | Ice and fire wands |

Core, gate, platform, ramp and the two harvesting tools keep their normal availability outside these gated-entry counts. Rifle and bombs now join the club in the free weapon pool. Rockets and Tesla are starter defenses; mortar, repair station and slow trap retain their original mastery requirements and purchase prices.

New profiles have all common and rare relics, mining hammer and dawn coin at epic rarity, and crystal safe and rebuild blueprint at legendary rarity. High-rarity chest rewards therefore retain their rarity from the first run. The remaining discoveries still require both their mastery goal and their silver purchase. Permanent shop gates and slot ranks are described in `UTILITY_SHOP.md`.

`ContentUnlocks.hpp` is the source of truth for starter flags. `tools/generate_meta_quests.py` reads those flags and turns quests for starter content into silver-only mastery goals while retaining their IDs, conditions and silver rewards. It writes both the JSON catalog and matching C++ fallback. Starter entries report owned and reject unnecessary purchase attempts without spending currency.

Existing profiles need no reset. Previously paid discoveries, currency, partial goals and claimed/pending silver rewards remain intact. Newly free content is recognized by the catalog rule rather than inserted into paid-purchase history. The app refreshes quests at startup so lowered purchase milestones become claimable; credit still requires manual claiming. Purchase milestones keep their historical IDs but now require 3, 7 and 12 purchases, remaining reachable with 12 gated discoveries. Shop milestones themselves no longer require a discovery purchase.

Validation: `ian_quest_tests` checks starter counts, exotic gates, fresh-run offers and drops at every rarity, existing-profile preservation, purchase protection, purchase-milestone reachability and JSON/fallback parity. `ian_unlock_menu_smoke` checks the actual catalog UI in both languages across supported viewport shapes.
