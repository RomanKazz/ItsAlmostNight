# Alpaking royal court

The existing Boss archetype now uses Quaternius's CC0 `Alpaking_Evolved`
model and native flying / attack animations. His familiars use `Alpaking`.
Both self-contained embedded glTF files are loaded at graphics initialization;
ordinary Flying enemies keep their dragon asset. EnemyType and save IDs stay
unchanged. A familiar is Flying with an optional `summonerId`, not a tenth type.

## Battle

Base stats: 2400 internal HP (24,000 displayed HP via CombatDisplayScale), 2.2 m/s, 30 damage before existing wave scaling. The old
boss had 70 base HP. Phases start at 60% and 30% HP. Arrival takes 1.6 s;
phase transitions take 1 s. The visible crown shield takes 35% incoming damage
through the shared direct / area hit flow. There is no full invulnerability.
Freeze still becomes slow under the existing boss status rule.

The king orbits / pursues the player inside 22 m, otherwise threatens the Core.
Within 19 m he runs one sequence: summon, volley, dive, volley, storm. Phase 1
substitutes another volley for storm. Cooldown is
`2.6 / (1 + 0.32 * (phase - 1))` seconds after releasing an ability.

| Ability | Behaviour |
| --- | --- |
| Royal volley | 0.85 s windup; 5 / 7 / 9 physical shots by phase, 8.5 m/s, 4.2 s lifetime, damage x0.55. Locked aim, physical misses and ordinary collision; no homing or RNG miss. |
| Dive | 1.35 s warning at a locked landing point, 2.7 m radius, damage x1.2. Climbs 3 m and travels during the final 45% of windup. Move out or jump above the 0.75 m ground band. Landing avoids blocking structures before the warning appears. |
| Feather storm | Phases 2/3: three independently surface-aligned marked positions, 1.45 s first windup, 0.3 s between strikes. Radius 2.05 / 2.2 m, damage x0.85. Move out; a small jump does not avoid the 2 m vertical band. |
| Court | 1.15 s windup; 3 / 4 / 5 flying familiars by phase, at most 12 owned living familiars. HP x7, damage x1.4 of ordinary Flying, inheriting the king's wave scaling; speed x1.12. A full court substitutes volley. |

Area impacts hit nearby buildings through the existing modular / conventional
structure damage and defeat pipeline at x0.35 damage. Each target receives
one hit per impact. Godmode and player invulnerability are respected.
King death cancels pending impacts, removes his projectiles and dismisses his
court. Reset clears sequence state. Existing suspended-run loading resets
combat actors; no new serialized runtime fields are needed.

A living or pending boss holds the night open after its timer expires; ordinary
wave spawning stops at expiry. Dawn can no longer silently banish the boss.

## Presentation

Native flapping, headbutt and spell poses; gold / lime unlit cartoon projectiles,
short animated wakes, layered feather impact clouds, orbiting crown shield.
Ground markers read the authoritative sequence timer directly, so a canceled
phase/death cannot leave a hidden delayed impact. Markers have a stable actual
radius with animated inward feathers. Effects obey world depth and reduced
particle / flash settings, and reuse triangle batches without per-frame allocation.
Impact sounds use the existing explosion voice/cooldown budget.

Both health labels use the shared CombatDisplayScale conversion; neither changes
actual health. The HUD uses the game's outlined fonts and beveled shapes: crown badge, gold
border, vertical HP gradient, numeric current/max HP, three phase pips, actual
60%/30% threshold notches and a temporary attack cue. Phase 3 turns coral.
There is no large panel or permanent idle caption.

Physical cylinder envelopes enclose the new animated silhouettes: king radius
3.18 m, height 3.65 m, bottom 0.5 m plus current hover; familiar radius 1.20 m,
height 1.06 m, bottom 1.68 m. The smaller forgiving attack capsule remains
separate. Airborne king bypasses ground structure exclusion until touchdown.

## Validation / tuning

CPU checks cover arrival shield, phase thresholds, shot count and travel,
locked dive targets, three storm warnings / impacts, summon cap, owned cleanup,
reset and night expiry. GPU checks cover native animated silhouettes, moving
royal effects, night readability, world occlusion and low detail.
An ice-wand sweep starting inside the larger boss footprint now counts immediate
contact instead of waiting for an exit that may lie beyond the segment.

Gameplay tuning still needs a full run with a weak and strong build. The values
most likely to need adjustment are 2400 base HP (multiplied by wave growth),
7x familiar HP, 12-summon cap and recovery duration. Automated checks verify
mechanics and presentation, not subjective encounter difficulty.
