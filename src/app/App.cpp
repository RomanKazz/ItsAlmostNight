#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace ian {
namespace app_detail {

constexpr int InitialWindowWidth = 1280;
constexpr int InitialWindowHeight = 720;
constexpr double BuildingHealthBarDwellSeconds = 0.15;

const char* modularPlacementMessage(
    std::optional<ModularPlacementError> error) {
    if (!error) {
        return "AIM AT TERRAIN";
    }
    switch (*error) {
    case ModularPlacementError::None:
        return "LMB BUILD";
    case ModularPlacementError::Occupied:
        return "PLACE OCCUPIED";
    case ModularPlacementError::OutOfBounds:
        return "OUTSIDE MAP";
    case ModularPlacementError::TooFar:
        return "TOO FAR";
    case ModularPlacementError::SupportTooLong:
        return "SUPPORTS TOO LONG";
    case ModularPlacementError::PlayerOverlap:
        return "PLAYER IN THE WAY";
    case ModularPlacementError::TerrainIntersection:
        return "TERRAIN INTERSECTION";
    case ModularPlacementError::MaximumStorey:
        return "MAXIMUM STOREY";
    case ModularPlacementError::NoSupport:
        return "NO STRUCTURAL SUPPORT";
    case ModularPlacementError::ResourceBlocked:
        return "CLEAR RESOURCE FIRST";
    case ModularPlacementError::InsufficientResources:
        return "NOT ENOUGH RESOURCES";
    }
    return "CANNOT BUILD";
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
        return 1.0F;
    }
    return 1.0F;
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

std::optional<EntityId> preciseModularBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot) {
    if (!snapshot.aimedModularBuilding) {
        return std::nullopt;
    }
    const EntityId candidate =
        *snapshot.aimedModularBuilding;
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

void drawBuildGrid(Vector3 playerPosition, double worldLimit) {
    constexpr float FadeStart = 15.0F;
    constexpr float FadeEnd = 25.0F;
    constexpr float GridHeight = 0.025F;
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

    for (int x = minimumX; x <= maximumX; ++x) {
        const bool major = x % MajorInterval == 0;
        for (int z = minimumZ; z < maximumZ; ++z) {
            const Color color =
                lineColor(static_cast<float>(x),
                          static_cast<float>(z) + 0.5F, major);
            if (color.a == 0U) {
                continue;
            }
            DrawLine3D({static_cast<float>(x), GridHeight,
                        static_cast<float>(z)},
                       {static_cast<float>(x), GridHeight,
                        static_cast<float>(z + 1)},
                       color);
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
            DrawLine3D({static_cast<float>(x), GridHeight,
                        static_cast<float>(z)},
                       {static_cast<float>(x + 1), GridHeight,
                        static_cast<float>(z)},
                       color);
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

std::array<EnvironmentProfile, 4> loadAppEnvironment() {
    return loadEnvironmentProfiles("assets/data/environment.json").profiles;
}

} // namespace app_detail

using namespace app_detail;

App::App()
    : simulation_(
          loadAppBalance(), loadAppMap(),
          loadAppWorldConfig()),
      environment_(loadAppEnvironment()) {
    effects_.reserve(128);
    arrowVisuals_.reserve(64);
    damageIndicators_.reserve(12);
    floatingDamageNumbers_.reserve(32);
    resourceGainVisuals_.reserve(16);
    destroyedResourceVisuals_.reserve(8);
    destroyedEnemyVisuals_.reserve(16);
    buildingShotRecoilVisuals_.reserve(32);
}

int App::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT |
                   FLAG_MSAA_4X_HINT);
    InitWindow(InitialWindowWidth, InitialWindowHeight,
               "It's Almost Night");
    ToggleBorderlessWindowed();
    SetTargetFPS(144);
    renderer_.emplace();
    renderer_->initialize();
    modularBuildingRenderer_.setRenderer(&*renderer_);
    renderer_->rebuildTerrain(simulation_.terrain());
    ui_.initialize();
    audio_.initialize();

    while (!WindowShouldClose()) {
        processInput();
        update();
        render();
    }

    ui_.shutdown();
    audio_.shutdown();
    modularBuildingRenderer_.setRenderer(nullptr);
    renderer_->shutdown();
    renderer_.reset();
    CloseWindow();
    return 0;
}

void App::render() {
    const auto snapshot = simulation_.snapshot();
    structuralRiskIds_.clear();
    std::vector<EntityId> structuralRiskRoots;
    const auto addStructuralRiskRoot =
        [&structuralRiskRoots](EntityId support) {
            if (std::ranges::find(
                    structuralRiskRoots, support) ==
                structuralRiskRoots.end()) {
                structuralRiskRoots.push_back(support);
            }
        };
    if (!foundationBuildMode_ &&
        !snapshot.selectedBuilding &&
        snapshot.aimedModularBuilding &&
        std::ranges::any_of(
            snapshot.platformFrames,
            [&snapshot](const PlatformFrameInstance& frame) {
                return frame.id ==
                       *snapshot.aimedModularBuilding;
            })) {
        addStructuralRiskRoot(
            *snapshot.aimedModularBuilding);
    }
    for (const EntityId target : removalDragTargets_) {
        if (std::ranges::any_of(
                snapshot.platformFrames,
                [target](const PlatformFrameInstance& frame) {
                    return frame.id == target;
                })) {
            addStructuralRiskRoot(target);
        }
    }
    structuralRiskIds_ =
        simulation_.structuralCollapseRisk(
            structuralRiskRoots);
    auto presentationSnapshot = snapshot;
    presentationSnapshot.aimedResource = hoveredResource_;
    presentationSnapshot.aimedBuilding = hoveredBuilding_;
    presentationSnapshot.aimedEnemy = hoveredEnemy_;
    presentationSnapshot.aimedBuildingUpgradeCost =
        hoveredBuildingUpgradeCost_;
    presentationSnapshot.aimedBuildingStats =
        hoveredBuildingStats_;

    if (snapshot.state == RunState::MainMenu) {
        renderer_->beginUiOnlyFrame({18, 22, 31, 255});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        ui_.drawPanel({centerX - 420.0F, centerY - 210.0F,
                       840.0F, 420.0F});
        ui_.drawInsetPanel({centerX - 380.0F, centerY - 164.0F,
                            760.0F, 128.0F});
        drawCentered("IT'S ALMOST NIGHT",
                     static_cast<int>(centerY) - 140, 42,
                     {245, 220, 174, 255});
        pendingStartFromUi_ =
            ui_.drawButton({centerX - 200.0F, centerY + 30.0F,
                            400.0F, 82.0F},
                           "START RUN") ||
            pendingStartFromUi_;
        drawCentered("ENTER", static_cast<int>(centerY) + 136, 16,
                     {199, 174, 142, 255});
    } else {
        const double visualYaw = snapshot.playerYaw;
        const double visualPitch = snapshot.playerPitch;
        const double cosPitch = std::cos(visualPitch);
        Vector3 position = {
            static_cast<float>(snapshot.playerPosition.x),
            static_cast<float>(
                groundCameraSmoothingInitialized_
                    ? smoothedGroundCameraY_
                    : snapshot.playerPosition.y),
            static_cast<float>(snapshot.playerPosition.z),
        };
        Vector3 forward = {
            static_cast<float>(std::sin(visualYaw) * cosPitch),
            static_cast<float>(std::sin(visualPitch)),
            static_cast<float>(-std::cos(visualYaw) * cosPitch),
        };
        const float bobAmount =
            static_cast<float>(cameraBobAmount_) *
            (input_.sprint ? 1.12F : 1.0F) *
            motionBobIntensity_;
        const float bobSide =
            static_cast<float>(
                std::sin(cameraBobPhase_)) *
            0.012F * bobAmount;
        const float bobVertical =
            -static_cast<float>(
                std::abs(std::sin(cameraBobPhase_))) *
            0.024F * bobAmount;
        const Vector3 bobRight = {
            static_cast<float>(
                std::cos(visualYaw)),
            0.0F,
            static_cast<float>(
                std::sin(visualYaw)),
        };
        position = Vector3Add(
            position,
            Vector3Scale(
                bobRight,
                static_cast<float>(cameraLookYawLag_ * 0.42) *
                    motionSwayIntensity_));
        position.y += static_cast<float>(
            cameraLookPitchLag_ * 0.32) *
            motionSwayIntensity_;
        position = Vector3Add(
            position,
            Vector3Scale(bobRight, bobSide));
        position.y += bobVertical;
        Vector3 bobCameraUp = Vector3Normalize(
            Vector3Add(
                {0.0F, 1.0F, 0.0F},
                Vector3Scale(
                    bobRight,
                    (-static_cast<float>(
                         std::sin(cameraBobPhase_)) *
                         0.0045F * bobAmount -
                     static_cast<float>(
                         cameraStrafeLean_) *
                         motionSwayIntensity_))));
        if (landingResponseRemaining_ > 0.0 &&
            landingResponseDuration_ > 0.0) {
            const double progress = std::clamp(
                1.0 - landingResponseRemaining_ /
                          landingResponseDuration_,
                0.0, 1.0);
            const float landingCurve = static_cast<float>(
                std::sin(progress * PI) *
                landingResponseStrength_) *
                motionLandingIntensity_;
            position.y -= landingCurve * 0.052F;
            forward.y -= landingCurve * 0.012F;
            forward = Vector3Normalize(forward);
        }
        position = Vector3Add(
            position,
            Vector3Scale(
                bobRight,
                static_cast<float>(cameraImpulseOffset_.x) *
                    motionShakeIntensity_));
        position.y +=
            static_cast<float>(cameraImpulseOffset_.y) *
            motionShakeIntensity_;
        position = Vector3Add(
            position,
            Vector3Scale(
                forward,
                static_cast<float>(cameraImpulseOffset_.z) *
                    motionShakeIntensity_));
        if (weaponRecoilRemaining_ > 0.0 &&
            weaponRecoilDuration_ > 0.0) {
            const float progress = std::clamp(
                static_cast<float>(
                    1.0 -
                    weaponRecoilRemaining_ /
                        weaponRecoilDuration_),
                0.0F, 1.0F);
            const float recoil =
                std::sin(progress * PI) *
                weaponRecoilStrength_;
            position = Vector3Subtract(
                position, Vector3Scale(forward, recoil));
            forward.y += recoil * 0.38F;
            forward = Vector3Normalize(forward);
        }
        if (cameraShakeRemaining_ > 0.0) {
            const double visualTime = GetTime();
            const float shake =
                static_cast<float>(cameraShakeStrength_ * cameraShakeRemaining_ / 0.35) *
                motionShakeIntensity_;
            position.x += static_cast<float>(std::sin(visualTime * 83.0)) * shake;
            position.y += static_cast<float>(std::cos(visualTime * 97.0)) * shake * 0.7F;
            position.z += static_cast<float>(std::sin(visualTime * 71.0)) * shake * 0.5F;
        }
        const Camera3D camera = {
            .position = position,
            .target = Vector3Add(position, forward),
            .up = bobCameraUp,
            .fovy = cameraFov_,
            .projection = CAMERA_PERSPECTIVE,
        };
        constexpr Color DayGround{66, 112, 67, 255};
        constexpr Color NightGround{28, 52, 50, 255};

        float automaticTime = environment_.timeOfDay();
        if (snapshot.state == RunState::Gathering ||
            snapshot.state == RunState::BuildPhase) {
            automaticTime = 0.25F;
        } else if (snapshot.state == RunState::Sunset) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.25F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        } else if (snapshot.state == RunState::Wave) {
            automaticTime = 0.75F;
        } else if (snapshot.state == RunState::WaveComplete) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.75F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        }
        environment_.setAutomaticTime(automaticTime);
        const EnvironmentState environment = environment_.state();
        const float nightAmount = environment.nightFactor;
        const Color ground = {
            static_cast<unsigned char>(
                static_cast<float>(DayGround.r) +
                (static_cast<float>(NightGround.r) -
                 static_cast<float>(DayGround.r)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.g) +
                (static_cast<float>(NightGround.g) -
                 static_cast<float>(DayGround.g)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.b) +
                (static_cast<float>(NightGround.b) -
                 static_cast<float>(DayGround.b)) *
                    nightAmount),
            255,
        };
        const Vector3 lightDirection =
            Vector3Scale(environment.celestialDirection, -1.0F);
        const WorldLighting lighting{
            .cameraPosition = camera.position,
            .sunDirection = lightDirection,
            .sunColor = environment.sunColor,
            .sunIntensity = environment.sunIntensity,
            .skyAmbientColor = environment.skyAmbientColor,
            .groundAmbientColor = environment.groundAmbientColor,
            .ambientIntensity = environment.ambientIntensity,
            .fogColor = colorToVector(environment.fogColor),
            .fogStart = environment.fogStart,
            .fogEnd = environment.fogEnd,
            .dayNightTint = environment.dayNightTint,
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };
        const Vector3 cameraRight =
            Vector3Normalize(Vector3CrossProduct(forward, {0.0F, 1.0F, 0.0F}));
        const Vector3 cameraUp =
            Vector3Normalize(Vector3CrossProduct(cameraRight, forward));
        const SkyState skyState{
            .cameraForward = forward,
            .cameraRight = cameraRight,
            .cameraUp = cameraUp,
            .verticalFovDegrees = camera.fovy,
            .zenithColor = colorToVector(environment.skyTop),
            .horizonColor = colorToVector(environment.skyHorizon),
            .lowerSkyColor = colorToVector(environment.lowerSky),
            .celestialDirection = environment.celestialDirection,
            .celestialColor = environment.celestialColor,
            .celestialIntensity = environment.sunIntensity,
            .nightAmount = nightAmount,
            .timeSeconds = static_cast<float>(GetTime()),
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };

        drawShadowPass(snapshot, lighting);
        drawSelectionPass(presentationSnapshot, camera);
        renderer_->beginWorldPass(environment.skyHorizon);
        renderer_->drawSky(skyState);
        BeginMode3D(camera);
        renderer_->beginWorldShader(lighting);
        WorldMaterialState terrainMaterial{};
        terrainMaterial.terrainAmount = 1.0F;
        terrainMaterial.bakedAo = 0.9F;
        renderer_->setWorldMaterial(terrainMaterial);
        renderer_->drawTerrain(
            ground, showTerrainWireframe_);
        drawWorldEntities(presentationSnapshot, camera, nightAmount,
                          lighting);
        drawBlobShadows(snapshot, camera);
        drawWorldOverlays(presentationSnapshot, lighting);
        drawPresentationEffects();
        EndMode3D();
        renderer_->drawSelectionOutline();

        auto healthBarSnapshot = presentationSnapshot;
        if (healthBarSnapshot.aimedBuilding &&
            buildingHoverSeconds_ <
                BuildingHealthBarDwellSeconds) {
            healthBarSnapshot.aimedBuilding.reset();
        }
        if (!healthBarSnapshot.aimedBuilding &&
            recentlyDamagedBuilding_ &&
            damagedBuildingHealthBarRemaining_ > 0.0) {
            const bool isBuilding =
                std::any_of(
                    healthBarSnapshot.buildings.begin(),
                    healthBarSnapshot.buildings.end(),
                    [this](
                        const BuildingInstance& building) {
                        return building.id ==
                               *recentlyDamagedBuilding_;
                    });
            if (isBuilding) {
                healthBarSnapshot.aimedBuilding =
                    recentlyDamagedBuilding_;
            } else {
                const bool isModular =
                    std::any_of(
                        healthBarSnapshot.platformFrames.begin(),
                        healthBarSnapshot.platformFrames.end(),
                        [this](
                            const PlatformFrameInstance& frame) {
                            return frame.id ==
                                   *recentlyDamagedBuilding_;
                        }) ||
                    std::any_of(
                        healthBarSnapshot.modularWalls.begin(),
                        healthBarSnapshot.modularWalls.end(),
                        [this](const WallInstance& wall) {
                            return wall.id ==
                                   *recentlyDamagedBuilding_;
                        }) ||
                    std::any_of(
                        healthBarSnapshot.ramps.begin(),
                        healthBarSnapshot.ramps.end(),
                        [this](const RampInstance& ramp) {
                            return ramp.id ==
                                   *recentlyDamagedBuilding_;
                        });
                if (isModular) {
                    healthBarSnapshot
                        .aimedModularBuilding =
                        recentlyDamagedBuilding_;
                }
            }
        }
        renderer_->endWorldPass();

        // World-space UI is composited after post-processing so health bars,
        // levels and production rewards remain crisp above pixelization.
        BeginMode3D(camera);
        targetHealthBar_.draw(healthBarSnapshot, camera);
        drawProductionVisuals(camera);
        EndMode3D();

        if (playerDamageFlashRemaining_ > 0.0) {
            const auto alpha = static_cast<unsigned char>(
                90.0 * playerDamageFlashRemaining_ / 0.18);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          {190, 24, 24, alpha});
        }
        drawFloatingDamageNumbers(camera);
        if (showTerrainWireframe_) {
            drawUiText(
                TextFormat(
                    "TERRAIN SEED: %u  CTRL+F7 SAME  CTRL+F9 NEW",
                    snapshot.terrainSeed),
                {24.0F,
                 static_cast<float>(
                     GetScreenHeight()) -
                     44.0F},
                16.0F, {245, 224, 154, 235});
        }
        if (showColliders_) {
            for (const SharedSupport& support :
                 snapshot.sharedSupports) {
                if (!support.active) {
                    continue;
                }
                const Vector2 screen =
                    GetWorldToScreen(
                        {
                            static_cast<float>(
                                support.top.x),
                            static_cast<float>(
                                support.top.y + 0.18),
                            static_cast<float>(
                                support.top.z),
                        },
                        camera);
                if (screen.x < 0.0F ||
                    screen.y < 0.0F ||
                    screen.x >
                        static_cast<float>(
                            GetScreenWidth()) ||
                    screen.y >
                        static_cast<float>(
                            GetScreenHeight())) {
                    continue;
                }
                drawUiText(
                    TextFormat(
                        "S%u x%u", support.id,
                        support.referenceCount),
                    screen, 12.0F,
                    {255, 213, 91, 235});
            }
        }

        auto hudSnapshot = presentationSnapshot;
        bool showBuildingContextCard =
            buildingContextCardTarget_.has_value();
        if (buildingContextCardTarget_) {
            const auto pinned = std::find_if(
                snapshot.buildings.begin(),
                snapshot.buildings.end(),
                [this](const BuildingInstance& candidate) {
                    return candidate.id ==
                           *buildingContextCardTarget_;
                });
            if (pinned == snapshot.buildings.end()) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                showBuildingContextCard = false;
            } else {
                hudSnapshot.aimedBuilding =
                    buildingContextCardTarget_;
                hudSnapshot.aimedBuildingUpgradeCost =
                    buildingContextCardUpgradeCost_;
                hudSnapshot.aimedBuildingStats =
                    buildingContextCardStats_;
            }
        }
        drawHud(
            ui_, hudSnapshot,
            {
                    .damageIndicators = damageIndicators_,
                    .statusMessage = statusMessage_,
                    .statusMessageRemaining = statusMessageRemaining_,
                    .debugSpawnType = debugSpawnType_,
                    .slowMotion = slowMotion_,
                    .showColliders = showColliders_,
                    .showFlowField = showFlowField_,
                    .showSpatialHash = showSpatialHash_,
                    .hideBottomHints = hideBottomHud_,
                    .foundationBuildMode =
                        foundationBuildMode_,
                    .selectedModularBuildPiece =
                        static_cast<std::size_t>(
                            modularBuildPiece_),
                    .buildHotbarSelectionPosition =
                        buildHotbarSelectionPosition_,
                    .buildHotbarSelectionAlpha =
                        buildHotbarSelectionAlpha_,
                    .foundationHotbarSelectionPosition =
                        foundationHotbarSelectionPosition_,
                    .foundationHotbarSelectionAlpha =
                        foundationHotbarSelectionAlpha_,
                    .showBuildingContextCard =
                        showBuildingContextCard,
                    .repairSweepActive =
                        repairSweepActive_,
                    .simulationTickMilliseconds =
                        simulationTickMilliseconds_,
                    .peakSimulationTickMilliseconds =
                        peakSimulationTickMilliseconds_,
                    .woodResourceBounce =
                        woodHudBounceRemaining_ > 0.0
                            ? static_cast<float>(
                                  std::sin(
                                      (1.0 -
                                       woodHudBounceRemaining_ /
                                           0.28) *
                                      PI) *
                                  10.0)
                            : 0.0F,
                    .stoneResourceBounce =
                        stoneHudBounceRemaining_ > 0.0
                            ? static_cast<float>(
                                  std::sin(
                                      (1.0 -
                                       stoneHudBounceRemaining_ /
                                           0.28) *
                                      PI) *
                                  10.0)
                            : 0.0F,
                    .goldResourceBounce = 0.0F,
                    .woodResourcePulse =
                        static_cast<float>(
                            woodHudBounceRemaining_ / 0.28),
                    .stoneResourcePulse =
                        static_cast<float>(
                            stoneHudBounceRemaining_ / 0.28),
                    .goldResourcePulse = 0.0F,
                    .crosshairHitRemaining =
                        crosshairHitRemaining_,
                    .crosshairHitDuration =
                        crosshairHitDuration_,
                    .crosshairHitCritical =
                        crosshairHitCritical_,
                    .invalidActionRemaining =
                        invalidActionRemaining_,
                    .weaponRecoilAmount =
                        weaponRecoilRemaining_ > 0.0 &&
                                weaponRecoilDuration_ > 0.0
                            ? std::sin(
                                  static_cast<float>(
                                      (1.0 -
                                       weaponRecoilRemaining_ /
                                           weaponRecoilDuration_) *
                                      PI)) *
                                  weaponRecoilStrength_ /
                                  0.11F
                            : 0.0F,
                    .buildingStatsUpgradeEntity =
                        buildingStatsUpgradeEntity_,
                    .buildingStatsUpgradeRemaining =
                        buildingStatsUpgradeRemaining_,
                    .buildingStatsUpgradeDuration =
                        buildingStatsUpgradeDuration_,
                    .environmentProfile =
                        environment_.nearestProfileName(),
                    .environmentTime = environment_.timeOfDay(),
                    .environmentFrozen = environment_.frozen(),
                    .environmentManualOverride =
                        environment_.manualOverride(),
            },
            camera);
        if (foundationBuildMode_) {
            const char* pieceName = "FOUNDATION 2x2";
            int previewStorey = 0;
            bool previewValid = false;
            std::size_t plannedCount = 1U;
            std::optional<ModularPlacementError>
                previewError;
            switch (modularBuildPiece_) {
            case ModularBuildPiece::Foundation:
            case ModularBuildPiece::FloorPlatform:
                pieceName =
                    modularBuildPiece_ ==
                            ModularBuildPiece::Foundation
                        ? "FOUNDATION 2x2"
                        : "FLOOR PLATFORM 2x2";
                if (platformFramePreview_) {
                    previewValid =
                        platformFramePreview_->valid();
                    previewError =
                        platformFramePreview_->error;
                    previewStorey =
                        platformFramePreview_->storey;
                }
                if (!modularPlatformDragPreviews_.empty()) {
                    plannedCount =
                        modularPlatformDragPreviews_.size();
                    previewStorey =
                        modularPlatformDragPreviews_
                            .front()
                            .storey;
                    const auto invalid = std::find_if(
                        modularPlatformDragPreviews_.begin(),
                        modularPlatformDragPreviews_.end(),
                        [](const PlatformFramePlacement&
                               placement) {
                            return !placement.valid();
                        });
                    previewValid =
                        invalid ==
                        modularPlatformDragPreviews_.end();
                    previewError =
                        previewValid
                            ? ModularPlacementError::None
                            : invalid->error;
                }
                break;
            case ModularBuildPiece::Wall:
                pieceName = "WALL";
                if (wallPreview_) {
                    previewValid =
                        wallPreview_->valid();
                    previewError =
                        wallPreview_->error;
                    previewStorey =
                        wallPreview_->storey;
                }
                if (!modularWallDragPreviews_.empty()) {
                    plannedCount =
                        modularWallDragPreviews_.size();
                    previewStorey =
                        modularWallDragPreviews_
                            .front()
                            .storey;
                    const auto invalid = std::find_if(
                        modularWallDragPreviews_.begin(),
                        modularWallDragPreviews_.end(),
                        [](const WallPlacement& placement) {
                            return !placement.valid();
                        });
                    previewValid =
                        invalid ==
                        modularWallDragPreviews_.end();
                    previewError =
                        previewValid
                            ? ModularPlacementError::None
                            : invalid->error;
                }
                break;
            case ModularBuildPiece::Ramp:
                pieceName = "RAMP";
                if (rampPreview_) {
                    previewValid =
                        rampPreview_->valid();
                    previewError =
                        rampPreview_->error;
                    previewStorey =
                        rampPreview_->targetStorey;
                }
                if (!modularRampDragPreviews_.empty()) {
                    plannedCount =
                        modularRampDragPreviews_.size();
                    previewStorey =
                        modularRampDragPreviews_
                            .front()
                            .targetStorey;
                    const auto invalid = std::find_if(
                        modularRampDragPreviews_.begin(),
                        modularRampDragPreviews_.end(),
                        [](const RampPlacement& placement) {
                            return !placement.valid();
                        });
                    previewValid =
                        invalid ==
                        modularRampDragPreviews_.end();
                    previewError =
                        previewValid
                            ? ModularPlacementError::None
                            : invalid->error;
                }
                break;
            }
            std::string pieceLabel = pieceName;
            if (modularDragPiece_) {
                pieceLabel += " x" +
                    std::to_string(plannedCount);
            }
            std::string foundationHint =
                pieceLabel + "   LEVEL " +
                std::to_string(previewStorey) +
                "   LMB DRAG";
            foundationHint += "   V PIECE";
            if (modularBuildPiece_ ==
                    ModularBuildPiece::Wall ||
                modularBuildPiece_ ==
                    ModularBuildPiece::Ramp) {
                foundationHint += "   WHEEL ROTATE";
            }
            foundationHint += "   RMB CANCEL";
            const Color messageColor =
                previewValid
                    ? Color{126, 239, 151, 255}
                    : Color{246, 112, 94, 255};
            drawCenteredUiText(
                foundationHint,
                static_cast<float>(
                    GetScreenHeight() / 2 + 76),
                18.0F, {245, 235, 214, 245});
            drawCenteredUiText(
                modularBuildPiece_ ==
                            ModularBuildPiece::
                                FloorPlatform &&
                        !previewError
                    ? "AIM AT PLATFORM OR RAMP"
                    : modularPlacementMessage(
                          previewError),
                static_cast<float>(
                    GetScreenHeight() / 2 + 102),
                18.0F, messageColor);
            if (modularDragPiece_) {
                const ResourceCost cost =
                    snapshot.modularBuildingCosts[
                        static_cast<std::size_t>(
                            *modularDragPiece_)];
                const int count =
                    static_cast<int>(plannedCount);
                const std::string lineCost =
                    std::to_string(count) +
                    " PIECES    W:" +
                    std::to_string(cost.wood * count) +
                    "  S:" +
                    std::to_string(cost.stone * count) +
                    "  C:" +
                    std::to_string(cost.gold * count);
                constexpr float Width = 470.0F;
                const float x =
                    static_cast<float>(GetScreenWidth()) *
                        0.5F -
                    Width * 0.5F;
                const float y =
                    static_cast<float>(GetScreenHeight()) *
                        0.5F +
                    130.0F;
                ui_.drawPanel(
                    {x, y, Width, 54.0F}, 230);
                const float textWidth =
                    measureUiText(lineCost, 15.0F).x;
                drawUiText(
                    lineCost,
                    {x + (Width - textWidth) * 0.5F,
                     y + 12.0F},
                    15.0F, {255, 235, 184, 255});
            }
        }
        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_) {
            const auto cells =
                placementLine(
                    *placementDragType_, *wallDragStart_,
                    *wallDragEnd_, placementDragAxis_);
            const ResourceCost buildingCost =
                snapshot.buildingCosts[
                    static_cast<std::size_t>(
                        *placementDragType_)];
            const int count =
                static_cast<int>(cells.size());
            const std::string lineCost =
                std::to_string(count) +
                " BUILDINGS    W:" +
                std::to_string(buildingCost.wood * count) +
                "  S:" +
                std::to_string(buildingCost.stone * count) +
                "  C:" +
                std::to_string(buildingCost.gold * count);
            constexpr float Width = 470.0F;
            const float x =
                static_cast<float>(GetScreenWidth()) * 0.5F -
                Width * 0.5F;
            const float y =
                static_cast<float>(GetScreenHeight()) * 0.5F +
                104.0F;
            ui_.drawPanel({x, y, Width, 58.0F}, 230);
            const float textWidth =
                measureUiText(lineCost, 15.0F).x;
            drawUiText(
                lineCost,
                {x + (Width - textWidth) * 0.5F,
                 y + 14.0F},
                15.0F, {255, 235, 184, 255});
        }
        if (removalDragActive_) {
            const std::string removalHint =
                "REMOVE x" +
                std::to_string(
                    removalDragTargets_.size()) +
                "   RELEASE X TO CONFIRM";
            drawCenteredUiText(
                removalHint,
                static_cast<float>(
                    GetScreenHeight() / 2 + 76),
                20.0F, {255, 104, 91, 255});
        }
        if (!structuralRiskIds_.empty()) {
            drawCenteredUiText(
                "COLLAPSE RISK: " +
                    std::to_string(
                        structuralRiskIds_.size()) +
                    " DEPENDENT PARTS",
                static_cast<float>(
                    GetScreenHeight() / 2 +
                    (removalDragActive_ ? 106 : 132)),
                17.0F, {255, 197, 82, 255});
        }
        drawResourceGainVisuals(camera);

        drawRunStateOverlay(snapshot);
    }

    drawBuildModePie();
    if (renderer_->graphicsPanelVisible()) {
        drawGraphicsPanel();
    }
    drawEnemySpawnMenu();
    drawUiText(TextFormat("%d FPS", GetFPS()),
               {static_cast<float>(GetScreenWidth() - 110),
                20.0F},
               20.0F, LIME);
    renderer_->endFrame();
}

