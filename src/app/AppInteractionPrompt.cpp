#include "app/App.hpp"

#include "app/AppRenderSupport.hpp"
#include "graphics/WorldTransforms.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/UiLabels.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

const char* recommendedTool(ResourceType type) {
    if (isDestructibleProp(type)) return "Any tool";
    return type == ResourceType::Wood ? "Axe" : "Pickaxe";
}

bool matchingResourceTool(PlayerWeapon weapon, ResourceType type) {
    if (isDestructibleProp(type)) return true;
    return weapon == PlayerWeapon::BareHands ||
           (type == ResourceType::Wood && weapon == PlayerWeapon::Axe) ||
           (type == ResourceType::Stone && weapon == PlayerWeapon::Pickaxe);
}

const char* enemyName(EnemyType type) {
    switch (type) {
    case EnemyType::Basic: return "Basic";
    case EnemyType::Fast: return "Fast";
    case EnemyType::Heavy: return "Heavy";
    case EnemyType::Boss: return "Boss";
    case EnemyType::Ranged: return "Ranged";
    case EnemyType::Sapper: return "Sapper";
    case EnemyType::Flying: return "Flying";
    case EnemyType::Splitter: return "Splitter";
    case EnemyType::Splitling: return "Splitling";
    }
    return "Enemy";
}

Vector3 resourcePromptAnchor(
    const ResourceNode& resource, const TerrainHeightfield& terrain) {
    const double ground = terrain.getHeight(
        resource.position.x, resource.position.z);
    const double height = resource.type == ResourceType::Wood
        ? 1.95 * resource.visualScale
        : isDestructibleProp(resource.type)
            ? 0.82 * resource.visualScale + 0.58
            : 1.45 * resource.visualScale;
    return {
        static_cast<float>(resource.position.x),
        static_cast<float>(ground + height),
        static_cast<float>(resource.position.z),
    };
}

bool terrainOccludesPrompt(
    Vector3 cameraPosition, Vector3 anchor,
    const TerrainHeightfield& terrain) {
    // Prompt composition happens after the 3D pass, so use the same
    // heightfield ray as gameplay to reject anchors hidden behind a ridge.
    const Vector3 offset = Vector3Subtract(anchor, cameraPosition);
    const float distance = Vector3Length(offset);
    if (distance <= 0.25F) {
        return false;
    }
    const Vector3 direction = Vector3Scale(offset, 1.0F / distance);
    const auto hit = terrain.raycast(
        {cameraPosition.x, cameraPosition.y, cameraPosition.z},
        {direction.x, direction.y, direction.z},
        std::max(0.0, static_cast<double>(distance) - 0.12));
    if (!hit) {
        return false;
    }
    const Vector3 terrainHit{
        static_cast<float>(hit->x),
        static_cast<float>(hit->y),
        static_cast<float>(hit->z),
    };
    return Vector3Distance(cameraPosition, terrainHit) + 0.12F <
           distance;
}

} // namespace

