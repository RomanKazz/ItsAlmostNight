#include "app/App.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/UiLabels.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

const char* lootPromptDescription(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Apple: return "Auto-heal once per night below 35% HP";
    case LootUpgradeEffect::Bread: return "Regenerate outside combat; full HP boosts attack speed";
    case LootUpgradeEffect::IronBar: return "+12 armor; breaking it stuns nearby enemies";
    case LootUpgradeEffect::FuelJerrycan: return "Hits ignite; repeated burns deal more damage";
    case LootUpgradeEffect::Compass: return "Points toward nearby chests";
    case LootUpgradeEffect::Nail: return "Every fifth hit chains lightning";
    case LootUpgradeEffect::Key: return "First chest each day is free";
    case LootUpgradeEffect::Map: return "Reveals chests; boss waves add more";
    case LootUpgradeEffect::Anvil: return "Full repairs release a knockback shockwave";
    case LootUpgradeEffect::Saw: return "Trees launch saw blades when destroyed";
    case LootUpgradeEffect::Potion: return "Low HP triggers Berserk";
    case LootUpgradeEffect::Blueprint: return "First building of each type grants XP";
    case LootUpgradeEffect::Hourglass: return "End the night early for Coins and XP";
    case LootUpgradeEffect::Rope: return "Reduce fall damage; prevent a fatal fall";
    case LootUpgradeEffect::Magnet: return "Attract Coins and hearts from farther away";
    case LootUpgradeEffect::HealthAid: return "Harvesting trees or stone restores health";
    case LootUpgradeEffect::CoalBell: return "Burning kills ignite nearby enemies";
    case LootUpgradeEffect::PowderPouch: return "Critical hits explode";
    case LootUpgradeEffect::PlagueMushroom: return "Hits can poison enemies";
    case LootUpgradeEffect::InfectedPouch: return "Poisoned kills leave a toxic cloud";
    case LootUpgradeEffect::GasMask: return "Nearby poisoned enemies reduce damage taken";
    case LootUpgradeEffect::IcePrism: return "Frozen kills shatter into icy shards";
    case LootUpgradeEffect::TargetingLens: return "Your hits mark targets for defenses";
    default: return lootUpgradeDescription(effect);
    }
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
    const Camera3D& camera) {
    if (snapshot.state == RunState::MainMenu ||
        snapshot.state == RunState::Paused ||
        snapshot.state == RunState::StageClear ||
        snapshot.state == RunState::Victory ||
        snapshot.state == RunState::Defeat ||
        snapshot.playerRespawning || snapshot.runUpgradeChoicePending ||
        snapshot.selectedBuilding || foundationBuildMode_) {
        return std::nullopt;
    }
    if (renderer_->graphicsPanelVisible() ||
        enemySpawnMenuVisible_ || itemGrantMenuVisible_ ||
        coreDefenseMenuVisible_) {
        return std::nullopt;
    }

    const auto finalize = [this, &camera](InteractionPrompt prompt) {
        prompt.occluded = terrainOccludesPrompt(
            camera.position, prompt.worldAnchor, simulation_.terrain());
        return prompt;
    };

    if (snapshot.aimedChallengeColumn) {
        const auto column = std::find_if(
            snapshot.challengeColumns.begin(),
            snapshot.challengeColumns.end(),
            [&snapshot](const ChallengeColumnInstance& value) {
                return value.id == *snapshot.aimedChallengeColumn &&
                       value.state == ChallengeColumnState::Dormant;
            });
        if (column != snapshot.challengeColumns.end()) {
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Challenge,
                .targetId = column->id,
                .worldAnchor = {
                    static_cast<float>(column->position.x),
                    static_cast<float>(column->position.y + 1.72),
                    static_cast<float>(column->position.z),
                },
                .actionText = "CHALLENGE",
                .input = ControlAction::Interact,
                .state = InteractionState::Available,
                .accentColor = Color{230, 188, 147, 255},
            });
        }
    }

    if (snapshot.aimedWorldLandmark) {
        const auto landmark = std::find_if(
            snapshot.worldLandmarks.begin(),
            snapshot.worldLandmarks.end(),
            [&snapshot](const WorldLandmarkInstance& value) {
                return value.id == *snapshot.aimedWorldLandmark &&
                       !value.activated;
            });
        if (landmark != snapshot.worldLandmarks.end()) {
            const auto shrine=landmarkShrineKind(landmark->type);
            if (shrine!=ShrineKind::None) {
                const auto cost=shrineActivationCost(shrine,snapshot.shrines.activations);
                const bool affordable=snapshot.unlimitedResources || canAfford(cost,snapshot.wood,snapshot.stone,snapshot.crystals);
                return finalize(InteractionPrompt{
                    .targetKind=InteractionPromptTargetKind::Landmark,
                    .targetId=landmark->id,
                    .worldAnchor={static_cast<float>(landmark->position.x),static_cast<float>(landmark->position.y+2.8),static_cast<float>(landmark->position.z)},
                    .objectName=shrine==ShrineKind::Player ? "Player Shrine" : "Building Shrine",
                    .actionText=shrine==ShrineKind::Player ? "Player bonus" : "Building bonus",
                    .input=ControlAction::Interact,
                    .state=affordable ? InteractionState::Available : InteractionState::Warning,
                    .cost=cost,
                    .showResourceCost=true,
                    .availableResources=snapshot.unlimitedResources ? cost : ResourceCost{snapshot.wood,snapshot.stone,snapshot.crystals},
                    .recentFailure=invalidActionRemaining_>0,
                    .accentColor=shrine==ShrineKind::Player ? Color{242,123,162,255} : Color{105,203,244,255},
                });
            }
            const bool affordable = snapshot.unlimitedResources ||
                snapshot.coins >= landmark->activationCoinCost;
            ResourceCost cost{};
            cost.crystals = landmark->activationCoinCost;
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Landmark,
                .targetId = landmark->id,
                .worldAnchor = {
                    static_cast<float>(landmark->position.x),
                    static_cast<float>(landmark->position.y + 6.7),
                    static_cast<float>(landmark->position.z),
                },
                .actionText = "Restore",
                .input = ControlAction::Interact,
                .state = affordable ? InteractionState::Available
                                    : InteractionState::Warning,
                .cost = cost,
                .availableCurrency = snapshot.unlimitedResources
                    ? std::nullopt
                    : std::optional<int>{snapshot.coins},
                .recentFailure = invalidActionRemaining_ > 0.0,
                .accentColor = affordable
                    ? Color{224, 184, 92, 255}
                    : Color{214, 108, 96, 255},
            });
        }
    }

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
                    static_cast<float>(visualPosition.y + 0.58),
                    static_cast<float>(visualPosition.z),
                },
                .objectName = lootUpgradeName(loot->loot.effect),
                .actionText = "Pick Up",
                .input = ControlAction::Interact,
                .state = InteractionState::Available,
                .hint = lootPromptDescription(loot->loot.effect),
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
            const int paidOpeningCost = scaledChestOpeningCost(
                chest->coinCost, snapshot.chestOpeningCostSurcharge,
                snapshot.chestOpeningCostMultiplier,
                snapshot.openedPaidChestCount);
            const bool freeWithKey =
                paidOpeningCost > 0 &&
                snapshot.freeChestOpeningAvailable;
            const int openingCost = freeWithKey ? 0 : paidOpeningCost;
            ResourceCost cost{};
            cost.crystals = openingCost;
            const bool affordable = snapshot.unlimitedResources ||
                                    snapshot.coins >= openingCost;
            const Vector3 chestPosition{static_cast<float>(chest->position.x),
                static_cast<float>(chest->position.y),static_cast<float>(chest->position.z)};
            const auto chestTransform=renderer_->lootChestWorldTransform(chest->type,
                chestPosition,static_cast<float>(chest->yaw),0);
            const auto bounds=chestTransform.worldBounds;
            const Vector3 chestAnchor=chestTransform.valid
                ? Vector3{(bounds.min.x+bounds.max.x)*.5F,
                    bounds.max.y+.22F,(bounds.min.z+bounds.max.z)*.5F}
                : Vector3{chestPosition.x,chestPosition.y+1.15F,chestPosition.z};
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Chest,
                .targetId = chest->id,
                .worldAnchor = chestAnchor,
                .surfaceNormal = {
                    static_cast<float>(chest->surfaceNormal.x),
                    static_cast<float>(chest->surfaceNormal.y),
                    static_cast<float>(chest->surfaceNormal.z),
                },
                .actionText = "Open",
                .input = ControlAction::Interact,
                .state = affordable ? InteractionState::Available
                                    : InteractionState::Warning,
                .cost = cost,
                .availableCurrency = snapshot.unlimitedResources
                    ? openingCost : snapshot.coins,
                .recentFailure = invalidActionRemaining_ > 0.0,
                .accentColor = affordable
                    ? Color{214, 203, 181, 255}
                    : Color{214, 108, 96, 255},
            });
        }
    }

    if (snapshot.aimedResource) {
        const auto prop = std::ranges::find(snapshot.resourceNodes, *snapshot.aimedResource, &ResourceNode::id);
        if (prop != snapshot.resourceNodes.end() && prop->active && isDestructibleProp(prop->type)) {
            const Vector3 position{static_cast<float>(prop->position.x),static_cast<float>(prop->position.y),static_cast<float>(prop->position.z)};
            const auto bounds=renderer_->destructiblePropWorldBounds(prop->type,position,
                static_cast<float>(prop->visualYaw),static_cast<float>(prop->visualScale));
            const Vector3 anchor=bounds.max.y>bounds.min.y
                ? Vector3{(bounds.min.x+bounds.max.x)*.5F,bounds.min.y+(bounds.max.y-bounds.min.y)*.65F,(bounds.min.z+bounds.max.z)*.5F}
                : Vector3{position.x,position.y+.45F,position.z};
            return finalize(InteractionPrompt{
                .targetKind=InteractionPromptTargetKind::Resource,
                .targetId=prop->id,
                .worldAnchor=anchor,
                .actionText="Open", .input=ControlAction::Interact,
                .accentColor={255,218,117,255},
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
            if (building->type == BuildingType::Wall ||
                building->type == BuildingType::Gate) {
                const Vec3 anchor = buildingHealthBarWorldAnchor(*building,
                    renderer_->buildingVisualHeight(building->type, building->level));
                return finalize(InteractionPrompt{
                    .targetKind = InteractionPromptTargetKind::Building,
                    .targetId = building->id,
                    .worldAnchor = {static_cast<float>(anchor.x),
                        static_cast<float>(anchor.y), static_cast<float>(anchor.z)},
                    .actionText = building->type == BuildingType::Wall
                        ? "DOOR" : building->open ? "CLOSE" : "OPEN",
                    .input = ControlAction::Interact,
                    .accentColor = {245, 231, 198, 255},
                });
            }
            if (!snapshot.sandboxMode && !snapshot.unlimitedResources)
                return std::nullopt;
            const bool isProducer =
                building->type == BuildingType::CrystalMine ||
                building->type == BuildingType::LumberMill ||
                building->type == BuildingType::Quarry;
            if (isProducer && !snapshot.aimedBuildingUpgradeCost)
                return std::nullopt;
            if (!isProducer &&
                (!snapshot.aimedBuildingUpgradeCost ||
                 building->level >= buildingLevelLimit(building->type)))
                return std::nullopt;
            const std::optional<ResourceCost> cost =
                building->level < buildingLevelLimit(building->type)
                    ? snapshot.aimedBuildingUpgradeCost
                    : std::nullopt;
            const bool affordable = snapshot.unlimitedResources ||
                !cost || canAfford(*cost, snapshot.wood, snapshot.stone, snapshot.crystals);
            const Vec3 anchor = buildingHealthBarWorldAnchor(*building,
                renderer_->buildingVisualHeight(building->type, building->level));
            return finalize(InteractionPrompt{
                .targetKind = InteractionPromptTargetKind::Building,
                .targetId = building->id,
                .worldAnchor = {
                    static_cast<float>(anchor.x),
                    static_cast<float>(anchor.y),
                    static_cast<float>(anchor.z),
                },
                .actionText = cost ? "Upgrade" : "Collect",
                .input = cost ? ControlAction::Upgrade : ControlAction::Interact,
                .state = affordable ? InteractionState::Available : InteractionState::Warning,
                .cost = cost,
                .showResourceCost = true,
                .availableResources = snapshot.unlimitedResources
                    ? cost.value_or(ResourceCost{})
                    : ResourceCost{snapshot.wood, snapshot.stone, snapshot.crystals},
                .accentColor = {0, 241, 123, 255},
            });
        }
    }

    if (snapshot.aimedModularBuilding) {
        const bool buildingManagementActive =
            !snapshot.selectedBuilding;
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
                .actionText = buildingManagementActive
                    ? frame->storey == 0
                        ? "Repair Foundation"
                        : "Repair Floor"
                    : "Repair",
                .input = ControlAction::Repair,
                .state = buildingManagementActive
                    ? InteractionState::Available
                    : InteractionState::Unavailable,
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
                .actionText = "Repair Wall",
                .input = ControlAction::Repair,
                .state = buildingManagementActive
                    ? InteractionState::Available
                    : InteractionState::Unavailable,
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
                .actionText = "Repair Ramp",
                .input = ControlAction::Repair,
                .state = buildingManagementActive
                    ? InteractionState::Available
                    : InteractionState::Unavailable,
                .accentColor = {238, 229, 207, 255},
            });
        }
    }

    // Basic attacks are already communicated by the equipped weapon and
    // crosshair. Enemy identity and health live in one target widget instead
    // of competing with a redundant interaction prompt.
    return std::nullopt;
}

} // namespace ian
