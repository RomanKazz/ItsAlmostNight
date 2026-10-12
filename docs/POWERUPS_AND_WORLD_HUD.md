# Temporary powerups, enemy health bars and tome offers

Implemented over the current attack/progression refactors; existing attack IDs and save version stay unchanged.

## Gameplay

Enemy kill rewards can drop one of three equally likely powerup types. Drop chance: ordinary enemies 0.5%, elites 2.5%, bosses 10%. Debug mass-defeat does not produce drops. Tokens are collected within 1.4 world units, disappear after 45 seconds, and land on terrain or a supporting platform. At most 32 tokens exist; exhausted slots reject a new token rather than allocating.

| Powerup | Duration | Effect |
| --- | --- | --- |
| Magnet | 6 s | Attract existing and newly dropped XP and coins from any distance; remote hearts remain unchanged. |
| Fury | 15 s | Defense damage ×1.5 and attack speed ×1.5 through the shared attack modifiers. |
| Gold Rush | 20 s | Enemy coin rewards ×3; existing coins, resource production and chest rewards remain unchanged. |

Repeated collection refreshes duration; strength does not stack. Pause freezes timers. Save/resume restores timers and active tokens; new runs and defeat clear them. Missing optional powerup JSON fields in older saves mean an empty state. Invalid/non-finite saved state is rejected.

Initial balance requires a real run: Fury gives approximately ×2.25 raw DPS where attack throughput scales with speed. Drop rates and durations are provisional gameplay values, not claimed Megabonk constants.

## Presentation

Floating authored 3D pickups replace flat icon cards: a cyan horseshoe magnet, a violet lightning bolt and a golden pouch with coins. See [pickup models](POWERUP_PICKUP_MODELS.md) for assets and the generator. HUD timer chips retain their geometric emblems. Fury surrounds living defenses with two flowing violet/lavender energy filaments, a soft haze, rising orbiting sparks with curved wakes, and sparse glints. Vertex alpha gives the ribbons and glow soft edges without particle textures or square billboards. Each building has an independent phase; aura height follows its upgraded model. Activation eases in over 0.4 seconds, expiry fades over the final second. No large flame silhouettes or ground circles remain. Active buffs retain the low-opacity HUD edge tint.

Solid pickups write depth. Energy effects test world depth without writing it, and cull beyond 65 units with a distance fade and fewer sparks at long range. Reduce-flashes settings disable glow/glint pulsing while preserving continuous ribbon and spark motion. Animation uses simulation time so pause freezes it. The drawing uses fixed loops and stack values, with no per-frame particle allocations or persistent effect state. Buff strengths and durations remain unchanged.

## Debug grants

Open **Shift + F5 → POWERUPS**, select Magnet/Fury/Gold Rush, then press **ACTIVATE POWERUP** or Enter. Activation is immediate, requires no pickup, and refreshes duration on repeated grants. The pane uses the HUD icons, supports localized search, and displays active time. Copies/rarity and hidden permanent-upgrade selections do not affect powerup activation.

`Simulation::grantDebugPowerup(PowerupType)` validates the type/run state, refreshes Fury modifiers immediately, and invalidates the snapshot. `DebugUpgradeGrant::powerup` carries the request through the existing App grant queue. No RunUpgrade IDs or powerup save fields change. Menu/API integration lives in `src/ui/DebugUpgradeMenu.hpp`, `src/app/AppUpdate.cpp`, `src/game/Simulation.hpp`, `src/game/SimulationProgression.cpp` and localization; these mixed files remain in the working tree alongside earlier refactors.

## Enemy health bar occlusion

Enemy health bar pixels compare their projected depth with the rendered world depth. Trees, buildings and other opaque foreground objects hide the covered pixels; objects behind a bar do not. Text inside the aimed enemy bar uses the same world-space clipping. Building/resource labels retain their previous overlay behavior.

The depth sample works with SSAO on or off and with scaled scene rendering. Non-SSAO offscreen targets now use a texture-backed depth attachment. Native rendering without an offscreen target falls back to hardware depth testing. There is no CPU depth readback or per-enemy occlusion raycast. Transparent effects that do not write depth cannot occlude bars.

## Tome offers

For level-up offers, all eligible non-retired tome types have equal weight. Once an offer is selected from the tome category, the owned/new groups each get 50% if both have eligible members. Selection inside that group is uniform, independent of tome level. If either group is unavailable, use the other. At full tome slots, only owned tomes are eligible.

This is the user's chosen 50/50 policy, not a verified Megabonk probability. Blueprint/other-card category weights, duplicate filtering and slot restrictions remain intact. The shop retains its existing reroll/anti-repeat policy.

## Public interfaces and files

- `economy/PowerupSystem.hpp`: `PowerupType`, definitions, pickup/run state and fixed-capacity timer/drop/collection runtime.
- `SimulationSnapshot::powerups`, `SuspendedRunState::powerups`: presentation and save state.
- `CoinPickupSystem::magnetizeExperienceAndCoins()`: reuse existing pickup attraction.
- `chooseRunUpgradeOffer(...)`: deterministic owned/new group selection over an already eligible pool.
- `Renderer::worldUiOcclusion()`, `WorldUiOcclusion`, optional `TargetHealthBar::draw` parameter: pass depth/shader context into enemy bars.
- `RenderTextureResource::loadWithDepthTexture(...)`: sampleable color/depth framebuffer without SSAO buffers.
- `app/PowerupDrawing.hpp`: world tokens, defense aura and HUD.
- `assets/shaders/world_ui_occlusion.fs`: fragment depth clipping.

Integration changes are in Simulation, App gameplay/world overlays, suspended-run JSON, localization, CMake and existing test runners. They overlap substantial pre-existing uncommitted refactors; the implementation commits record isolated powerup files and leave mixed existing files in the working tree to preserve that work.

## Validation

Automatic tests cover deterministic drop rates, collection distance, refresh/expiry, bounded reuse, invalid state, pause/resume/reset, magnet exclusions and reward delivery; tome 50/50 sampling, uniform groups and fallback; GPU foreground/background depth clipping at different framebuffer resolutions, SSAO-off depth texture, and powerup presentation.

The Fury presentation pass rebuilds Debug and Release without new compiler warnings; `git diff --check` is clean. Full Debug CTest: **17/18**; Release GPU presentation/occlusion suite: **1/1**. The linker still reports the existing duplicate `libian_simulation.a` warning. The preceding gameplay/debug-grant pass also passed **17/18 in ASan/UBSan** without sanitizer diagnostics; sanitizers were not rerun for this drawing-only pass. All powerup/tome tests and new Fury GPU checks pass. Debug-grant tests cover all three types, immediate activation, repeated Fury without compounding, invalid enum/run state, tab access, mouse/Enter activation, hidden copy controls and isolation from selected permanent upgrades. English landscape and Russian portrait menu captures were inspected. Fury GPU checks additionally verify movement between frames, full expiry, final-second fade, continued movement with Reduce flashes enabled and foreground occlusion; multi-frame captures use real turret, cannon and catapult models.

The remaining failure is the pre-existing `ian_run_menus_smoke` assertion `native window retains upgrade names after the SDF cache warms under scissor`, also reproduced in isolated Debug retries. It is outside these gameplay/HUD changes.
