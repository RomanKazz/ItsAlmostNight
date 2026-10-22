#include "app/AppRenderSupport.hpp"
#include "buildings/RepairStation.hpp"
#include "buildings/DecoyBeacon.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/TowerRig.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/BuildingFoundationVisual.hpp"
#include "graphics/PortalArrivalEffect.hpp"

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
    case EnemyType::AirTank: return EnemyModelVisual::AirTank;
    case EnemyType::Bee: return EnemyModelVisual::Bee;
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

EnemyModelVisual enemyModelVisual(const EnemyInstance& enemy) {
    if(enemy.type==EnemyType::Bee) return enemy.eliteAffixes?EnemyModelVisual::BeeElite:EnemyModelVisual::Bee;
    return enemy.type==EnemyType::Flying && enemy.summonerId
        ? EnemyModelVisual::BossSummon : enemyModelVisual(enemy.type);
}

EnemyAnimationVisual enemyAnimationVisual(
    const EnemyInstance& enemy) {
    if (enemyStatusEffect(enemy, StatusEffectType::Freeze).remaining > 0.0 ||
        (enemy.type == EnemyType::Splitter && enemy.splitAnimationRemaining > 0.0)) {
        return EnemyAnimationVisual::Idle;
    }
    if (enemy.type == EnemyType::AirTank) {
        if (enemy.airTankWindupRemaining > 0 || enemy.attackCooldownRemaining > 1.7)
            return EnemyAnimationVisual::MeleeAttack;
        return enemy.state == EnemyState::MoveToCore ? EnemyAnimationVisual::Walk : EnemyAnimationVisual::Idle;
    }
    if(enemy.type==EnemyType::Bee) {
        if(enemy.beeWindupRemaining>0 || (enemy.attackCooldownRemaining>2.6 && enemy.beeRepositionRemaining>0))
            return EnemyAnimationVisual::RangedAttack;
        return enemy.state==EnemyState::MoveToCore?EnemyAnimationVisual::Run:EnemyAnimationVisual::Idle;
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
    case EnemyState::BossVolleyWindup:
    case EnemyState::BossStormWindup:
        if (enemy.type==EnemyType::Boss && enemy.state!=EnemyState::BossSlamWindup)
            return EnemyAnimationVisual::RangedAttack;
        if (enemy.type == EnemyType::Ranged || enemy.type==EnemyType::Bee) {
            return EnemyAnimationVisual::RangedAttack;
        }
        if (enemy.type == EnemyType::Sapper) {
            return EnemyAnimationVisual::SapperAttack;
        }
        return EnemyAnimationVisual::MeleeAttack;
    case EnemyState::Spawn:
        return EnemyAnimationVisual::Spawn;
    case EnemyState::Dead:
        return EnemyAnimationVisual::Idle;
    }
    return EnemyAnimationVisual::Idle;
}

