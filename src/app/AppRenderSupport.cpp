#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"

#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace ian::app_detail {

namespace {

constexpr float LootVisualScaleMultiplier = 1.20F;

struct GroundSectorVertex {
    Vector3 position{};
    unsigned char opacity{};
};

struct GroundSectorCacheEntry {
    Vector3 center{};
    float radius{};
    float innerRadius{};
    float yawRadians{};
    float arcDegrees{};
    bool valid{};
    std::vector<GroundSectorVertex> triangles;
};

[[nodiscard]] bool nearlyEqual(float left, float right) {
    return std::abs(left - right) <= 0.0005F;
}

[[nodiscard]] bool matchesGroundSector(
    const GroundSectorCacheEntry& entry, Vector3 center,
    float radius, float yawRadians, float arcDegrees,
    float innerRadius) {
    return entry.valid &&
        nearlyEqual(entry.center.x, center.x) &&
        nearlyEqual(entry.center.y, center.y) &&
        nearlyEqual(entry.center.z, center.z) &&
        nearlyEqual(entry.radius, radius) &&
        nearlyEqual(entry.innerRadius, innerRadius) &&
        nearlyEqual(entry.yawRadians, yawRadians) &&
        nearlyEqual(entry.arcDegrees, arcDegrees);
}

} // namespace

float spikeTrapAnimationSeconds(
    const SimulationSnapshot& snapshot, EntityId id) {
    const auto runtime = std::ranges::find(
        snapshot.traps, id, &TrapRuntime::buildingId);
    if (runtime == snapshot.traps.end() ||
        runtime->activationRemaining <= 0.0) {
        return -1.0F;
    }
    return static_cast<float>(
        TrapSystem::SpikeAnimationDuration -
        runtime->activationRemaining);
}

LootItemVisual lootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest) {
    const float reveal = static_cast<float>(
        std::clamp(chest.loot.revealProgress, 0.0, 1.0));
    const float eased = 1.0F - std::pow(1.0F - reveal, 3.0F);
    const float riseSpin = reveal * reveal * (3.0F - 2.0F * reveal);
    const Vec3 position = lootVisualPosition(chest);
    LootItemVisual visual{
        .position = {
            static_cast<float>(position.x),
            static_cast<float>(position.y),
            static_cast<float>(position.z)},
        .rotation = static_cast<float>(snapshot.elapsedSeconds) * 1.65F +
            static_cast<float>(chest.id.index) * 0.73F +
            riseSpin * PI,
        .scale = LootVisualScaleMultiplier *
            (1.50F * (0.35F + eased * 0.65F) +
             std::sin(reveal * PI) * 0.24F),
    };
    if (chest.rerolling) {
        const float progress = static_cast<float>(
            std::clamp(chest.rerollProgress, 0.0, 1.0));
        const float outgoing = std::clamp(
            progress * 2.0F, 0.0F, 1.0F);
        const float transitionEased = outgoing * outgoing * outgoing *
            (outgoing * (outgoing * 6.0F - 15.0F) + 10.0F);
        visual.rotation += transitionEased * PI * 2.0F;
        visual.scale *= 1.0F - transitionEased;
        visual.tint.a = static_cast<unsigned char>(std::lround(
            255.0F * (1.0F - transitionEased)));
    }
    return visual;
}

std::optional<LootItemVisual> rerollTargetLootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest) {
    if (!chest.rerolling) return std::nullopt;
    const float progress = static_cast<float>(
        std::clamp(chest.rerollProgress, 0.0, 1.0));
    const float incoming = std::clamp(
        (progress - 0.5F) * 2.0F, 0.0F, 1.0F);
    const float eased = incoming * incoming * incoming *
        (incoming * (incoming * 6.0F - 15.0F) + 10.0F);
    LootItemVisual visual = lootItemVisual(snapshot, chest);
    // Reconstruct the idle transform: lootItemVisual currently contains the
    // outgoing half of the transition.
    const float outgoing = std::clamp(
        progress * 2.0F, 0.0F, 1.0F);
    const float outgoingEased = outgoing * outgoing * outgoing *
        (outgoing * (outgoing * 6.0F - 15.0F) + 10.0F);
    visual.rotation -= outgoingEased * PI * 2.0F;
    visual.rotation += (eased - 1.0F) * PI * 2.0F;
    const float outgoingScale = 1.0F - outgoingEased;
    if (outgoingScale > 0.0001F) {
        visual.scale /= outgoingScale;
    } else {
        const float reveal = static_cast<float>(
            std::clamp(chest.loot.revealProgress, 0.0, 1.0));
        const float revealEased =
            1.0F - std::pow(1.0F - reveal, 3.0F);
        visual.scale = LootVisualScaleMultiplier *
            (1.50F * (0.35F + revealEased * 0.65F) +
             std::sin(reveal * PI) * 0.24F);
    }
    visual.scale *= eased;
    visual.tint.a = static_cast<unsigned char>(std::lround(
        255.0F * eased));
    return visual;
}

EnemyModelVisual enemyModelVisual(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return EnemyModelVisual::Minion;
    case EnemyType::Fast:
        return EnemyModelVisual::Rogue;
    case EnemyType::Heavy:
        return EnemyModelVisual::Warrior;
    case EnemyType::Ranged:
        return EnemyModelVisual::Mage;
    case EnemyType::Sapper:
        return EnemyModelVisual::Sapper;
    case EnemyType::Flying:
        return EnemyModelVisual::Flying;
    case EnemyType::Boss:
        return EnemyModelVisual::Boss;
    case EnemyType::Splitter:
        return EnemyModelVisual::Splitter;
    case EnemyType::Splitling:
        return EnemyModelVisual::Splitling;
    }
    return EnemyModelVisual::Minion;
}

