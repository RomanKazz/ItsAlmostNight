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

The king approaches/orbits the Core whenever it exists. In ordinary battles he
uses a six-action pattern within 28 m: summon, player volley, siege storm,
player dive, Core volley, siege storm. A far-away player cannot redirect the
siege turns. Without a Core he targets the player. Cooldown is now
`1.10 / (1 + 0.32 * (phase - 1))` seconds; movement is x1.35 baseline speed
between actions. Every phase can use a siege storm, including phase one.

| Ability | Behaviour |
| --- | --- |
| Royal volley | 0.65 s windup; 5 / 7 / 9 physical shots, 13 m/s, damage x0.55. Player aim leads the tracked velocity (windup + 65% flight time, capped at 1.35 s); the trajectory then remains physical and does not home. Siege volleys target the Core and apply the structure damage multiplier. |
| Dive | 0.95 s warning, radius 3.3 m, damage x1.2. The point leads 80% of windup along tracked movement and is then locked. Blocking-structure avoidance still resolves before warning. Changing direction or jumping out of the 0.75 m ground band avoids it. |
| Siege storm | 1.10 s first warning, then 0.3 s between three impacts. Radius 2.7 / 2.85 / 3.0 m. First strike marks the Core. Other strikes can cover a distant player, a nearby outer defense or flanking base positions. Player forecast uses 80% of windup. Damage x0.85; 2 m vertical hit band. |
| Court | 0.85 s windup; unchanged 3 / 4 / 5 familiars, limit 12, HP x7, damage x1.4. Full court substitutes a volley. |

Windups additionally scale by `1 - 0.12 * (phase - 1)`; all markers read the
actual duration. Constant-direction running is less safe; there remains a
visible, fixed warning and no unavoidable homing/RNG miss mechanic.

Area impacts hit nearby buildings through the existing modular / conventional
structure damage and defeat pipeline at x0.65 damage. Each target receives
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

The dedicated boss HUD uses the shared CombatDisplayScale conversion; world
health bars remain plain strips without numeric HP. The HUD uses the game's outlined fonts and beveled shapes: crown badge, gold
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

## Aggression regression

A 30-second phase-one fixture circles the player around the king while retaining
an active Core. It requires at least 12 ability starts, five Core hits, four Core
storm warnings, and the first Core hit before 10 seconds. Existing physical-hit,
locked-target, phase, summon limit and death/reset checks remain active. This is
a deterministic regression, not a substitute for tuning a complete played run.
The x0.65 structure multiplier and shorter cooldown need playtesting against
Core HP and well-built defenses, especially low-level or underbuilt bases.