float enemyVisualScale(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.55F;
    case EnemyType::AirTank: return .90F;
    case EnemyType::Bee: return .70F;
    case EnemyType::Flying:
        return 0.52F;
    case EnemyType::Heavy:
        return 0.67F;
    case EnemyType::Boss:
        return 1.35F;
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

float enemyAppearanceScale(const EnemyInstance&) {
    // Split squash/stretch now uses the shared foot-pivot pose in every pass.
    return 1.0F;
}

SplitterPose enemySplitPose(const EnemyInstance& enemy, double subTickSeconds) {
    if(enemy.portalArrivalRemaining>0) return portal_arrival::pose(static_cast<float>(
        1-std::max(0.0,enemy.portalArrivalRemaining-subTickSeconds)/EnemyPortalArrivalDuration),enemy.type==EnemyType::Boss);
    if (enemy.type == EnemyType::Splitter && enemy.splitAnimationRemaining > 0.0) {
        return splitterPose(true, static_cast<float>(
            1.0 - (enemy.splitAnimationRemaining - subTickSeconds) / 0.38));
    }
    if (enemy.type == EnemyType::Splitling && enemy.spawnAnimationRemaining > 0.0) {
        auto pose = splitterPose(false, static_cast<float>(
            1.0 - (enemy.spawnAnimationRemaining - subTickSeconds) / 0.72));
        if (enemy.id.index % 2U == 0U) pose.roll = -pose.roll;
        return pose;
    }
    return {};
}

double enemySplitSubTickSeconds(const SimulationSnapshot& snapshot, double interpolationAlpha) {
    return acceptsGameplayInput(snapshot.state) && !snapshot.runUpgradeChoicePending
        ? std::clamp(interpolationAlpha, 0.0, 1.0) * FixedStep::TickSeconds : 0.0;
}

Vector3 enemyRenderPosition(const EnemyInstance& enemy, double subTickSeconds) {
    const double arrival=std::clamp((enemy.portalArrivalRemaining-subTickSeconds)/EnemyPortalArrivalDuration,0.0,1.0);
    const double slip=.65*arrival*arrival;
    return {
        static_cast<float>(enemy.position.x-std::sin(enemy.portalExitYaw)*slip),
        enemy.type == EnemyType::Boss
            ? static_cast<float>(enemy.bossHoverOffset)
            : enemy.type == EnemyType::AirTank ? .50F
            : enemy.type == EnemyType::Bee ? .80F
            : enemy.type == EnemyType::Flying
            ? 1.25F
            : static_cast<float>(0.02 + enemy.surfaceHeightOffset) +
                enemySplitPose(enemy, subTickSeconds).hop,
        static_cast<float>(enemy.position.z-std::cos(enemy.portalExitYaw)*slip),
    };
}

float enemyAnimationSeconds(
    const EnemyInstance& enemy, double elapsedSeconds, double subTickSeconds) {
    if (enemyStatusEffect(enemy, StatusEffectType::Freeze).remaining > 0.0) return 0.0F;
    if (enemy.type == EnemyType::AirTank) {
        if (enemy.airTankWindupRemaining > 0)
            return static_cast<float>(AirTankWindupSeconds - enemy.airTankWindupRemaining + subTickSeconds);
        if (enemy.attackCooldownRemaining > 1.7)
            return static_cast<float>(AirTankWindupSeconds + AirTankRecoverySeconds - enemy.attackCooldownRemaining + subTickSeconds);
        return static_cast<float>((elapsedSeconds + subTickSeconds) * .7);
    }
    if(enemy.type==EnemyType::Bee) {
        const double windup=enemy.eliteAffixes?1.0:.75;
        if(enemy.beeWindupRemaining>0)return static_cast<float>(windup-enemy.beeWindupRemaining+subTickSeconds);
        if(enemy.attackCooldownRemaining>2.6 && enemy.beeRepositionRemaining>0)
            return static_cast<float>(windup+2.8-enemy.attackCooldownRemaining+subTickSeconds);
        return static_cast<float>((elapsedSeconds+subTickSeconds)*enemy.locomotionRate);
    }
    if (enemy.state == EnemyState::BossRamWindup) {
        return static_cast<float>(
            std::max(
                0.0,
                enemy.ramWindup -
                    enemy.ramWindupRemaining + subTickSeconds));
    }
    if (enemy.state == EnemyState::BossVolleyWindup ||
        enemy.state == EnemyState::BossStormWindup ||
        enemy.state == EnemyState::BossSlamWindup ||
        enemy.state == EnemyState::BossWarCryWindup ||
        enemy.state == EnemyState::BossPhaseTransition) {
        return static_cast<float>(std::max(
            0.0, 1.5 - enemy.bossAbilityWindupRemaining + subTickSeconds));
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
                    enemy.attackCooldownRemaining + subTickSeconds));
    }
    return static_cast<float>(
        (elapsedSeconds + subTickSeconds) * enemy.locomotionRate +
        static_cast<double>(enemy.id.index % 4U) *
            0.173);
}

