# Tesla tower

New defense: BuildingType::TeslaTower = 17, RunUpgradeEffect::UnlockTeslaTower = 62, both append-only. Construction, four defense slots, shared levels 1–8, normal core level cap, shop/debug cards, saves, icon, shadows/highlight/ghost/raycast and range display are connected. The authored base model and embedded colormap are unchanged.

## Attack

Static omnidirectional tower; eight-meter base range, with no rear blind sector. First arc exits the 0.18-meter cyan sphere from its center at Y=1.260, toward the target. Each discharge hits one enemy plus floor(Bounces) additional enemies, choosing the nearest remaining neighbor in 3D within Bounce Range. One enemy can only be hit once per chain. Every target retains full shared Damage, Crit and Knockback, and standard item/status/kill attribution.

Base Bounces = 2, Quantity = 1. Quantity adds independent discharges to one controlled sequence. Spacing is compressed with the existing dynamic burst formula; Attack Speed controls event frequency. Chains never create pooled fake projectile models. Limits on presentation do not reduce damage. Very extreme aggregate batteries defer work after 256 discharges per tick, retaining remaining sequences.

## Compatibility

Supported: Damage, Attack Speed, Quantity, Bounces, Crit Chance/Damage, Knockback, Range, Bounce Range and Burst Interval.

Not applicable: Projectile Speed, Duration, Size, Pierce, Spread, Fan Spread, Prediction or Homing. Compatibility is centralized in DefenseAttackCompatibility and unused modifiers are filtered before resolving final stats.

## Balance

Cost: 40 wood, 30 stone, 20 crystals; level-one health 170 internal. Existing UI combat values use 10× display scale.

| Level | Damage / target | Event interval, s | Range, m | Additional jumps |
|---|---:|---:|---:|---:|
| 1 | 1.20 | 1.200 | 8.00 | 2 |
| 2 | 1.50 | 1.132 | 8.35 | 2 |
| 3 | 1.80 | 1.071 | 8.70 | 2 |
| 4 | 2.10 | 1.017 | 9.05 | 3 |
| 5 | 2.40 | 0.968 | 9.40 | 3 |
| 6 | 2.70 | 0.923 | 9.75 | 3 |
| 7 | 3.00 | 0.882 | 10.10 | 4 |
| 8 | 3.30 | 0.845 | 10.45 | 4 |

Bounce Range 4.5 m; burst spacing 0.12 s; knockback 0.18. Base raw DPS 1 per enemy, up to 3 total if all base chain contacts exist. Playtest clustered DPS and combination of Quantity + Bounces; these are initial balance values.

## Performance

- TowerSystem rebuilds one spatial lightning query per tick only when firing; idle misses search once per 0.25 s.
- LightningTargetCache reuses the nearest 16 neighbors per enemy slot for the current tick. Dead targets/history exclusions are validated on every jump; range/position/ID changes invalidate the entry. Chains needing more candidates fall back to the spatial query. The cache is bounded and allocated before combat.
- Nail retains its existing item damage and jump limit, but nearest-target scanning now uses one lazy spatial query per tick instead of scanning the full enemy list for every jump.
- Nail and Tesla share ChainLightningEffect: at most 96 visible arcs, 18 segments per arc, a soft violet-blue halo, thin tapered cyan/white strokes, short flickering forks, four-point impact flashes and four diamond sparks. One explicit triangle batch replaces per-segment cylinders/spheres/blend-state changes. Depth testing stays enabled, so walls occlude lightning.
- Repeat arcs refresh bounded existing entries and keep flicker animation running. Visual saturation does not evict unrelated fire/explosion effects. Tesla firing does not shake the camera or flash the player crosshair. Audio uses the shared Lightning cue with a cooldown.

## Asset / API

- assets/models/buildings/tesla_tower.glb; art/models/tesla_tower.blend and tesla_tower_handoff.md. Native loader strips one collider and leaves base_mesh + energy_mesh.
- GraphicsResources::teslaTowerModel, Renderer::drawTeslaTower. Energy material retains its cyan palette and stays readable at night.
- TeslaAttack applies the existing shared hit function; presentation receives zero-damage ChainLightningHit alongside the authoritative ProjectileHit, preventing duplicate damage or recursive item procs.
- AppRunSave reads shrine bonus arrays as expandable arrays, so checkpoints from before the appended defense IDs remain readable. Existing blueprint-insight reward keys retain stride 16; new types get separate namespaces.
- assets/ui/building_tesla.png: 256×256 RGBA, built-in imagegen using tesla_tower_preview.png and building_cannon.png references. Original PNG /Users/roman/.codex/generated_images/01a0fed9-7555-7993-9b69-3f98f89b675f/exec-eeaba168-5b3a-4646-97ba-f9c3b5c884ea.png. Prompt requested the authored round orange/slate coil tower with cyan sphere, faceted inventory shading and dark contour on transparent background.

## Checks

TeslaTowerTests covers chain count/flooring, unique targets, shared crit, neighbor reach, compatibility, 360-degree attack, quantity/speed sequences, resets, eight levels, stress battery, cache invalidation and dead targets, core-limited progression/construction/save-load. TeslaGraphicsChecks covers native asset/collider/socket, icon, model, raycast, circle range, animated lightning and wall occlusion. Existing Nail/item tests and all system tests remain required.

Synthetic Debug stress (24 towers, 1000 enemies, Quantity30, AttackSpeed5, 5 jumps): approximately 41 ms/tick before caching, 8 ms/tick after caching on this workstation. This is not an in-game FPS measurement or manual gameplay playtest.

## Final runtime checks

The first isolated drawing checks did not catch a misplaced event branch. AppPresentationEventsSmoke now drives the actual App event handler and App renderer: Tesla/Nail lightning, missile explosions, saw splinters and boss impacts must enqueue independently, and queued arcs must be visible. The handler dispatch regression is fixed in Debug and Release. The smoke entry is compiled only with BUILD_TESTING and runs with isolated temporary settings.

Release synthetic Tesla stress: approximately 1.19 ms/tick (24 towers / 1000 enemies / Quantity30 / AttackSpeed5 / 5 jumps), measured on this workstation.

The final visual pass replaces solid neon bands with thinner tapered strokes, a sideways fading halo, unequal jagged segments, short growing forks and quick impact stars/diamond sparks. The same bounded batch is used for Tesla and Nail.
