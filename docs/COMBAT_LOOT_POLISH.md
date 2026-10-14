# Combat, exploration rewards and wall cards

## Presentation

- Vanguard's `SHIELD DEPLETED` HUD text is removed.
- Focused enemies, including all elite affixes, use a white selection mask.
  Unfocused elites retain their affix color.
- Musket and GunTurret share decoded rifle PCM but have independent alias pools:
  eight player voices and four defense voices, all created at audio startup.
  Occupied voices are never restarted. A full pool skips extra audio instances.
  The ordinary isolated shot retains its original volume; overlapping shots
  receive lower gain to avoid the previous burst noise.
  Aliases unload before the underlying sample and reset on reinitialization.
- Explosion pressure rings and boss warning circles use `SurfaceEffectPlane`.
  Its orthonormal basis follows the terrain normal. Vertices additionally check
  local terrain height and keep 0.045 m clearance on uneven surfaces.
  Existing render batches are used; no new draw call or per-frame allocation.

## Containers and silver

| Source | XP | Gold XP crystals | Meta silver |
| --- | ---: | ---: | ---: |
| Barrel | 24 | 3 × 8 XP | 0 |
| Crate | 32 | 4 × 8 XP | 0 |
| Gold/item crate | 48 | 6 × 8 XP | 1 |

XP tuning lives in `assets/data/insight.json` and `InsightConfig::defaults`.
`PickupKind::MetaSilver` is appended to the existing enum and collects separately
from run coins. Its visible coin uses the silver palette; value-five ordinary
run coins now use the gold palette to distinguish currencies.

Silver does not expire. Pending silver pickups survive suspended-run saves
with position, velocity, magnet state and unique pickup ID. Invalid/duplicate
states are rejected. Old saves default to no pending silver. Collection credits
the permanent wallet and immediately requests profile persistence. Sandbox
events do not credit permanent currency. At a full pickup pool, the silver
reward is credited directly so it cannot disappear.

Opened gold crates update the active night's checkpoint, including their
normal respawn timer, preventing immediate duplication by resuming the night.
Container respawn and other loot mechanics retain the existing rules.

## Walls

`UnlockWall` is appended as stable run-upgrade ID 60; prior IDs stay the same.
The first card acquires walls and uses one of four defense-type slots. Further
cards raise their common blueprint level, up to the current core level (max 8).
Existing and newly built wall segments share the same health/visual level.
Wall cards are Common, use the existing wall icon and appear in the level-up,
run shop and debug catalogs through the normal blueprint metadata.

Walls move to the defense hotbar, before traps; the construction hotbar contains
foundations, gates, economy buildings, floors and ramps. Drag placement and
placement-cost growth continue to use the existing wall construction path.
Normal runs upgrade walls through cards; direct resource-funded upgrades return
`CardRequired`. Sandbox/god mode retain direct upgrades.

Building arsenal save version is now 4. Legacy constructed walls acquire their
strongest existing level when there is a free defense slot. A legacy run already
holding all four types keeps its existing wall segments; it does not gain a fifth
type slot. New cards use the normal core-level and slot restrictions.

## Validation

Tests cover actual overlapping audio voices, full-pool tail completion,
repeated initialization/shutdown, container XP sums and tiers, silver collection
and restore/rejection, permanent-wallet credit, wall gating/levels/hotbar and
save compatibility. A real graphics comparison checks the boss ring on a 30°
incline; the flat control is visibly clipped while the corrected ring stays above
the plane. Surface-basis tests also check normals and wide uneven-terrain rings.

Public additions: silver pickup save/restore methods and collection fields,
`SurfaceEffectPlane`, and `GameUi::metaShopTexture/shopActionTexture` for the
generated shop icons. New icons and exact prompts are in `assets/ui/shop` and
`art/ui/shop/prompts.json`.