double enemyAnimationSubTickSeconds(const SimulationSnapshot& snapshot, double interpolationAlpha) {
    return acceptsGameplayInput(snapshot.state) && !snapshot.runUpgradeChoicePending
        ? std::clamp(interpolationAlpha, 0.0, 1.0) * FixedStep::TickSeconds : 0.0;
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
    case UpgradeError::CardRequired:
        return "Upgrade this defense with cards";
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
                const BuildingInstance& building, float interpolationAlpha) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    if (runtime != snapshot.cannons.end()) {
        if (!runtime->targetId) return static_cast<float>(runtime->yaw);
        return static_cast<float>(wrapBuildingAngle(runtime->previousYaw +
            wrapBuildingAngle(runtime->yaw - runtime->previousYaw) *
            std::clamp(interpolationAlpha, 0.0F, 1.0F)));
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

float windBlowerRotor(const SimulationSnapshot& snapshot, const BuildingInstance& building, float interpolationAlpha) {
    const auto tower = std::ranges::find(snapshot.towers,building.id,&TowerRuntime::buildingId);
    return tower == snapshot.towers.end() ? 0.0F : static_cast<float>(wrapBuildingAngle(
        tower->previousWindRotor + wrapBuildingAngle(tower->windRotor-tower->previousWindRotor) *
        std::clamp(interpolationAlpha,0.0F,1.0F)));
}

float towerYaw(const SimulationSnapshot& snapshot,
               const BuildingInstance& building, float interpolationAlpha) {
    const auto runtime = std::find_if(
        snapshot.towers.begin(), snapshot.towers.end(),
        [&building](const TowerRuntime& tower) {
            return tower.buildingId == building.id;
        });
    if (runtime != snapshot.towers.end()) {
        if (!runtime->targetId &&
            std::abs(wrapBuildingAngle(runtime->baseYaw - runtime->restYaw)) > 0.0001) {
            return static_cast<float>(runtime->yaw);
        }
        return static_cast<float>(wrapBuildingAngle(runtime->previousYaw +
            wrapBuildingAngle(runtime->yaw - runtime->previousYaw) *
            std::clamp(interpolationAlpha, 0.0F, 1.0F)));
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
                 const BuildingInstance& building, float interpolationAlpha) {
    const auto runtime = std::find_if(
        snapshot.towers.begin(), snapshot.towers.end(),
        [&building](const TowerRuntime& tower) {
            return tower.buildingId == building.id;
        });
    if (runtime == snapshot.towers.end()) return 0.0F;
    return static_cast<float>(runtime->previousPitch +
        (runtime->pitch - runtime->previousPitch) *
        std::clamp(interpolationAlpha, 0.0F, 1.0F));
}

float cannonPitch(const SimulationSnapshot& snapshot,
                  const BuildingInstance& building, float interpolationAlpha) {
    const auto runtime = std::find_if(
        snapshot.cannons.begin(), snapshot.cannons.end(),
        [&building](const CannonRuntime& cannon) {
            return cannon.buildingId == building.id;
        });
    if (runtime == snapshot.cannons.end()) return 0.0F;
    return static_cast<float>(runtime->previousPitch +
        (runtime->pitch - runtime->previousPitch) *
        std::clamp(interpolationAlpha, 0.0F, 1.0F));
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
    const SimulationSnapshot& snapshot,
    const std::function<float(EntityId, bool)>&
        gateOpenAmount,
    const std::function<float(const BuildingInstance&)>& flamethrowerYaw) {
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
        float yaw = static_cast<float>(buildingRotationYaw(building.type, building.rotation));
        float pitch = 0.0F;
        if (building.type == BuildingType::Turret ||
            building.type == BuildingType::GunTurret ||
            building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::LaserTower || building.type == BuildingType::WindBlower) {
            yaw = towerYaw(snapshot, building);
            if (building.type == BuildingType::Turret || building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::LaserTower || building.type == BuildingType::WindBlower) {
                pitch = towerPitch(snapshot, building);
            }
        } else if (
            building.type == BuildingType::Cannon ||
            building.type == BuildingType::Catapult || building.type == BuildingType::Mortar) {
            yaw = cannonYaw(snapshot, building);
            pitch = cannonPitch(snapshot, building);
        }
        if (building.type == BuildingType::Flamethrower && flamethrowerYaw) {
            yaw = flamethrowerYaw(building);
        }
        const auto distance =
            renderer.buildingRaycastDistance(
                building, snapshot.buildings, ray,
                MaximumDistance, yaw, pitch,
                building.type == BuildingType::Gate &&
                        gateOpenAmount
                    ? gateOpenAmount(
                          building.id, building.open)
                    : building.open ? 1.0F : 0.0F,
                building.type == BuildingType::SpikeTrap
                    ? spikeTrapAnimationSeconds(snapshot, building.id)
                    : -1.0F,
                (building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::LaserTower || building.type == BuildingType::WindBlower)
                    ? std::optional<float>{towerBaseYaw(snapshot, building)}
                    : building.type == BuildingType::Mortar ? std::optional<float>{cannonBaseYaw(snapshot, building)} : std::nullopt,
                windBlowerRotor(snapshot, building));
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = building.id;
        }
    }
    return result;
}