EnemyAnimationVisual enemyAnimationVisual(
    const EnemyInstance& enemy) {
    if (enemy.hitAnimationRemaining > 0.0 &&
        enemy.state != EnemyState::Dead) {
        return EnemyAnimationVisual::Hit;
    }
    switch (enemy.state) {
    case EnemyState::MoveToCore:
    case EnemyState::ChasePlayer:
        return enemy.type == EnemyType::Fast ||
                       enemy.type == EnemyType::Flying
                   ? EnemyAnimationVisual::Run
                   : EnemyAnimationVisual::Walk;
    case EnemyState::AttackBuilding:
    case EnemyState::AttackCore:
    case EnemyState::AttackPlayer:
    case EnemyState::BossRamWindup:
    case EnemyState::BossSlamWindup:
    case EnemyState::BossWarCryWindup:
    case EnemyState::BossPhaseTransition:
        if (enemy.type == EnemyType::Ranged) {
            return EnemyAnimationVisual::RangedAttack;
        }
        if (enemy.type == EnemyType::Sapper) {
            return EnemyAnimationVisual::SapperAttack;
        }
        return EnemyAnimationVisual::MeleeAttack;
    case EnemyState::Spawn:
        return EnemyAnimationVisual::Spawn;
    case EnemyState::Dead:
        return EnemyAnimationVisual::Death;
    }
    return EnemyAnimationVisual::Idle;
}

float enemyVisualScale(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.55F;
    case EnemyType::Flying:
        return 0.52F;
    case EnemyType::Heavy:
        return 0.67F;
    case EnemyType::Boss:
        return 0.85F;
    case EnemyType::Ranged:
        return 0.94F;
    case EnemyType::Sapper:
        return 0.52F;
    case EnemyType::Basic:
        return 0.94F;
    case EnemyType::Splitter:
        return 0.58F;
    case EnemyType::Splitling:
        return 0.60F;
    }
    return 1.0F;
}

float enemyHitScale(const EnemyInstance& enemy) {
    constexpr float HitDuration = 0.22F;
    constexpr float Pi = 3.14159265358979323846F;
    constexpr float BounceAmplitude = 0.10F;
    const float progress = std::clamp(
        static_cast<float>(
            1.0 - enemy.hitAnimationRemaining / HitDuration),
        0.0F, 1.0F);
    const float pulse = std::sin(progress * Pi);
    const float hitScale = 1.0F - BounceAmplitude * pulse;
    if (enemy.type == EnemyType::Splitter &&
        enemy.splitAnimationRemaining > 0.0) {
        constexpr double SplitDuration = 0.38;
        const float splitProgress = std::clamp(
            static_cast<float>(
                1.0 - enemy.splitAnimationRemaining / SplitDuration),
            0.0F, 1.0F);
        const float eased = splitProgress * splitProgress *
            (3.0F - 2.0F * splitProgress);
        const float anticipation =
            std::sin(splitProgress * Pi * 3.0F) *
            (1.0F - splitProgress) * 0.035F;
        return 1.0F + eased * 0.34F + anticipation;
    }
    if (enemy.type != EnemyType::Splitling ||
        enemy.spawnAnimationRemaining <= 0.0) {
        return hitScale;
    }
    constexpr double SpawnDuration = 0.72;
    const float spawnProgress = std::clamp(
        static_cast<float>(
            1.0 - enemy.spawnAnimationRemaining / SpawnDuration),
        0.0F, 1.0F);
    const float eased = 1.0F -
        std::pow(1.0F - spawnProgress, 3.0F);
    const float overshoot =
        std::sin(spawnProgress * PI) * 0.18F;
    return hitScale * (0.28F + eased * 0.72F + overshoot);
}

Vector3 enemyRenderPosition(const EnemyInstance& enemy) {
    return {
        static_cast<float>(enemy.position.x),
        enemy.type == EnemyType::Flying
            ? 1.25F
            : static_cast<float>(0.02 + enemy.surfaceHeightOffset),
        static_cast<float>(enemy.position.z),
    };
}

float enemyAnimationSeconds(
    const EnemyInstance& enemy, double elapsedSeconds) {
    if (enemy.hitAnimationRemaining > 0.0) {
        return static_cast<float>(
            0.22 - enemy.hitAnimationRemaining);
    }
    if (enemy.state == EnemyState::BossRamWindup) {
        return static_cast<float>(
            std::max(
                0.0,
                enemy.ramWindup -
                    enemy.ramWindupRemaining));
    }
    if (enemy.state == EnemyState::BossSlamWindup ||
        enemy.state == EnemyState::BossWarCryWindup ||
        enemy.state == EnemyState::BossPhaseTransition) {
        return static_cast<float>(std::max(
            0.0, 1.5 - enemy.bossAbilityWindupRemaining));
    }
    if (enemy.state == EnemyState::AttackBuilding ||
        enemy.state == EnemyState::AttackCore ||
        enemy.state == EnemyState::AttackPlayer) {
        double interval = 1.0;
        if (enemy.type == EnemyType::Ranged) {
            interval = 1.45;
        } else if (enemy.type == EnemyType::Sapper) {
            interval = 1.2;
        }
        return static_cast<float>(
            std::max(
                0.0,
                interval -
                    enemy.attackCooldownRemaining));
    }
    return static_cast<float>(
        elapsedSeconds * enemy.locomotionRate +
        static_cast<double>(enemy.id.index % 4U) *
            0.173);
}

void drawCentered(const char* text, int y, int fontSize, Color color) {
    drawCenteredUiText(text, static_cast<float>(y),
                       static_cast<float>(fontSize), color);
}

const char* upgradeErrorMessage(UpgradeError error) {
    switch (error) {
    case UpgradeError::None:
        return "";
    case UpgradeError::NotFound:
        return "Building no longer exists";
    case UpgradeError::MaxLevel:
        return "Building is already at max level";
    case UpgradeError::Unsupported:
        return "Building cannot be upgraded";
    case UpgradeError::CoreLevelRequired:
        return "Upgrade Core first";
    case UpgradeError::InsufficientResources:
        return "Not enough resources for upgrade";
    }
    return "";
}

