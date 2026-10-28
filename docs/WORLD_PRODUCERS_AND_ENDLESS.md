# World producers and continuous night

World landmarks are restored and upgraded with Coins using E. Their floating action pill uses the same keycap, coin-price and insufficient-funds treatment as chests. Hold LMB while aiming to see income per minute and, when another upgrade is available, the next level's income. At maximum level the inactive E keycap is hidden.

## Economy

| Landmark | Restore | Level 1 | Level 2 | Level 3 | Level 4 | Level 5 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Lumber mill | 60 Coins | 60 wood/min | 90 | 120 | 150 | 180 |
| Mine | 80 Coins | 40 stone/min | 60 | 80 | 100 | 120 |

Upgrades cost 50, 75, 110 and 165 Coins. Values above are before class, meta-production and resource-productivity relic bonuses. World landmarks upgrade only through their paid E interaction: ordinary producer/tome cards and building-shrine production bonuses affect player-built producers only. The world pill is anchored 2.8 metres above the landmark base rather than above its roof (6.7 metres). Production runs both day and night, stops on pause and during challenges, and respects resource capacity. Upgrades retain partial cycle progress. These buildings consume no player building slots and cannot be destroyed, giving them a different role from a scalable cluster of ordinary producers. Prices and base output live in `assets/data/economy.json` (array order: mine, lumber mill).

Placed producers' hold-information cards display only income per minute, rather than HP, batch size and cycle time. Regular and modular repairs retain their visual/audio feedback without a repaired-text toast.

## Nights and pressure

After clearing night 10, normal play immediately enters continuous night 11. There is no stage-clear choice, dawn or recurring day. The timeline blends to fully purple over three seconds. The compact HUD below it shows elapsed endless time, live enemy count and a crowd bar (512 active enemies maximum).

Each minute advances the displayed pressure tier and wave-survival quest statistics without removing actors, awarding dawn income or resetting the overtime clock. Boss scheduling continues at the existing cadence. Old completed-stage saves remain loadable; new endless saves resume automatically at the current tier, retaining accumulated overtime and building/relic/economy state. Combat actors respawn on load, as with existing suspended-night checkpoints.

Base active-population targets for nights 1–10 are 8, 14, 22, 32, 45, 60, 78, 85, 110 and 135, with encounter-pattern and within-night adjustments. Endless begins at a base target of 160 and adds 18 per tier. The overtime density multiplier grows by 0.50 per minute, capped at 4×; the existing total actor cap remains in force.

All enemy archetypes gain 2% base movement speed per night, capped at +30%. Endless adds 4% per minute, capped at 1.5×. Applying a new night multiplier preserves existing overtime scaling and does not compound the same bonus on repeated calls; reinforcements and delayed spawns inherit the same current multiplier.

Normal enemy damage growth stops after night 11, then overtime damage adds only 3% of that base per minute (linear). Later boss cycles add 2% damage per cycle. HP keeps its existing stronger growth. Legacy `damageDoublingSeconds` data remains accepted but no longer drives damage.

The debug enemy menu (B) also offers **Start night 11**. It needs a placed Core and no active challenge, keeps the existing base/equipment/resources, clears old enemies and pending choices, and starts continuous night with a fresh pressure clock. It follows the existing debug-hotkey setting.

## Rendering and validation

Paused and upgrade-choice projectiles use the final simulation position rather than the continually changing render interpolation fraction. Fire projectile effects use simulation time. Flamethrower direction interpolation also freezes. Full-rate enemy animation now extends to 40 metres instead of 20.

Focused regression target: `ian_economy_endless_tests` covers prices, output, partial progress, max level, insufficient funds, save round-trip, night speed, the automatic 10→11 transition, uninterrupted tiers, pause and overtime persistence. `IAN_ECONOMY_ENDLESS_CAPTURE=<directory> its_almost_night --test-presentation-events` checks pixel-identical paused projectiles at different interpolation fractions and captures the endless HUD and a held world-producer prompt.
