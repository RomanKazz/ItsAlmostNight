# Missile launcher

Integrated defense, append-only BuildingType 16 and RunUpgradeEffect 61. Acquisition and shared upgrades use the four defense slots and the core level cap. One authored visual model is used at all eight stat levels.

## Attack

- Two full-damage rockets per base event. Projectile Count extends the same sequence. Four MUZZLE_01..04 sockets alternate.
- Raised pitch is +30 degrees, with smooth yaw/pitch; idle aim remains where it stopped.
- Guidance eases in with a smoothstep ramp after the first 0.04 s, reaching full strength over 0.42–0.495 s according to the tube. Seeded lateral/vertical offsets fade smoothly over 1.05 s. Exponential heading convergence replaces snapping onto the target direction; a distance-dependent angular cap starts at 3 rad/s at base speed and tightens continuously on approach. Steering response and turn limits scale with projectile speed to keep boosted rockets from orbiting close targets. Constant flight speed and swept capsule contact preserve physical travel time.
- Launch targeting excludes XZ distances at or below 2.5 m. Targets must be in the normal 90–160 degree defense sector; no extra height restriction. Groups are preferred, flying enemies included.
- Fired rockets may reacquire and hit targets inside the blind zone or behind the original sector. After target loss they search nearby once per 0.12 s; no target means coast until 4.5 s expiry.
- One contact detonates one sphere, radius 1.6 × Size. Direct target is excluded from splash, so every enemy receives one full shared hit with independent crit/knockback/item/kill attribution.
- Bounces/Pierce/Bounce Range are inapplicable and filtered centrally. Damage, Attack Speed, Quantity, Projectile Speed, Duration, Size, Crit, Knockback, Range, Homing, Spread and Burst Interval are supported.
- Dynamic spacing is min(baseBurstInterval / sqrt(max(1, attackSpeed)), attackInterval × 0.6 / max(1, floor(quantity)-1)). No overlapping sequences.
- Existing projectile pool, fixed eight-point curved trail, shared triangle batches; no independent smoke allocations or extra per-rocket draw calls. Explosion sounds use the existing three-voice/cooldown budget; launches use a bounded FireShot cue.

## Base balance

Construction: 50 wood, 40 stone, 25 crystals; 180 HP at level 1. Displayed damage/HP use the existing 10× combat display scale.

| Level | Damage / rocket (internal) | Quantity | Event interval, s | Range, m | Speed, m/s | Raw DPS / target |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 2.00 | 2 | 1.800 | 15.0 | 12.0 | 2.222 |
| 2 | 2.45 | 2 | 1.698 | 15.5 | 12.4 | 2.886 |
| 3 | 2.90 | 2 | 1.607 | 16.0 | 12.8 | 3.609 |
| 4 | 3.35 | 2 | 1.525 | 16.5 | 13.2 | 4.392 |
| 5 | 3.80 | 2 | 1.452 | 17.0 | 13.6 | 5.236 |
| 6 | 4.25 | 2 | 1.385 | 17.5 | 14.0 | 6.139 |
| 7 | 4.70 | 2 | 1.324 | 18.0 | 14.4 | 7.102 |
| 8 | 5.15 | 2 | 1.268 | 18.5 | 14.8 | 8.126 |

Base burst spacing 0.18 s; burst duration 0.18 s. Homing 4, spread 1 degree, lifetime 4.5 s, knockback 1.2. Minimum range stays 2.5 m at every level.

## Asset and interfaces

- GraphicsResources::missileLauncherModel; Renderer::drawMissileLauncher uses identical three-part transforms in color/shadow/highlight/raycast/preview passes. Authored UVs and embedded colormap are unchanged.
- AttackProjectileSystem::spawn has an optional initial target (default none, existing callers preserved); blasts() reports one presentation blast per rocket. TowerSystem exposes those blasts to SimulationTowerCombat.
- Existing expandable save arrays accept the previous 16 buildings / 61 upgrades; no old IDs are reordered. Existing blueprint insight reward keys retain the old stride of 16; missile rewards use a separate namespace. Newly added GameEventType::MissileExplosion is appended.
- Runtime GLB: assets/models/buildings/missile_launcher.glb. Source and authored handoff: art/models/missile_launcher.blend and missile_launcher_handoff.md.
- UI icon: assets/ui/building_missile_launcher.png, 256×256 RGBA. Built-in imagegen references missile_launcher_preview.png + building_cannon.png; prompt requested the same four-tube silhouette, faceted orange/slate palette and thick inventory contour on transparent background. Original generated PNG remains in /Users/roman/.codex/generated_images/01a0fed9-7555-7993-9b69-3f98f89b675f/exec-e94c0937-a9e7-4f19-bd3b-7f1b45db1a36.png.

## Validation / playtest

MissileLauncherTests covers gradual launch steering, distributed curvature, 60/120/240 Hz trajectory consistency, interception from all four tubes at 2.6/8/18 m with 6/12/30 m/s speeds, continuous retargeting, homing contact, direct vs splash damage, airborne and incidental close targets, target loss, expiry, pool reuse/reset, tube alternation, dense bursts, raised-launch hit, blind/rear zones, compatibility, eight stat levels, core-limited cards, construction, save/load and stable IDs. MissileLauncherGraphicsChecks verifies the current native loader, three render meshes, four sockets, posed muzzle agreement, raycast, inventory icon, visible model and animated rocket exhaust. Existing combat/progression/render/audio tests remain required.

Playtest the 2.5 m minimum range, two-rocket salvo and full splash damage against groups; these are starting balance values. This validation is automated, not a claim of a manual gameplay playtest.