const char* buildingActionErrorMessage(BuildingActionError error) {
    switch (error) {
    case BuildingActionError::None:
        return "";
    case BuildingActionError::NotFound:
        return "Building no longer exists";
    case BuildingActionError::FullHealth:
        return "Building already fully repaired";
    case BuildingActionError::Unsupported:
        return "Core cannot be sold";
    case BuildingActionError::InsufficientResources:
        return "Not enough resources for repair";
    case BuildingActionError::Cooldown:
        return "Repair is recharging";
    }
    return "";
}

const char* weaponUpgradeErrorMessage(WeaponUpgradeError error) {
    switch (error) {
    case WeaponUpgradeError::None:
        return "";
    case WeaponUpgradeError::MaxLevel:
        return "Rifle already level III";
    case WeaponUpgradeError::CoreLevelRequired:
        return "Upgrade Core before Rifle";
    case WeaponUpgradeError::InsufficientCrystals:
        return "Not enough crystals for Rifle upgrade";
    }
    return "";
}

bool acceptsGameplayInput(RunState state) {
    return state == RunState::Gathering || state == RunState::BuildPhase ||
           state == RunState::Sunset || state == RunState::Wave ||
           state == RunState::WaveComplete;
}

std::vector<GridPosition> placementLine(
    BuildingType type, GridPosition start, GridPosition end,
    std::optional<PlacementLineAxis> axis) {
    const int spacing =
        buildingFootprintHalfExtent(type) == 1.0 ? 2 : 1;
    return ian::placementLine(
        start, end, spacing, axis);
}

std::vector<GridPosition> placementGestureLine(
    BuildingType type, GridPosition start,
    GridPosition aimedEnd, bool dragExtended,
    std::optional<PlacementLineAxis> axis) {
    const int spacing =
        buildingFootprintHalfExtent(type) == 1.0 ? 2 : 1;
    return ian::placementGestureLine(
        start, aimedEnd, spacing, dragExtended, axis);
}

std::uint8_t wallConnectionToward(
    GridPosition from, GridPosition neighbor) {
    const int deltaX = neighbor.x - from.x;
    const int deltaZ = neighbor.z - from.z;
    if (deltaX == 0 && deltaZ == -1) {
        return WallConnectionNorth;
    }
    if (deltaX == 1 && deltaZ == 0) {
        return WallConnectionEast;
    }
    if (deltaX == 0 && deltaZ == 1) {
        return WallConnectionSouth;
    }
    if (deltaX == -1 && deltaZ == 0) {
        return WallConnectionWest;
    }
    return 0U;
}

Vector2 repelInvalidPreview(
    Vector2 center, const BuildingPreview& preview,
    Vec3 playerPosition) {
    if (preview.placement.valid() ||
        preview.placement.error ==
            PlacementError::Occupied) {
        return center;
    }
    const float deltaX =
        center.x - static_cast<float>(playerPosition.x);
    const float deltaZ =
        center.y - static_cast<float>(playerPosition.z);
    const float length =
        std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    if (length <= 1e-4F) {
        return center;
    }
    const float pulse =
        0.08F +
        std::sin(static_cast<float>(GetTime()) * 18.0F) *
            0.025F;
    center.x += deltaX / length * pulse;
    center.y += deltaZ / length * pulse;
    return center;
}

float cannonYaw(const SimulationSnapshot& snapshot,
                const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    if (runtime != snapshot.cannons.end()) {
        return static_cast<float>(runtime->yaw);
    }
    return static_cast<float>(buildingRotationYaw(
        building.type, building.rotation));
}

float cannonBaseYaw(const SimulationSnapshot& snapshot,
                    const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    if (runtime != snapshot.cannons.end()) {
        return static_cast<float>(runtime->baseYaw);
    }
    return static_cast<float>(buildingRotationYaw(
        building.type, building.rotation));
}

float towerYaw(const SimulationSnapshot& snapshot,
               const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.towers.begin(), snapshot.towers.end(),
        [&building](const TowerRuntime& tower) {
            return tower.buildingId == building.id;
        });
    if (runtime != snapshot.towers.end()) {
        return static_cast<float>(runtime->yaw);
    }
    return static_cast<float>(buildingRotationYaw(
        building.type, building.rotation));
}

float towerBaseYaw(const SimulationSnapshot& snapshot,
                   const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.towers.begin(), snapshot.towers.end(),
        [&building](const TowerRuntime& tower) {
            return tower.buildingId == building.id;
        });
    if (runtime != snapshot.towers.end()) {
        return static_cast<float>(runtime->baseYaw);
    }
    return static_cast<float>(buildingRotationYaw(
        building.type, building.rotation));
}

float towerPitch(const SimulationSnapshot& snapshot,
                 const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.towers.begin(), snapshot.towers.end(),
        [&building](const TowerRuntime& tower) {
            return tower.buildingId == building.id;
        });
    return runtime != snapshot.towers.end()
        ? static_cast<float>(runtime->pitch) : 0.0F;
}

float cannonPitch(const SimulationSnapshot& snapshot,
                  const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    return runtime != snapshot.cannons.end()
               ? static_cast<float>(runtime->pitch)
               : 0.0F;
}

bool cannonLoaded(const SimulationSnapshot& snapshot,
                  const BuildingInstance& building) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    return runtime == snapshot.cannons.end() || runtime->loaded;
}

