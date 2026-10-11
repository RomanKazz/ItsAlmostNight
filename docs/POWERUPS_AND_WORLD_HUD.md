# Temporary powerups, enemy health bars and tome offers

Implemented over the current attack/progression refactors; existing attack IDs and save version stay unchanged.

## Gameplay

Enemy kill rewards can drop one of three equally likely powerup types. Drop chance: ordinary enemies 1%, elites 5%, bosses 20%. Debug mass-defeat does not produce drops. Tokens are collected within 1.4 world units, disappear after 45 seconds, and land on terrain or a supporting platform. At most 32 tokens exist; exhausted slots reject a new token rather than allocating.

| Powerup | Duration | Effect |
| --- | --- | --- |
| Magnet | 6 s | Attract existing and newly dropped XP and coins from any distance; remote hearts remain unchanged. |
| Fury | 15 s | Defense damage ×1.5 and attack speed ×1.5 through the shared attack modifiers. |
| Gold Rush | 20 s | Enemy coin rewards ×3; existing coins, resource production and chest rewards remain unchanged. |

Repeated collection refreshes duration; strength does not stack. Pause freezes timers. Save/resume restores timers and active tokens; new runs and defeat clear them. Missing optional powerup JSON fields in older saves mean an empty state. Invalid/non-finite saved state is rejected.

Initial balance requires a real run: Fury gives approximately ×2.25 raw DPS where attack throughput scales with speed. Drop rates and durations are provisional gameplay values, not claimed Megabonk constants.

## Presentation

Floating colored cartoon tokens share geometric emblems with HUD timer chips. Fury adds sparse purple wisps and sparkles to living defenses, and active buffs add a low-opacity edge tint. Effects respect world depth, disable depth writes while drawing, and cull beyond 65 units. Reduce-flashes settings suppress HUD pulsing and animate Fury less. No world outline or particle texture is required.

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

Integration changes are in Simulation, App gameplay/world overlays, suspended-run JSON, localization, CMake and existing test runners. They overlap substantial pre-existing uncommitted refactors; the implementation commit records new isolated files and leaves mixed existing files in the working tree to preserve that work.

## Validation

Automatic tests cover deterministic drop rates, collection distance, refresh/expiry, bounded reuse, invalid state, pause/resume/reset, magnet exclusions and reward delivery; tome 50/50 sampling, uniform groups and fallback; GPU foreground/background depth clipping at different framebuffer resolutions, SSAO-off depth texture, and powerup presentation.

Debug and sanitizer builds pass; `git diff --check` is clean. Debug full CTest: 17/18 pass. The final logic rerun passes after the platform-support/invalid-enum adjustments. ASan/UBSan full CTest: 16/18 pass; the attack test exceeded its short CTest timeout but passed a subsequent direct sanitizer run (exit 0). The new powerup/tome tests and GPU presentation/occlusion checks pass with sanitizers, without sanitizer diagnostics.

The remaining failure is the pre-existing `ian_run_menus_smoke` assertion `native window retains upgrade names after the SDF cache warms under scissor`, also reproduced in isolated Debug retries. It is outside these gameplay/HUD changes.
