# One-use run shrines

New runs generate 12 Player Shrines and 12 Building Shrines at seeded, safe sites, alongside the existing mine and lumber mill. Each shrine can be paid for once with E. The purchase pauses gameplay and opens the existing three-card upgrade overlay. Choose one blessing; it lasts for the run and never owns/levels a tome, consumes a slot, unlocks a blueprint, or changes normal tome offer eligibility. The consumed altar becomes dim and stops being interactive.

## Costs

Both types share the count of paid activations, `n` (0–23). Player: `(20+8n) wood, (12+5n) stone, (4+2n) crystals`. Buildings: `(25+8n), (15+5n), (5+2n)`. The world prompt uses the same wood/stone/crystal icon-and-amount row as building upgrades, with affordable amounts light and missing resources red, and a compact E action in the same panel. No coin cost is shown. Payment is atomic; insufficient resources preserve the altar and counter. Godmode bypasses payment. Three distinct common-strength bonuses are drawn uniformly from the relevant pool, independently of owned tomes. Paid offers cannot use normal reroll tokens. Unfinished offers persist through save/load with their exact choices and source shrine.

## Tome effects

Names, icons and ordinary descriptions reuse the active tome catalog, not a separate upgrade family. Each blessing grants one ordinary/common tome power, scoped to the appropriate side. Might and Rapid grant only their player or defense half. No retired tomes, unlocks, weapon/blueprint levels, shop consumables, or slot upgrades enter the pool.

Player (11): Might (+10% damage), Rapid (+15% weapon speed), Swift (+7% movement), Vitality (+12 HP), Guard (+8 armor), Fortune (+10% luck), Regen (+0.5 HP/s), Vampiric (+8 HP per 10 kills), Gathering (+15% tool speed), Capacity (+20% world resource node stock/durability), Venom (+10 percentage points poison chance for player hits).

Buildings (14): Might (+12% defense damage), Rapid (+10% defense attack speed), Fortitude (+12% structure HP), Industry (+15% producer speed), Volley (+1 quantity), Bounce (+1), Piercing (+1), Velocity (+20% projectile speed), Duration (+20%), Size (+15%), Precision (+5 percentage points crit chance), Ferocity (+25% crit damage), Impact (+20% knockback), Venom (+10 percentage points poison chance for defense hits).

Repeated shrine percentages add within the shrine source, which multiplies with other independent attack sources. Flat counters add. Passive luck, regeneration, gathering, capacity, healing and poison feed their existing tome consumers. Defense bonuses feed the shared AttackStats bindings; physical travel, individual pierce/bounce counters, pooling and weapon compatibility remain unchanged. Building Venom never poisons player hits; Player Venom never poisons defense hits. A normal Venom tome still benefits both.

## State and compatibility

`ShrineRunState` stores separate player/building counters, total paid activations and an optional pending source/kind. Save fields are additive. State validation checks pool membership, totals, per-type limits, matching used altar counts, and the pending source. The activation frame finishes already-produced loot/XP rewards before combat pauses; queued level rewards resume after the paid shrine choice. Restart clears all shrine state.

Old two-landmark maps regenerate their historical challenge/scenery layout and resume with two landmarks, avoiding new shrines through existing bases. New runs get all 24. Existing enum/save IDs are retained; shrine landmark types are appended.

## Assets and verification

- Runtime models: `assets/models/world_landmarks/player_shrine.glb`, `building_shrine.glb`.
- Current packed editable Blender project, previews, manifest and geometry report: `art/models/shrines/rework/`. The earlier source remains in `art/models/shrines/` for comparison.
- Current builder/validator/renderer: `tools/refine_shrine_models.py`, `validate_refined_shrines.py`, `render_refined_shrines.py`. The scripts without `refined`/`refine` describe the retired column design.
- Original defense colormap is embedded byte-for-byte, SHA256 `cbd2f95976ed3f18070769ce32a819527d9bc00d74253844e3882f747b5d76b6`.
- Woodland redesign: Player has a split hewn-stone arch, inset ruby heart and amethyst votives; Building has an iron-banded oak frame and bronze gear with sapphire hub. Both use stepped stone, wood and thick fasteners from the existing palette. Geometry is mirrored left/right and front/back: paired steps, badges, crystals and fasteners, central symbols, no protruding rear braces or asymmetric offerings.
- Neutral floor pivot, GLB Y-up, authored/runtime height 2.8 m, two meshes and one material per model. Player: 2,732 triangles; Building: 3,320 triangles. Both fit inside the existing 2×2 m footprint. Static collision box: centered 2×2 m, height 2.8 m; hover raycast uses visible geometry.
- Closed outward geometry, finite UVs, unit exported normals and emblem clearance are validated. Tests exercise real loader, world shader, raycasts, consumed tint and upgrade UI. These automated renders are not a native gameplay playtest.
- `ian_shrine_tests`: seeded natural/flat maps, resource payment/rejection, three unique choices, all tome bonuses, full slots, scope isolation and real poison hits, save/load, all 24 purchases, increasing costs, reset and corruption rejection.