std::optional<EntityId> preciseBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot) {
    constexpr double MaximumDistance = 4.0;
    const double horizontal =
        std::cos(snapshot.playerPitch);
    const Ray ray{
        {static_cast<float>(snapshot.playerPosition.x),
         static_cast<float>(snapshot.playerPosition.y),
         static_cast<float>(snapshot.playerPosition.z)},
        {static_cast<float>(
             std::sin(snapshot.playerYaw) * horizontal),
         static_cast<float>(
             std::sin(snapshot.playerPitch)),
         static_cast<float>(
             -std::cos(snapshot.playerYaw) * horizontal)},
    };
    std::optional<EntityId> result;
    double closestDistance = MaximumDistance;
    for (const auto& building : snapshot.buildings) {
        const Vec3 center =
            buildingWorldPosition(building);
        const double offsetX =
            center.x - snapshot.playerPosition.x;
        const double offsetZ =
            center.z - snapshot.playerPosition.z;
        constexpr double BroadPhaseRadius =
            MaximumDistance + 2.0;
        if (offsetX * offsetX + offsetZ * offsetZ >
            BroadPhaseRadius * BroadPhaseRadius) {
            continue;
        }
        float yaw =
            static_cast<float>(building.rotation) *
            PI * 0.5F;
        float pitch = 0.0F;
        if (building.type == BuildingType::Turret ||
            building.type == BuildingType::GunTurret) {
            yaw = towerYaw(snapshot, building);
        } else if (
            building.type == BuildingType::Cannon ||
            building.type == BuildingType::Catapult) {
            yaw = cannonYaw(snapshot, building);
            pitch = cannonPitch(snapshot, building);
        }
        const auto distance =
            renderer.buildingRaycastDistance(
                building, snapshot.buildings, ray,
                MaximumDistance, yaw, pitch);
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = building.id;
        }
    }
    return result;
}

std::optional<EntityId> preciseResourceAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot) {
    if (snapshot.selectedWeapon == PlayerWeapon::Rifle ||
        snapshot.selectedWeapon == PlayerWeapon::IceWand ||
        snapshot.selectedWeapon == PlayerWeapon::FireWand) {
        return std::nullopt;
    }
    constexpr double MaximumDistance = 2.6;
    const double horizontal = std::cos(snapshot.playerPitch);
    const Ray ray{
        {static_cast<float>(snapshot.playerPosition.x),
         static_cast<float>(snapshot.playerPosition.y),
         static_cast<float>(snapshot.playerPosition.z)},
        {static_cast<float>(
             std::sin(snapshot.playerYaw) * horizontal),
         static_cast<float>(std::sin(snapshot.playerPitch)),
         static_cast<float>(
             -std::cos(snapshot.playerYaw) * horizontal)},
    };
    std::optional<EntityId> result;
    double closestDistance = MaximumDistance;
    for (const ResourceNode& node : snapshot.resourceNodes) {
        if (!node.active) {
            continue;
        }
        const double offsetX =
            node.position.x - snapshot.playerPosition.x;
        const double offsetZ =
            node.position.z - snapshot.playerPosition.z;
        constexpr double BroadPhaseRadius = MaximumDistance + 3.0;
        if (offsetX * offsetX + offsetZ * offsetZ >
            BroadPhaseRadius * BroadPhaseRadius) {
            continue;
        }
        const Vector3 position{
            static_cast<float>(node.position.x),
            static_cast<float>(
                node.position.y - node.groundOffset),
            static_cast<float>(node.position.z),
        };
        const auto distance = renderer.resourceRaycastDistance(
            node.type, position, ray, MaximumDistance,
            node.visualVariant,
            static_cast<float>(node.visualScale),
            static_cast<float>(node.visualYaw));
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = node.id;
        }
    }
    return result;
}

std::optional<EntityId> preciseWorldLandmarkAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot) {
    constexpr double MaximumDistance = 8.0;
    const double horizontal = std::cos(snapshot.playerPitch);
    const Ray ray{
        {static_cast<float>(snapshot.playerPosition.x),
         static_cast<float>(snapshot.playerPosition.y),
         static_cast<float>(snapshot.playerPosition.z)},
        {static_cast<float>(std::sin(snapshot.playerYaw) * horizontal),
         static_cast<float>(std::sin(snapshot.playerPitch)),
         static_cast<float>(-std::cos(snapshot.playerYaw) * horizontal)},
    };
    std::optional<EntityId> result;
    double closestDistance = MaximumDistance;
    for (const WorldLandmarkInstance& landmark : snapshot.worldLandmarks) {
        const double dx = landmark.position.x - snapshot.playerPosition.x;
        const double dz = landmark.position.z - snapshot.playerPosition.z;
        constexpr double BroadPhaseRadius = MaximumDistance + 7.0;
        if (dx * dx + dz * dz > BroadPhaseRadius * BroadPhaseRadius) continue;
        const auto distance = renderer.worldLandmarkRaycastDistance(
            static_cast<std::size_t>(landmark.type),
            {static_cast<float>(landmark.position.x),
             static_cast<float>(landmark.position.y),
             static_cast<float>(landmark.position.z)},
            static_cast<float>(landmark.yaw), ray, MaximumDistance);
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = landmark.id;
        }
    }
    return result;
}