std::optional<InteractionPrompt> App::buildInteractionPrompt(
    const SimulationSnapshot& snapshot,
    const Camera3D& camera) const {
    if (snapshot.state == RunState::MainMenu ||
        snapshot.state == RunState::Paused ||
        snapshot.state == RunState::Defeat ||
        snapshot.playerRespawning ||
        snapshot.selectedBuilding || foundationBuildMode_) {
        return std::nullopt;
    }
    if (skillTree_.isOpen() || renderer_->graphicsPanelVisible() ||
        enemySpawnMenuVisible_) {
        return std::nullopt;
    }

    const auto finalize = [this, &camera](InteractionPrompt prompt) {
        prompt.occluded = terrainOccludesPrompt(
            camera.position, prompt.worldAnchor, simulation_.terrain());
        return prompt;
    };

    if (snapshot.aimedLoot) {
        const auto loot = std::find_if(
            snapshot.lootChests.begin(), snapshot.lootChests.end(),
            [&snapshot](const LootChestInstance& chest) {
                return chest.loot.id == *snapshot.aimedLoot &&
                       chest.loot.available && !chest.loot.collected;
        });
        if (loot != snapshot.lootChests.end()) {
            const Vec3 visualPosition = lootVisualPosition(*loot);
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Loot,
                .targetId = loot->loot.id,
                .worldAnchor = {
                    static_cast<float>(visualPosition.x),
                    static_cast<float>(visualPosition.y + 0.52),
                    static_cast<float>(visualPosition.z),
                },
                .objectName = lootUpgradeName(loot->loot.effect),
                .actionText = "Pick Up",
                .input = ControlAction::Interact,
                .state = InteractionState::Available,
                .hint = keyboardKeyName(controlKey(
                    userSettings_.controls, ControlAction::Interact)) +
                    " TAKE · " +
                    (loot->looseLoot
                         ? std::string("CRATE DROP")
                         : loot->rerollCount > 0U
                             ? std::string("REROLL USED")
                             : keyboardKeyName(controlKey(
                                   userSettings_.controls,
                                   ControlAction::Upgrade)) +
                                   " REROLL · " +
                                   std::to_string(
                                       snapshot.chestRerollCoinCost) +
                                   " COINS"),
                .accentColor = loot->loot.rarity == LootRarity::Legendary
                    ? Color{255, 126, 38, 255}
                    : loot->loot.rarity == LootRarity::Rare
                    ? Color{255, 170, 170, 255}
                    : loot->loot.rarity == LootRarity::Uncommon
                        ? Color{255, 228, 148, 255}
                        : Color{185, 225, 255, 255},
            });
        }
    }

    if (snapshot.aimedChest) {
        const auto chest = std::find_if(
            snapshot.lootChests.begin(), snapshot.lootChests.end(),
            [&snapshot](const LootChestInstance& value) {
                return value.id == *snapshot.aimedChest &&
                       value.state == LootChestState::Closed;
        });
        if (chest != snapshot.lootChests.end()) {
            const int openingCost = std::max(
                1,
                static_cast<int>(std::lround(
                    static_cast<double>(chest->coinCost) *
                    snapshot.chestOpeningCostMultiplier)));
            ResourceCost cost{};
            cost.crystals = openingCost;
            const bool affordable = snapshot.unlimitedResources ||
                                    snapshot.coins >= openingCost;
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Chest,
                .targetId = chest->id,
                .worldAnchor = {
                    static_cast<float>(chest->position.x),
                    static_cast<float>(chest->position.y + 1.42),
                    static_cast<float>(chest->position.z),
                },
                .actionText = std::string("Open ") +
                    (chest->type == LootChestType::Stone
                         ? "Stone Chest"
                         : "Wooden Chest"),
                .input = ControlAction::Interact,
                .state = affordable ? InteractionState::Available
                                    : InteractionState::Warning,
                .cost = cost,
                .availableCurrency = snapshot.unlimitedResources
                    ? std::nullopt
                    : std::optional<int>{snapshot.coins},
                .recentFailure = invalidActionRemaining_ > 0.0,
                .accentColor = affordable
                    ? Color{214, 203, 181, 255}
                    : Color{214, 108, 96, 255},
            });
        }
    }

    if (snapshot.aimedResource) {
        const auto resource = std::find_if(
            snapshot.resourceNodes.begin(), snapshot.resourceNodes.end(),
            [&snapshot](const ResourceNode& value) {
                return value.active &&
                       value.id == *snapshot.aimedResource;
            });
        if (resource != snapshot.resourceNodes.end()) {
            const bool matching = matchingResourceTool(
                snapshot.selectedWeapon, resource->type);
            const bool warning = !matching;
            const int efficiencyPercent = static_cast<int>(
                std::lround(
                    snapshot.aimedResourceEfficiency * 100.0));
            const bool held = InputKeycap::held(
                userSettings_.controls, ControlAction::Attack);
            float progress = 0.0F;
            if (toolSwingRemaining_ > 0.0 &&
                toolSwingDuration_ > 0.0) {
                progress = static_cast<float>(std::clamp(
                    1.0 - toolSwingRemaining_ /
                              toolSwingDuration_,
                    0.0, 1.0));
            } else if (pendingPickaxe_) {
                progress = 1.0F;
            }
            const bool progressActive =
                held || toolSwingRemaining_ > 0.0 ||
                toolSwingQueued_ || pendingPickaxe_;
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Resource,
                .targetId = resource->id,
                .worldAnchor = resourcePromptAnchor(
                    *resource, simulation_.terrain()),
                .actionText = resource->type == ResourceType::Wood
                    ? "Gather Wood"
                    : resource->type == ResourceType::Stone
                        ? "Mine Stone"
                        : resource->type == ResourceType::Barrel
                            ? "Break Barrel"
                            : "Break Crate",
                .input = ControlAction::Attack,
                .state = warning ? InteractionState::Warning
                                 : InteractionState::Available,
                .hint = warning
                    ? std::optional<std::string>{
                          std::to_string(efficiencyPercent) +
                          "% efficiency  ·  " +
                          recommendedTool(resource->type) +
                          " recommended"}
                    : snapshot.selectedWeapon == PlayerWeapon::BareHands
                        ? std::optional<std::string>{
                              std::to_string(efficiencyPercent) +
                              "% efficiency  ·  " +
                              recommendedTool(resource->type) +
                              " recommended"}
                        : std::nullopt,
                .progress = progress,
                .showProgress = progressActive,
                .recentSuccess = crosshairHitRemaining_ > 0.0,
                .accentColor = warning
                    ? Color{222, 169, 77, 255}
                    : Color{238, 229, 207, 255},
            });
        }
    }

    if (snapshot.aimedBuilding) {
        const auto building = std::find_if(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [&snapshot](const BuildingInstance& value) {
                return value.id == *snapshot.aimedBuilding;
            });
        if (building != snapshot.buildings.end()) {
            const bool gate = building->type == BuildingType::Gate;
            const Vec3 anchor = buildingHealthBarWorldAnchor(*building);
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Building,
                .targetId = building->id,
                .worldAnchor = {
                    static_cast<float>(anchor.x),
                    static_cast<float>(anchor.y + 0.42),
                    static_cast<float>(anchor.z),
                },
                .actionText = gate
                    ? "Toggle Gate"
                    : std::string("Select ") +
                          std::string(buildingDisplayName(
                              building->type)),
                .input = gate ? ControlAction::Interact
                              : ControlAction::Attack,
                .state = InteractionState::Available,
                .recentFailure = invalidActionRemaining_ > 0.0,
                .accentColor = {238, 229, 207, 255},
            });
        }
    }

    if (snapshot.aimedModularBuilding) {
        const EntityId target = *snapshot.aimedModularBuilding;
        if (const auto frame = std::find_if(
                snapshot.platformFrames.begin(),
                snapshot.platformFrames.end(),
                [target](const PlatformFrameInstance& value) {
                    return value.id == target;
                }); frame != snapshot.platformFrames.end()) {
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::ModularBuilding,
                .targetId = frame->id,
                .worldAnchor = {
                    static_cast<float>(
                        (frame->anchor.x + PlatformFrameWidthCells * 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(frame->floorHeight + 1.0),
                    static_cast<float>(
                        (frame->anchor.z + PlatformFrameWidthCells * 0.5) *
                        snapshot.worldCellSize),
                },
                .actionText = frame->storey == 0
                    ? "Select Foundation"
                    : "Select Floor",
                .input = ControlAction::Attack,
                .state = InteractionState::Available,
                .accentColor = {238, 229, 207, 255},
            });
        }
        if (const auto wall = std::find_if(
                snapshot.modularWalls.begin(), snapshot.modularWalls.end(),
                [target](const WallInstance& value) {
                    return value.id == target;
                }); wall != snapshot.modularWalls.end()) {
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::ModularBuilding,
                .targetId = wall->id,
                .worldAnchor = {
                    static_cast<float>(
                        (wall->anchor.x + 0.5) * snapshot.worldCellSize),
                    static_cast<float>(wall->topHeight + 0.92),
                    static_cast<float>(
                        (wall->anchor.z + 0.5) * snapshot.worldCellSize),
                },
                .actionText = "Select Wall",
                .input = ControlAction::Attack,
                .state = InteractionState::Available,
                .accentColor = {238, 229, 207, 255},
            });
        }
        if (const auto ramp = std::find_if(
                snapshot.ramps.begin(), snapshot.ramps.end(),
                [target](const RampInstance& value) {
                    return value.id == target;
                }); ramp != snapshot.ramps.end()) {
            const bool alongZ = ramp->rotation == Rotation::Deg0 ||
                                ramp->rotation == Rotation::Deg180;
            const int widthCells = alongZ
                ? ModularRampWidthCells : ModularRampRunCells;
            const int depthCells = alongZ
                ? ModularRampRunCells : ModularRampWidthCells;
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::ModularBuilding,
                .targetId = ramp->id,
                .worldAnchor = {
                    static_cast<float>(
                        (ramp->anchor.x + widthCells * 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(ramp->topHeight + 0.92),
                    static_cast<float>(
                        (ramp->anchor.z + depthCells * 0.5) *
                        snapshot.worldCellSize),
                },
                .actionText = "Select Ramp",
                .input = ControlAction::Attack,
                .state = InteractionState::Available,
                .accentColor = {238, 229, 207, 255},
            });
        }
    }

    if (snapshot.aimedEnemy) {
        const auto enemy = std::find_if(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [&snapshot](const EnemyInstance& value) {
                return value.active && value.id == *snapshot.aimedEnemy;
            });
        if (enemy != snapshot.enemies.end()) {
            Vector3 position = app_detail::enemyRenderPosition(*enemy);
            position.y += static_cast<float>(
                simulation_.terrain().getHeight(
                    enemy->position.x, enemy->position.z));
            const BoundingBox bounds = renderer_->enemyWorldBounds(
                app_detail::enemyModelVisual(enemy->type), position,
                static_cast<float>(enemy->yaw),
                app_detail::enemyVisualScale(enemy->type));
            const bool hasBounds =
                world_transforms::finite(bounds);
            const Vector3 worldAnchor = hasBounds
                ? Vector3{
                      (bounds.min.x + bounds.max.x) * 0.5F,
                      bounds.max.y + 0.42F,
                      (bounds.min.z + bounds.max.z) * 0.5F}
                : Vector3{
                      position.x, position.y + 1.66F,
                      position.z};
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Enemy,
                .targetId = enemy->id,
                .worldAnchor = worldAnchor,
                .actionText = std::string("Attack ") +
                    enemyName(enemy->type),
                .input = ControlAction::Attack,
                .state = InteractionState::Available,
                .accentColor = {238, 229, 207, 255},
            });
        }
    }
    return std::nullopt;
}

} // namespace ian
