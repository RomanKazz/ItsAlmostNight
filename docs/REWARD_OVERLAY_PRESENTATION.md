# Shop and level-up presentation

The between-wave run shop now enters over 0.18 s with a 40%-to-full scale
reveal, shared smooth fade of plates/icons/text and a small damped scale
overshoot. Input still uses the existing release/quiet barrier (0.35 s);
hover previews/tooltips are suppressed until the visible layout has settled.
Final button hit areas, offer eligibility, locks and godmode controls stay intact.

The shop has 36 decorative coins behind its opaque panels. They use the existing
UI coin texture, downward rain (230–302 px/s at 1080p), individual depth/size and
face-width rotation. Warm amber rays animate behind the shop cards; level-up
rays keep their existing pale cream palette.
Level-up choice overlays have 36 cyan faceted XP crystals with white glints,
matching the game's XP palette without a heavy world outline. Decorations use
independent deterministic hash channels for X/Y/depth/phase, avoiding rows or
regular diagonal patterns. Both backgrounds fall almost vertically at varied speeds; edge fade removes
wraparound pops. See SHOP_WAVE_PRICING.md for the updated economy.

The shop's presentation clock advances while the game is paused for shopping,
resets on each opening and clears on run reset/close. Level-up decorations use
its existing entrance/exit clock. Reduced-flash accessibility freezes decoration
motion and omits the shop overshoot. Background decorations never capture input.

Geometry uses the dedicated solid white texture for procedural crystal facets,
avoiding font-atlas sampling artefacts. There are no particle allocations,
new bitmap assets, shaders or simulation/save changes.

Public additions: optional entrance-time/reduced-motion arguments to
`drawRunShopOverlay`, optional reduced-motion argument to `drawRunUpgradeOverlay`,
`RunShopOverlayEntranceDuration`, and the code-native `reward_backdrop::draw`.
Default shop arguments keep existing callers fully visible.

Visual smoke checks cover hidden/intermediate/settled shop frames, moving coin
and crystal backgrounds, reduced-motion stability, zero-opacity output, existing
English/Russian labels, viewport fitting, prices and godmode card state.