std::optional<EntityId> preciseModularBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot) {
    if (!snapshot.aimedModularBuildingCandidate) {
        return std::nullopt;
    }
    const EntityId candidate =
        *snapshot.aimedModularBuildingCandidate;
    const auto frame = std::find_if(
        snapshot.platformFrames.begin(),
        snapshot.platformFrames.end(),
        [candidate](const PlatformFrameInstance& item) {
            return item.id == candidate;
        });
    if (frame == snapshot.platformFrames.end()) {
        return candidate;
    }

    constexpr double MaximumDistance = 6.0;
    const double horizontal =
        std::cos(snapshot.playerPitch);
    const Ray ray{
        {static_cast<float>(snapshot.playerPosition.x),
         static_cast<float>(snapshot.playerPosition.y),
         static_cast<float>(snapshot.playerPosition.z)},
        {static_cast<float>(
             std::sin(snapshot.playerYaw) * horizontal),
         static_cast<float>(
             std::sin(snapshot.playerPitch)),
         static_cast<float>(
             -std::cos(snapshot.playerYaw) * horizontal)},
    };
    const double minimumX = std::min(
        frame->supports[0].top.x,
        frame->supports[2].top.x);
    const double maximumX = std::max(
        frame->supports[1].top.x,
        frame->supports[3].top.x);
    const double minimumZ = std::min(
        frame->supports[0].top.z,
        frame->supports[1].top.z);
    const double maximumZ = std::max(
        frame->supports[2].top.z,
        frame->supports[3].top.z);
    std::array<float, 4> supportLengths{};
    for (std::size_t index = 0;
         index < frame->supports.size(); ++index) {
        supportLengths[index] = static_cast<float>(
            std::max(frame->supports[index].length, 0.0));
    }
    const auto distance =
        renderer.platformFrameRaycastDistance(
            {static_cast<float>((minimumX + maximumX) * 0.5),
             static_cast<float>(frame->floorHeight),
             static_cast<float>((minimumZ + maximumZ) * 0.5)},
            static_cast<float>(maximumX - minimumX) * 0.5F,
            supportLengths, ray, MaximumDistance);
    return distance ? std::optional<EntityId>{candidate}
                    : std::nullopt;
}

Vector3 colorToVector(Color color) {
    constexpr float ChannelScale = 1.0F / 255.0F;
    return {
        static_cast<float>(color.r) * ChannelScale,
        static_cast<float>(color.g) * ChannelScale,
        static_cast<float>(color.b) * ChannelScale,
    };
}

float smoothstep(float edge0, float edge1, float value) {
    const float normalized =
        std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return normalized * normalized * (3.0F - 2.0F * normalized);
}

void drawBuildGrid(
    Vector3 playerPosition, double worldLimit,
    const TerrainHeightfield& terrain) {
    constexpr float FadeStart = 13.0F;
    constexpr float FadeEnd = 20.0F;
    constexpr float GridTerrainOffset = 0.025F;
    constexpr float MinorOpacity = 0.15F;
    constexpr float MajorOpacity = 0.28F;
    constexpr int MajorInterval = 5;

    const int worldMinimum =
        static_cast<int>(std::ceil(-worldLimit));
    const int worldMaximum =
        static_cast<int>(std::floor(worldLimit));
    const int minimumX = std::max(
        worldMinimum,
        static_cast<int>(std::floor(playerPosition.x - FadeEnd)));
    const int maximumX = std::min(
        worldMaximum,
        static_cast<int>(std::ceil(playerPosition.x + FadeEnd)));
    const int minimumZ = std::max(
        worldMinimum,
        static_cast<int>(std::floor(playerPosition.z - FadeEnd)));
    const int maximumZ = std::min(
        worldMaximum,
        static_cast<int>(std::ceil(playerPosition.z + FadeEnd)));

    const auto lineColor = [playerPosition](float x, float z,
                                            bool major) {
        const float offsetX = x - playerPosition.x;
        const float offsetZ = z - playerPosition.z;
        const float distance = std::sqrt(offsetX * offsetX +
                                         offsetZ * offsetZ);
        const float fade =
            1.0F - smoothstep(FadeStart, FadeEnd, distance);
        const float opacity =
            fade * (major ? MajorOpacity : MinorOpacity);
        return Color{
            216,
            225,
            218,
            static_cast<unsigned char>(
                std::lround(std::clamp(opacity, 0.0F, 1.0F) *
                            255.0F)),
        };
    };
    constexpr int MaximumGridPointsPerAxis =
        static_cast<int>(FadeEnd) * 2 + 2;
    std::array<Vector3,
               MaximumGridPointsPerAxis *
                   MaximumGridPointsPerAxis>
        gridPoints{};
    const int pointCountX = maximumX - minimumX + 1;
    const int pointCountZ = maximumZ - minimumZ + 1;
    for (int z = 0; z < pointCountZ; ++z) {
        for (int x = 0; x < pointCountX; ++x) {
            const float worldX =
                static_cast<float>(minimumX + x);
            const float worldZ =
                static_cast<float>(minimumZ + z);
            gridPoints[static_cast<std::size_t>(
                z * pointCountX + x)] = {
                worldX,
                static_cast<float>(terrain.getHeight(
                    worldX, worldZ)) +
                    GridTerrainOffset,
                worldZ,
            };
        }
    }
    const auto gridPoint =
        [&](int x, int z) -> Vector3 {
            return gridPoints[static_cast<std::size_t>(
                (z - minimumZ) * pointCountX +
                (x - minimumX))];
        };
    const auto emitLine = [](Vector3 start, Vector3 end,
                             Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(start.x, start.y, start.z);
        rlVertex3f(end.x, end.y, end.z);
    };

    // DrawLine3D wraps every tiny segment in its own rlBegin/rlEnd pair.
    // The build grid contains thousands of segments, so that overhead can
    // push otherwise busy scenes over the frame budget.  Keep the same
    // terrain-conforming geometry, but submit it as one line batch.
    rlBegin(RL_LINES);
    for (int x = minimumX; x <= maximumX; ++x) {
        const bool major = x % MajorInterval == 0;
        for (int z = minimumZ; z < maximumZ; ++z) {
            const Color color =
                lineColor(static_cast<float>(x),
                          static_cast<float>(z) + 0.5F, major);
            if (color.a == 0U) {
                continue;
            }
            const Vector3 start = gridPoint(x, z);
            const Vector3 end = gridPoint(x, z + 1);
            Vector3 middle = Vector3Lerp(
                start, end, 0.5F);
            middle.y = static_cast<float>(terrain.getHeight(
                           middle.x, middle.z)) +
                GridTerrainOffset;
            emitLine(
                start, middle, color);
            emitLine(
                middle, end, color);
        }
    }
    for (int z = minimumZ; z <= maximumZ; ++z) {
        const bool major = z % MajorInterval == 0;
        for (int x = minimumX; x < maximumX; ++x) {
            const Color color =
                lineColor(static_cast<float>(x) + 0.5F,
                          static_cast<float>(z), major);
            if (color.a == 0U) {
                continue;
            }
            const Vector3 start = gridPoint(x, z);
            const Vector3 end = gridPoint(x + 1, z);
            Vector3 middle = Vector3Lerp(
                start, end, 0.5F);
            middle.y = static_cast<float>(terrain.getHeight(
                           middle.x, middle.z)) +
                GridTerrainOffset;
            emitLine(
                start, middle, color);
            emitLine(
                middle, end, color);
        }
    }
    rlEnd();
}