void App::drawBuildModePie() const {
    if (!buildModePieVisible_) {
        return;
    }

    constexpr float OuterRadius = 150.0F;
    constexpr float InnerRadius = 42.0F;
    constexpr float ArrowRadius = 78.0F;
    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    const bool buildingsSelected =
        buildModePieChoice_ ==
        BuildModePieChoice::Buildings;
    const bool foundationsSelected =
        buildModePieChoice_ ==
        BuildModePieChoice::Foundations;

    DrawCircleV(center, OuterRadius + 7.0F,
                {247, 224, 173, 95});
    DrawCircleV(center, OuterRadius,
                {15, 18, 25, 238});
    DrawCircleSector(
        center, OuterRadius - 5.0F, 90.0F,
        270.0F, 48,
        buildingsSelected
            ? Color{239, 197, 101, 225}
            : Color{52, 62, 78, 220});
    DrawCircleSector(
        center, OuterRadius - 5.0F, -90.0F,
        90.0F, 48,
        foundationsSelected
            ? Color{239, 197, 101, 225}
            : Color{52, 62, 78, 220});
    DrawLineEx(
        {center.x, center.y - OuterRadius + 5.0F},
        {center.x, center.y - InnerRadius},
        3.0F, {20, 24, 32, 180});
    DrawLineEx(
        {center.x, center.y + InnerRadius},
        {center.x, center.y + OuterRadius - 5.0F},
        3.0F, {20, 24, 32, 180});
    DrawCircleV(center, InnerRadius + 4.0F,
                {247, 224, 173, 130});
    DrawCircleV(center, InnerRadius,
                {20, 24, 32, 255});

    const auto drawLabel =
        [](std::string_view label, Vector2 position,
           bool selected, bool activeMode) {
            const Color color =
                selected
                    ? Color{31, 27, 20, 255}
                    : Color{242, 232, 211, 255};
            const Vector2 size =
                measureUiText(label, 16.0F);
            drawUiText(
                label,
                {position.x - size.x * 0.5F,
                 position.y - size.y * 0.5F},
                16.0F, color);
            if (activeMode) {
                DrawCircleV(
                    {position.x,
                     position.y + 37.0F},
                    5.0F,
                    selected
                        ? Color{31, 27, 20, 255}
                        : Color{239, 197, 101, 255});
            }
        };
    drawLabel("BUILDINGS",
              {center.x - 93.0F, center.y},
              buildingsSelected,
              !foundationBuildMode_);
    drawLabel("PLATFORMS",
              {center.x + 93.0F, center.y},
              foundationsSelected,
              foundationBuildMode_);

    const float length =
        Vector2Length(buildModePieDirection_);
    if (length > 1.0F) {
        const Vector2 direction =
            Vector2Scale(
                buildModePieDirection_,
                1.0F / length);
        const Vector2 arrowCenter =
            Vector2Add(
                center,
                Vector2Scale(direction, ArrowRadius));
        const Vector2 tip =
            Vector2Add(
                arrowCenter,
                Vector2Scale(direction, 17.0F));
        const Vector2 arrowBase =
            Vector2Subtract(
                arrowCenter,
                Vector2Scale(direction, 2.0F));
        const Vector2 tail =
            Vector2Subtract(
                arrowCenter,
                Vector2Scale(direction, 13.0F));
        const Vector2 perpendicular{
            -direction.y, direction.x};
        const Color arrowColor{
            255, 247, 224, 255};
        DrawLineEx(tail, arrowBase, 7.0F,
                   arrowColor);
        DrawTriangle(
            tip,
            Vector2Add(
                arrowBase,
                Vector2Scale(perpendicular, 9.0F)),
            Vector2Subtract(
                arrowBase,
                Vector2Scale(perpendicular, 9.0F)),
            arrowColor);
    } else {
        DrawCircleV(center, 7.0F,
                    {255, 247, 224, 255});
    }

    drawCenteredUiText(
        "HOLD TAB  |  RELEASE TO SELECT",
        center.y + OuterRadius + 20.0F,
        14.0F, {242, 232, 211, 235});
}


} // namespace ian