std::optional<ResourceAimHit> preciseResourceAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot,
    PlayerWeapon weapon) {
    const bool ranged = weapon == PlayerWeapon::Rifle ||
        weapon == PlayerWeapon::IceWand ||
        weapon == PlayerWeapon::FireWand;
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
    std::optional<ResourceAimHit> result;
    double closestDistance = MaximumDistance*DestructiblePropInteractionRangeMultiplier;
    for (const ResourceNode& node : snapshot.resourceNodes) {
        if (!node.active) {
            continue;
        }
        const double offsetX =
            node.position.x - snapshot.playerPosition.x;
        const double offsetZ =
            node.position.z - snapshot.playerPosition.z;
        constexpr double BroadPhaseRadius = MaximumDistance*DestructiblePropInteractionRangeMultiplier + 3.0;
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
            node.type, position, ray, isDestructibleProp(node.type)
                ? MaximumDistance*DestructiblePropInteractionRangeMultiplier : MaximumDistance,
            node.visualVariant,
            static_cast<float>(node.visualScale),
            static_cast<float>(node.visualYaw));
        if (distance && *distance < closestDistance) {
            closestDistance = *distance;
            result = ResourceAimHit{
                .id = node.id,
                .position = {
                    static_cast<double>(ray.position.x) +
                        static_cast<double>(ray.direction.x) * *distance,
                    static_cast<double>(ray.position.y) +
                        static_cast<double>(ray.direction.y) * *distance,
                    static_cast<double>(ray.position.z) +
                        static_cast<double>(ray.direction.z) * *distance,
                },
            };
        }
    }
    if (ranged && result) {
        const auto node=std::ranges::find(snapshot.resourceNodes,result->id,&ResourceNode::id);
        if (node==snapshot.resourceNodes.end() || !isDestructibleProp(node->type)) return std::nullopt;
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
        if (landmark.activated) continue;
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
    std::optional<EntityId> result;
    double closestDistance = MaximumDistance;
    const auto consider =
        [&result, &closestDistance](
            EntityId id, std::optional<double> distance) {
            if (distance && *distance <= closestDistance) {
                closestDistance = *distance;
                result = id;
            }
        };

    const auto supportVisibility =
        platformSupportVisibilityMasks(
            snapshot.platformFrames,
            snapshot.sharedSupports);
    const auto visualSupportLengths =
        platformSupportVisualLengths(
            snapshot.platformFrames,
            snapshot.sharedSupports);
    for (std::size_t frameIndex = 0;
         frameIndex < snapshot.platformFrames.size();
         ++frameIndex) {
        const PlatformFrameInstance& frame =
            snapshot.platformFrames[frameIndex];
        const double minimumX = std::min(
            frame.supports[0].top.x,
            frame.supports[2].top.x);
        const double maximumX = std::max(
            frame.supports[1].top.x,
            frame.supports[3].top.x);
        const double minimumZ = std::min(
            frame.supports[0].top.z,
            frame.supports[1].top.z);
        const double maximumZ = std::max(
            frame.supports[2].top.z,
            frame.supports[3].top.z);
        std::array<float, 4> supportLengths{};
        for (std::size_t index = 0;
             index < frame.supports.size(); ++index) {
            supportLengths[index] = static_cast<float>(
                std::max(
                    visualSupportLengths[frameIndex][index],
                    0.0));
        }
        consider(
            frame.id,
            renderer.platformFrameRaycastDistance(
                {static_cast<float>(
                     (minimumX + maximumX) * 0.5),
                 static_cast<float>(frame.floorHeight),
                 static_cast<float>(
                     (minimumZ + maximumZ) * 0.5)},
                static_cast<float>(maximumX - minimumX) * 0.5F,
                supportLengths, ray, closestDistance,
                supportVisibility[frameIndex]));
    }

    const double cellSize = snapshot.worldCellSize;
    for (const WallInstance& wall : snapshot.modularWalls) {
        const bool alongX =
            wall.rotation == Rotation::Deg0 ||
            wall.rotation == Rotation::Deg180;
        const float centerX = static_cast<float>(
            (wall.anchor.x + 0.5) * cellSize);
        const float centerZ = static_cast<float>(
            (wall.anchor.z + 0.5) * cellSize);
        const float halfLength =
            static_cast<float>(cellSize * 0.5);
        constexpr float HalfThickness = 0.09F;
        const BoundingBox bounds{
            {centerX - (alongX ? halfLength : HalfThickness),
             static_cast<float>(wall.bottomHeight),
             centerZ - (alongX ? HalfThickness : halfLength)},
            {centerX + (alongX ? halfLength : HalfThickness),
             static_cast<float>(wall.topHeight),
             centerZ + (alongX ? HalfThickness : halfLength)},
        };
        const RayCollision hit = GetRayCollisionBox(ray, bounds);
        consider(
            wall.id,
            hit.hit && hit.distance >= 0.0F
                ? std::optional<double>{hit.distance}
                : std::nullopt);
    }

    for (const RampInstance& ramp : snapshot.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        const Vector3 center{
            static_cast<float>(
                (ramp.anchor.x + widthCells * 0.5) * cellSize),
            static_cast<float>(ramp.bottomHeight),
            static_cast<float>(
                (ramp.anchor.z + depthCells * 0.5) * cellSize),
        };
        const float yaw =
            -static_cast<float>(ramp.rotation) * PI * 0.5F;
        consider(
            ramp.id,
            renderer.rampRaycastDistance(
                center, yaw, static_cast<float>(cellSize),
                ray, closestDistance));
    }
    return result;
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
    constexpr float FadeStart = 7.0F;
    constexpr float FadeEnd = 13.0F;
    constexpr float GridTerrainOffset = 0.045F;
    constexpr float MinorOpacity = 0.10F;
    constexpr float MajorOpacity = 0.20F;
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
        border ? 165 : 34;
    switch (error) {
    case PlacementError::None:
        return border ? Color{62, 218, 119, 165}
                      : Color{48, 196, 99, 30};
    case PlacementError::InsufficientResources:
        return {255, 176, 62, alpha};
    case PlacementError::OutsideCoreArea:
        return {187, 104, 255, alpha};
    case PlacementError::OutOfRange:
        return {94, 172, 255, alpha};
    case PlacementError::PlayerOverlap:
        return {255, 222, 82, alpha};
    case PlacementError::CoreRequired:
    case PlacementError::BlueprintRequired:
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
    Vector2 visualCenter, float /*visualYaw*/) {
    const float x = visualCenter.x;
    const float z = visualCenter.y;
    const float halfExtent = static_cast<float>(
        buildingFootprintHalfExtent(preview.type));
    const Color border =
        placementColor(preview.placement.error, true);
    const float height =
        static_cast<float>(preview.baseHeight) +
        0.055F;
    const float borderHeight = height + 0.018F;
    constexpr float BorderRadius = 0.006F;

    const auto drawBorder = [&](Vector3 start, Vector3 end) {
        DrawCylinderEx(
            start, end, BorderRadius, BorderRadius, 5, border);
    };

    drawBorder(
        {x - halfExtent, borderHeight, z - halfExtent},
        {x + halfExtent, borderHeight, z - halfExtent});
    drawBorder(
        {x + halfExtent, borderHeight, z - halfExtent},
        {x + halfExtent, borderHeight, z + halfExtent});
    drawBorder(
        {x + halfExtent, borderHeight, z + halfExtent},
        {x - halfExtent, borderHeight, z + halfExtent});
    drawBorder(
        {x - halfExtent, borderHeight, z + halfExtent},
        {x - halfExtent, borderHeight, z - halfExtent});
}

void drawPlacementPreviewGeometry(
    Renderer& renderer, BuildingType type, Vector3 position,
    float yawRadians, std::uint8_t rotation,
    std::uint8_t wallConnections, int level) {
    const float x = position.x;
    const float y = position.y;
    const float z = position.z;
    if (type == BuildingType::Core) {
        if (!renderer.drawCore(position, yawRadians)) {
            DrawCube({x, y + 1.25F, z}, 2.0F, 2.5F, 2.0F, WHITE);
        }
    } else if (type == BuildingType::Turret) {
        if (!renderer.drawCrossbow(position, yawRadians)) {
            DrawCube({x, y + 0.6F, z}, 1.0F, 1.2F, 1.0F, WHITE);
            DrawCylinder({x, y + 1.45F, z}, 0.42F, 0.32F,
                         0.7F, 8, WHITE);
            DrawCube({x, y + 1.55F, z - 0.55F}, 0.18F,
                     0.18F, 1.0F, WHITE);
        }
    } else if (type == BuildingType::WindBlower) {
        static_cast<void>(renderer.drawWindBlower(position,yawRadians,WHITE,1,yawRadians,0,level));
    } else if (type == BuildingType::DecoyBeacon) {
        static_cast<void>(renderer.drawDecoyBeacon(position, yawRadians, WHITE, 1, .5F, level));
    } else if (type == BuildingType::RepairStation) {
        static_cast<void>(renderer.drawRepairStation(position, yawRadians, WHITE, 1, level));
    } else if (type == BuildingType::Mortar) {
        static_cast<void>(renderer.drawMortar(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::LaserTower) {
        static_cast<void>(renderer.drawLaserTower(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::MissileLauncher) {
        static_cast<void>(renderer.drawMissileLauncher(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::Flamethrower) {
        static_cast<void>(renderer.drawFlamethrower(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::GunTurret) {
        if (!renderer.drawGunTurret(
                position, yawRadians, yawRadians)) {
            DrawCube({x, y + 0.55F, z}, 1.1F, 1.1F, 1.1F, WHITE);
        }
    } else if (type == BuildingType::Cannon) {
        if (!renderer.drawCannon(position, yawRadians, 0.0F)) {
            DrawCube({x, y + 0.6F, z}, 1.0F, 1.2F, 1.0F, WHITE);
            DrawSphere({x, y + 1.35F, z}, 0.48F, WHITE);
            DrawCube({x, y + 1.45F, z - 0.75F}, 0.28F,
                     0.28F, 1.4F, WHITE);
        }
    } else if (type == BuildingType::Catapult) {
        if (!renderer.drawCatapult(position, yawRadians, 0.0F, false)) {
            DrawCube({x, y + 0.45F, z}, 1.2F, 0.9F, 1.2F, WHITE);
        }
    } else if (type == BuildingType::TeslaTower) {
        static_cast<void>(renderer.drawTeslaTower(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::MissileLauncher) {
        static_cast<void>(renderer.drawMissileLauncher(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::Flamethrower) {
        static_cast<void>(renderer.drawFlamethrower(position, yawRadians, WHITE, 1, 0, {}, level));
    } else if (type == BuildingType::GunTurret) {
        if (!renderer.drawGunTurret(
                position, yawRadians, yawRadians)) {
            DrawCube({x, y + 0.55F, z}, 1.1F, 1.1F, 1.1F, WHITE);
        }
    } else if (type == BuildingType::Cannon) {
        if (!renderer.drawCannon(position, yawRadians, 0.0F)) {
            DrawCube({x, y + 0.6F, z}, 1.0F, 1.2F, 1.0F, WHITE);
            DrawSphere({x, y + 1.35F, z}, 0.48F, WHITE);
            DrawCube({x, y + 1.45F, z - 0.75F}, 0.28F,
                     0.28F, 1.4F, WHITE);
        }
    } else if (type == BuildingType::Catapult) {
        if (!renderer.drawCatapult(position, yawRadians, 0.0F, false)) {
            DrawCube({x, y + 0.45F, z}, 1.2F, 0.9F, 1.2F, WHITE);
        }
    } else if (
        type == BuildingType::CrystalMine ||
        type == BuildingType::LumberMill ||
        type == BuildingType::Quarry) {
        if (!renderer.drawResourceProducer(type, position, yawRadians, WHITE, 1, level)) {
            DrawCube({x, y + 0.275F, z}, 1.0F, 0.55F, 1.0F, WHITE);
        }
    } else if (
        type == BuildingType::WoodStorage ||
        type == BuildingType::StoneStorage ||
        type == BuildingType::CrystalStorage) {
        DrawCube({x, y + 0.75F, z}, 1.8F, 1.5F, 1.8F, WHITE);
        DrawCube({x, y + 1.62F, z}, 1.25F, 0.24F, 1.25F, WHITE);
    } else if (type == BuildingType::SlowTrap) {
        DrawCube({x, y + 0.08F, z}, 1.0F, 0.16F, 1.0F, WHITE);
    } else if (type == BuildingType::SpikeTrap) {
        if (!renderer.drawSpikeTrap(position, yawRadians, -1.0F)) {
            DrawCube({x, y + 0.08F, z}, 1.0F, 0.16F, 1.0F, WHITE);
        }
    } else if (type == BuildingType::Wall) {
        if (!renderer.drawWall(position, wallConnections, yawRadians)) {
            DrawCube({x, y + 1.0F, z}, 1.0F, 2.0F, 1.0F, WHITE);
        }
    } else if (
        type == BuildingType::Gate &&
        renderer.drawGate(position, yawRadians, 0.0F)) {
        // Imported gate model submitted.
    } else if ((rotation % 2U) == 0U) {
        DrawCube({x - 0.38F, y + 1.0F, z}, 0.22F, 2.0F,
                 1.0F, WHITE);
        DrawCube({x + 0.38F, y + 1.0F, z}, 0.22F, 2.0F,
                 1.0F, WHITE);
        DrawCube({x, y + 1.0F, z}, 0.55F, 1.7F, 0.18F, WHITE);
    } else {
        DrawCube({x, y + 1.0F, z - 0.38F}, 1.0F, 2.0F,
                 0.22F, WHITE);
        DrawCube({x, y + 1.0F, z + 0.38F}, 1.0F, 2.0F,
                 0.22F, WHITE);
        DrawCube({x, y + 1.0F, z}, 0.18F, 1.7F, 0.55F, WHITE);
    }
}

std::optional<PlatformFrameInstance>
automaticBuildingFoundation(
    BuildingType type, GridPosition position,
    double topHeight, double bottomHeight,
    double cellSize, EntityId id) {
    return makeBuildingFoundationVisual(type, position, topHeight, bottomHeight, cellSize, id);
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

void drawBuildingTacticalOverlay(
    Renderer& renderer, const SimulationSnapshot& snapshot) {
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
        building->type == BuildingType::GunTurret || building->type == BuildingType::Flamethrower || building->type == BuildingType::MissileLauncher || building->type == BuildingType::TeslaTower || building->type == BuildingType::LaserTower || building->type == BuildingType::WindBlower) {
        double range = TowerSystem::attackRange(building->type, building->level);
        if ((building->type == BuildingType::MissileLauncher || building->type == BuildingType::TeslaTower || building->type == BuildingType::LaserTower || building->type == BuildingType::WindBlower) && snapshot.aimedFinalAttackStats) range = snapshot.aimedFinalAttackStats->range;
        if ((building->type == BuildingType::Flamethrower || building->type == BuildingType::WindBlower) && snapshot.aimedFinalAttackStats) {
            range = snapshot.aimedFinalAttackStats->range * snapshot.aimedFinalAttackStats->size;
        }
        renderer.drawTacticalGroundSector(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(range),
            towerBaseYaw(snapshot, *building),
            static_cast<float>(defenseAttackArcDegrees(building->type, building->level)),
            {255, 252, 244, 190}, building->type == BuildingType::MissileLauncher
                ? static_cast<float>(MissileLauncherMinimumRange) : 0.0F);
        const auto runtime = std::find_if(
            snapshot.towers.begin(), snapshot.towers.end(),
            [&building](const TowerRuntime& tower) {
                return tower.buildingId == building->id;
            });
        if (runtime != snapshot.towers.end()) {
            targetId = runtime->targetId;
        }
        center.y += building->type == BuildingType::LaserTower ? LaserPitchPivotY :
            building->type == BuildingType::MissileLauncher ? MissileLauncherPitchPivotY :
            building->type == BuildingType::Flamethrower ? FlamethrowerPitchPivotY :
            building->type == BuildingType::GunTurret ? 0.69 : 1.42;
    } else if (building->type == BuildingType::Cannon ||
               building->type == BuildingType::Catapult || building->type == BuildingType::Mortar) {
        renderer.drawTacticalGroundSector(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                (building->type == BuildingType::Mortar && snapshot.aimedFinalAttackStats)
                    ? snapshot.aimedFinalAttackStats->range : CannonSystem::attackRange(
                    building->type, building->level)),
            cannonBaseYaw(snapshot, *building),
            static_cast<float>(defenseAttackArcDegrees(building->type, building->level)),
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
    } else if (building->type == BuildingType::DecoyBeacon) {
        renderer.drawTacticalGroundSector(
            {static_cast<float>(center.x), static_cast<float>(center.y) + .085F, static_cast<float>(center.z)},
            static_cast<float>(decoyBeaconRadius(building->level)), 0, 360, {255, 217, 99, 190});
        return;
    } else if (building->type == BuildingType::RepairStation) {
        renderer.drawTacticalGroundSector(
            {static_cast<float>(center.x), static_cast<float>(center.y) + .085F, static_cast<float>(center.z)},
            static_cast<float>(repairStationRadius(building->level)), 0, 360, {99, 234, 255, 190});
        return;
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
        renderer.drawTacticalGroundSector(
            {static_cast<float>(center.x),
             static_cast<float>(center.y) + 0.085F,
             static_cast<float>(center.z)},
            static_cast<float>(
                TrapSystem::spikeTriggerRadius(
                    building->level)),
            0.0F, 360.0F, {255, 252, 244, 190});
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
    if (building->type != BuildingType::Cannon) {
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
