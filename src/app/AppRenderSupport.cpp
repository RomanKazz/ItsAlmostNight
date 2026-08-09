#include "app/AppRenderSupport.hpp"

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

} // namespace

LootItemVisual lootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest) {
    const float reveal = static_cast<float>(
        std::clamp(chest.loot.revealProgress, 0.0, 1.0));
    const float eased = 1.0F - std::pow(1.0F - reveal, 3.0F);
    const float riseSpin = reveal * reveal * (3.0F - 2.0F * reveal);
    const Vec3 position = lootVisualPosition(chest);
    return {
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
        return 0.84F;
    case EnemyType::Flying:
        return 0.52F;
    case EnemyType::Heavy:
        return 1.18F;
    case EnemyType::Boss:
        return 0.85F;
    case EnemyType::Ranged:
        return 0.94F;
    case EnemyType::Sapper:
        return 0.52F;
    case EnemyType::Basic:
        return 0.94F;
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
    return 1.0F - BounceAmplitude * pulse;
}

Vector3 enemyRenderPosition(const EnemyInstance& enemy) {
    return {
        static_cast<float>(enemy.position.x),
        enemy.type == EnemyType::Flying ? 1.25F : 0.02F,
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
    case WeaponUpgradeError::InsufficientGold:
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
    constexpr float QuarterTurn = PI * 0.5F;
    return static_cast<float>(building.rotation) * QuarterTurn;
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
    constexpr float QuarterTurn = PI * 0.5F;
    return static_cast<float>(building.rotation) * QuarterTurn;
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
        if (building.type == BuildingType::Turret) {
            yaw = towerYaw(snapshot, building);
        } else if (
            building.type == BuildingType::Cannon) {
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
        snapshot.selectedWeapon == PlayerWeapon::IceWand) {
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
            static_cast<std::size_t>(
                node.id.index % TreeVisualVariantCount),
            static_cast<float>(node.visualScale),
            static_cast<float>(node.visualYaw));
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = node.id;
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
    constexpr float FadeStart = 15.0F;
    constexpr float FadeEnd = 25.0F;
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
    const auto gridPoint =
        [&terrain](float x, float z) {
            return Vector3{
                x,
                static_cast<float>(
                    terrain.getHeight(x, z)) +
                    GridTerrainOffset,
                z,
            };
        };

    for (int x = minimumX; x <= maximumX; ++x) {
        const bool major = x % MajorInterval == 0;
        for (int z = minimumZ; z < maximumZ; ++z) {
            const Color color =
                lineColor(static_cast<float>(x),
                          static_cast<float>(z) + 0.5F, major);
            if (color.a == 0U) {
                continue;
            }
            const float lineX = static_cast<float>(x);
            const float startZ = static_cast<float>(z);
            const float middleZ = startZ + 0.5F;
            const float endZ = startZ + 1.0F;
            DrawLine3D(
                gridPoint(lineX, startZ),
                gridPoint(lineX, middleZ), color);
            DrawLine3D(
                gridPoint(lineX, middleZ),
                gridPoint(lineX, endZ), color);
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
            const float lineZ = static_cast<float>(z);
            const float startX = static_cast<float>(x);
            const float middleX = startX + 0.5F;
            const float endX = startX + 1.0F;
            DrawLine3D(
                gridPoint(startX, lineZ),
                gridPoint(middleX, lineZ), color);
            DrawLine3D(
                gridPoint(middleX, lineZ),
                gridPoint(endX, lineZ), color);
        }
    }
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
    Vector2 visualCenter, float visualYaw) {
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

    const bool directional =
        preview.type == BuildingType::Turret ||
        preview.type == BuildingType::Cannon ||
        preview.type == BuildingType::Gate;
    if (!directional) {
        return;
    }
    const Vector2 direction{
        -std::sin(visualYaw), -std::cos(visualYaw)};
    const Vector3 arrowStart{x, height + 0.04F, z};
    const Vector3 arrowEnd{
        x + direction.x * (halfExtent + 0.7F),
        height + 0.04F,
        z + direction.y * (halfExtent + 0.7F),
    };
    const Vector3 arrowBase{
        arrowEnd.x - direction.x * 0.24F,
        arrowEnd.y,
        arrowEnd.z - direction.y * 0.24F,
    };
    DrawCylinderEx(
        arrowStart, arrowBase, 0.045F, 0.045F, 10,
        border);
    DrawCylinderEx(
        arrowBase, arrowEnd, 0.15F, 0.0F, 12, border);
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
    if (!snapshot.aimedBuilding) {
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
    if (building->type == BuildingType::Turret) {
        drawTacticalGroundCircle(
            {static_cast<float>(center.x), 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TowerSystem::attackRange(building->level)),
            {255, 226, 135, 165});
        const auto runtime = std::find_if(
            snapshot.towers.begin(), snapshot.towers.end(),
            [&building](const TowerRuntime& tower) {
                return tower.buildingId == building->id;
            });
        if (runtime != snapshot.towers.end()) {
            targetId = runtime->targetId;
        }
        center.y = 1.42;
    } else if (building->type == BuildingType::Cannon) {
        drawTacticalGroundCircle(
            {static_cast<float>(center.x), 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                CannonSystem::attackRange(building->level)),
            {255, 226, 135, 165});
        const auto runtime = std::find_if(
            snapshot.cannons.begin(), snapshot.cannons.end(),
            [&building](const CannonRuntime& cannon) {
                return cannon.buildingId == building->id;
            });
        if (runtime != snapshot.cannons.end()) {
            targetId = runtime->targetId;
        }
        center.y = 1.5;
    } else if (building->type == BuildingType::SlowTrap) {
        drawTacticalGroundCircle(
            {static_cast<float>(center.x), 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TrapSystem::triggerRadius(building->level)),
            {91, 209, 255, 190}, true);
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

std::array<EnvironmentProfile, 4> loadAppEnvironment() {
    return loadEnvironmentProfiles("assets/data/environment.json").profiles;
}


} // namespace ian::app_detail