Color placementColor(
    PlacementError error, bool border) {
    const unsigned char alpha =
        border ? 245 : 74;
    switch (error) {
    case PlacementError::None:
        return border ? Color{106, 255, 146, 235}
                      : Color{57, 224, 109, 64};
    case PlacementError::InsufficientResources:
        return {255, 176, 62, alpha};
    case PlacementError::OutsideCoreArea:
        return {187, 104, 255, alpha};
    case PlacementError::OutOfRange:
        return {94, 172, 255, alpha};
    case PlacementError::PlayerOverlap:
        return {255, 222, 82, alpha};
    case PlacementError::CoreRequired:
    case PlacementError::CoreLevelRequired:
    case PlacementError::SkillRequired:
    case PlacementError::CoreAlreadyPlaced:
    case PlacementError::LimitReached:
        return {255, 132, 71, alpha};
    case PlacementError::Occupied:
    case PlacementError::WorldCollision:
    case PlacementError::ResourceBlocked:
        return {255, 88, 76, alpha};
    }
    return {255, 88, 76, alpha};
}

bool placementPreviewObstructed(PlacementError error) {
    return error == PlacementError::Occupied ||
           error == PlacementError::WorldCollision ||
           error == PlacementError::ResourceBlocked;
}

void drawPlacementFootprint(
    const BuildingPreview& preview,
    Vector2 visualCenter, float /*visualYaw*/) {
    const float x = visualCenter.x;
    const float z = visualCenter.y;
    const float halfExtent = static_cast<float>(
        buildingFootprintHalfExtent(preview.type));
    const float size = halfExtent * 2.0F;
    const Color fill =
        placementColor(preview.placement.error, false);
    const Color border =
        placementColor(preview.placement.error, true);
    const float height =
        static_cast<float>(preview.baseHeight) +
        0.055F;

    DrawPlane({x, height, z}, {size, size}, fill);
    DrawLine3D({x - halfExtent, height, z - halfExtent},
               {x + halfExtent, height, z - halfExtent},
               border);
    DrawLine3D({x + halfExtent, height, z - halfExtent},
               {x + halfExtent, height, z + halfExtent},
               border);
    DrawLine3D({x + halfExtent, height, z + halfExtent},
               {x - halfExtent, height, z + halfExtent},
               border);
    DrawLine3D({x - halfExtent, height, z + halfExtent},
               {x - halfExtent, height, z - halfExtent},
               border);
    if (halfExtent == 1.0F) {
        DrawLine3D({x, height, z - halfExtent},
                   {x, height, z + halfExtent}, border);
        DrawLine3D({x - halfExtent, height, z},
                   {x + halfExtent, height, z}, border);
    }

}

std::optional<PlatformFrameInstance>
automaticBuildingFoundation(
    BuildingType type, GridPosition position,
    double topHeight, double bottomHeight,
    double cellSize, EntityId id) {
    const double depth =
        topHeight - bottomHeight;
    if (depth <= 0.025) {
        return std::nullopt;
    }
    const Vec3 worldCenter =
        buildingWorldPosition(type, position);
    const int anchorX =
        snapPlatformFrameAxis(
            static_cast<int>(std::floor(
                worldCenter.x / cellSize)));
    const int anchorZ =
        snapPlatformFrameAxis(
            static_cast<int>(std::floor(
                worldCenter.z / cellSize)));
    const double minimumX =
        anchorX * cellSize;
    const double maximumX =
        (anchorX + PlatformFrameWidthCells) *
        cellSize;
    const double minimumZ =
        anchorZ * cellSize;
    const double maximumZ =
        (anchorZ + PlatformFrameWidthCells) *
        cellSize;
    const auto support =
        [topHeight, bottomHeight](
            double x, double z) {
            return FoundationSupport{
                .top = {x, topHeight, z},
                .bottom = {x, bottomHeight, z},
                .length =
                    topHeight - bottomHeight,
            };
        };
    return PlatformFrameInstance{
        .id = id,
        .anchor = {
            anchorX,
            static_cast<int>(std::lround(
                topHeight / cellSize)),
            anchorZ,
        },
        .floorHeight = topHeight,
        .storey = -1,
        .supports = {
            support(minimumX, minimumZ),
            support(maximumX, minimumZ),
            support(minimumX, maximumZ),
            support(maximumX, maximumZ),
        },
    };
}

void drawTacticalGroundCircle(
    Vector3 center, float radius, Color color,
    bool emphasizeArea = false) {
    if (radius <= 0.0F) {
        return;
    }
    center.y = 0.085F;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(radius * 7.0F)), 40, 112);
    for (int index = 0; index < segments; ++index) {
        const float angle0 =
            static_cast<float>(index) /
            static_cast<float>(segments) * PI * 2.0F;
        const float angle1 =
            static_cast<float>(index + 1) /
            static_cast<float>(segments) * PI * 2.0F;
        const Vector3 start{
            center.x + std::cos(angle0) * radius,
            center.y,
            center.z + std::sin(angle0) * radius,
        };
        const Vector3 end{
            center.x + std::cos(angle1) * radius,
            center.y,
            center.z + std::sin(angle1) * radius,
        };
        DrawCylinderEx(
            start, end, 0.022F, 0.022F, 5, color);
    }
    const auto softAlpha =
        static_cast<unsigned char>(
            std::min(
                static_cast<int>(color.a),
                emphasizeArea ? 82 : 48));
    const Color softColor{
        color.r, color.g, color.b, softAlpha};
    DrawCircle3D(
        center, radius * 0.66F,
        {1.0F, 0.0F, 0.0F}, 90.0F, softColor);
    DrawCircle3D(
        center, radius * 0.33F,
        {1.0F, 0.0F, 0.0F}, 90.0F, softColor);
    if (emphasizeArea) {
        for (int spoke = 0; spoke < 8; ++spoke) {
            const float angle =
                static_cast<float>(spoke) *
                PI * 0.25F;
            DrawLine3D(
                center,
                {
                    center.x + std::cos(angle) * radius,
                    center.y,
                    center.z + std::sin(angle) * radius,
                },
                softColor);
        }
    }
}

void drawTacticalGroundSector(
    Vector3 center, float radius, float yawRadians,
    float arcDegrees, Color color, float innerRadius) {
    if (radius <= 0.0F || arcDegrees <= 0.0F) return;
    arcDegrees = std::clamp(arcDegrees, 2.0F, 360.0F);
    innerRadius = std::clamp(innerRadius, 0.0F, radius - 0.05F);

    // Aiming at the same cell is the overwhelmingly common case. Cache this
    // flat mesh so trigonometry and gradient work only run when placement,
    // rotation, range or level changes.
    static std::array<GroundSectorCacheEntry, 8> cache{};
    static std::size_t nextCacheEntry{};
    GroundSectorCacheEntry* cached = nullptr;
    for (GroundSectorCacheEntry& candidate : cache) {
        if (matchesGroundSector(
                candidate, center, radius, yawRadians,
                arcDegrees, innerRadius)) {
            cached = &candidate;
            break;
        }
    }

    if (cached == nullptr) {
        cached = &cache[nextCacheEntry];
        nextCacheEntry = (nextCacheEntry + 1U) % cache.size();
        cached->center = center;
        cached->radius = radius;
        cached->innerRadius = innerRadius;
        cached->yawRadians = yawRadians;
        cached->arcDegrees = arcDegrees;
        cached->valid = true;
        cached->triangles.clear();

        const float halfAngle = std::clamp(
            arcDegrees * DEG2RAD * 0.5F, 0.02F, PI);
        const int segments = std::clamp(
            static_cast<int>(std::ceil(
                radius * 3.2F * arcDegrees / 90.0F)),
            24, 72);
        constexpr std::array<float, 7> RadialScales{
            0.0F, 0.08F, 0.24F, 0.52F,
            0.76F, 0.90F, 1.0F};

        const auto pointAt =
            [center, radius, yawRadians](
                float offset, float scale) {
                const float angle = yawRadians + offset;
                return Vector3{
                    center.x - std::sin(angle) * radius * scale,
                    center.y,
                    center.z - std::cos(angle) * radius * scale,
                };
            };
        const auto opacityAt = [innerRadius](float angleT, float radiusT) {
            if (innerRadius <= 0.001F && radiusT <= 0.001F) {
                return 0.82F;
            }
            const float angularDistance =
                std::min(angleT, 1.0F - angleT) * 2.0F;
            // The boundary is the visual focus: a crisp white outer arc and
            // two white rays feather inward into an almost clear center.
            const float sideEdge = 1.0F - smoothstep(
                0.0F, 0.20F, angularDistance);
            const float outerEdge = smoothstep(
                0.70F, 1.0F, radiusT);
            const float innerEdge = innerRadius > 0.001F
                ? 1.0F - smoothstep(0.0F, 0.24F, radiusT)
                : 0.0F;
            constexpr float ClearInterior = 0.025F;
            const float edge = std::max(
                sideEdge, std::max(outerEdge, innerEdge));
            return ClearInterior + edge * (1.0F - ClearInterior);
        };
        const auto vertexAt = [&](float angleT, float radiusT) {
            const float angle =
                -halfAngle + angleT * halfAngle * 2.0F;
            const float radiusScale =
                (innerRadius / radius) + radiusT *
                (1.0F - innerRadius / radius);
            return GroundSectorVertex{
                .position = radiusScale <= 0.001F
                    ? center
                    : pointAt(angle, radiusScale),
                .opacity = static_cast<unsigned char>(std::lround(
                    opacityAt(angleT, radiusT) * 255.0F)),
            };
        };
        const auto appendTriangle =
            [&cached](GroundSectorVertex first,
                      GroundSectorVertex second,
                      GroundSectorVertex third) {
                cached->triangles.push_back(first);
                cached->triangles.push_back(second);
                cached->triangles.push_back(third);
            };

        cached->triangles.reserve(
            static_cast<std::size_t>(segments) *
            (RadialScales.size() - 1U) * 6U);
        for (int index = 0; index < segments; ++index) {
            const float angleT0 = static_cast<float>(index) /
                static_cast<float>(segments);
            const float angleT1 = static_cast<float>(index + 1) /
                static_cast<float>(segments);
            for (std::size_t band = 0;
                 band + 1U < RadialScales.size(); ++band) {
                const float radiusT0 = RadialScales[band];
                const float radiusT1 = RadialScales[band + 1U];
                const GroundSectorVertex p00 =
                    vertexAt(angleT0, radiusT0);
                const GroundSectorVertex p01 =
                    vertexAt(angleT1, radiusT0);
                const GroundSectorVertex p10 =
                    vertexAt(angleT0, radiusT1);
                const GroundSectorVertex p11 =
                    vertexAt(angleT1, radiusT1);
                appendTriangle(p00, p11, p10);
                if (innerRadius > 0.001F || band > 0U) {
                    appendTriangle(p00, p01, p11);
                }
            }
        }
    }

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (const GroundSectorVertex& vertex : cached->triangles) {
        const auto alpha = static_cast<unsigned char>(std::lround(
            static_cast<float>(color.a) *
            static_cast<float>(vertex.opacity) / 255.0F));
        rlColor4ub(color.r, color.g, color.b, alpha);
        rlVertex3f(
            vertex.position.x, vertex.position.y,
            vertex.position.z);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

void drawTacticalTargetLink(
    Vector3 start, Vector3 end, Color color) {
    const Vector3 delta = Vector3Subtract(end, start);
    const float length = Vector3Length(delta);
    if (length <= 0.001F) {
        return;
    }
    const Vector3 direction =
        Vector3Scale(delta, 1.0F / length);
    constexpr float DashLength = 0.42F;
    constexpr float GapLength = 0.24F;
    for (float distance = 0.0F; distance < length;
         distance += DashLength + GapLength) {
        const float endDistance =
            std::min(distance + DashLength, length);
        DrawCylinderEx(
            Vector3Add(
                start, Vector3Scale(direction, distance)),
            Vector3Add(
                start,
                Vector3Scale(direction, endDistance)),
            0.018F, 0.018F, 5, color);
    }
    DrawSphere(end, 0.075F, color);
}

void drawBuildingTacticalOverlay(
    const SimulationSnapshot& snapshot) {
    if (snapshot.selectedBuilding || !snapshot.aimedBuilding) {
        return;
    }
    const auto building = std::find_if(
        snapshot.buildings.begin(), snapshot.buildings.end(),
        [&snapshot](const BuildingInstance& candidate) {
            return candidate.id == *snapshot.aimedBuilding;
        });
    if (building == snapshot.buildings.end()) {
        return;
    }

    Vec3 center = buildingWorldPosition(*building);
    std::optional<EntityId> targetId;
    if (building->type == BuildingType::Turret ||
        building->type == BuildingType::GunTurret) {
        drawTacticalGroundSector(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TowerSystem::attackRange(
                    building->type, building->level)),
            towerBaseYaw(snapshot, *building),
            static_cast<float>(defenseAttackArcDegrees(
                building->level)),
            {255, 252, 244, 190});
        const auto runtime = std::find_if(
            snapshot.towers.begin(), snapshot.towers.end(),
            [&building](const TowerRuntime& tower) {
                return tower.buildingId == building->id;
            });
        if (runtime != snapshot.towers.end()) {
            targetId = runtime->targetId;
        }
        center.y += building->type == BuildingType::GunTurret
            ? 0.69 : 1.42;
    } else if (building->type == BuildingType::Cannon ||
               building->type == BuildingType::Catapult) {
        drawTacticalGroundSector(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                CannonSystem::attackRange(
                    building->type, building->level)),
            cannonBaseYaw(snapshot, *building),
            static_cast<float>(defenseAttackArcDegrees(
                building->level)),
            {255, 252, 244, 190},
            static_cast<float>(CannonSystem::minimumRange(
                building->type, building->level)));
        const auto runtime = std::find_if(
            snapshot.cannons.begin(), snapshot.cannons.end(),
            [&building](const CannonRuntime& cannon) {
                return cannon.buildingId == building->id;
            });
        if (runtime != snapshot.cannons.end()) {
            targetId = runtime->targetId;
        }
        center.y += 1.5;
    } else if (building->type == BuildingType::SlowTrap) {
        drawTacticalGroundCircle(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TrapSystem::triggerRadius(building->level)),
            {91, 209, 255, 190}, true);
        return;
    } else if (building->type == BuildingType::SpikeTrap) {
        drawTacticalGroundCircle(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TrapSystem::spikeTriggerRadius(
                    building->level)),
            {255, 115, 82, 190}, true);
        return;
    } else {
        return;
    }

    if (!targetId) {
        return;
    }
    const auto target = std::find_if(
        snapshot.enemies.begin(), snapshot.enemies.end(),
        [&targetId](const EnemyInstance& enemy) {
            return enemy.active && enemy.id == *targetId;
        });
    if (target == snapshot.enemies.end()) {
        return;
    }
    Vec3 targetPosition = target->position;
    targetPosition.y += 0.45;
    drawTacticalTargetLink(
        {static_cast<float>(center.x),
         static_cast<float>(center.y),
         static_cast<float>(center.z)},
        {static_cast<float>(targetPosition.x),
         static_cast<float>(targetPosition.y),
         static_cast<float>(targetPosition.z)},
        {255, 238, 184, 220});
    if (building->type == BuildingType::Cannon) {
        drawTacticalGroundCircle(
            {static_cast<float>(target->position.x), 0.09F,
             static_cast<float>(target->position.z)},
            static_cast<float>(
                CannonSystem::explosionRadius(building->level)),
            {255, 129, 62, 210}, true);
    } else {
        drawTacticalGroundCircle(
            {static_cast<float>(target->position.x), 0.09F,
             static_cast<float>(target->position.z)},
            0.38F, {255, 238, 184, 205});
    }
}

GameBalance loadAppBalance() {
    return loadGameBalance("assets/data/enemies.json", "assets/data/waves.json",
                           "assets/data/buildings.json", "assets/data/weapons.json",
                           "assets/data/economy.json", "assets/data/gameplay.json")
        .balance;
}

MapDefinition loadAppMap() {
    return loadMapDefinition("assets/maps/graybox.json").map;
}

WorldConfig loadAppWorldConfig() {
    return loadWorldConfig(
               "assets/data/world.json")
        .config;
}

std::vector<SkillNodeDefinition> loadAppSkills() {
    return loadSkillTreeDefinitions("assets/data/skills.json");
}

InsightConfig loadAppInsightConfig() {
    return loadInsightConfig("assets/data/insight.json");
}

std::vector<ObjectiveDefinition> loadAppObjectives() {
    return loadObjectiveDefinitions("assets/data/objectives.json");
}

std::array<EnvironmentProfile, 4> loadAppEnvironment() {
    return loadEnvironmentProfiles("assets/data/environment.json").profiles;
}


} // namespace ian::app_detail
