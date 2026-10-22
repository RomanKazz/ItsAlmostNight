#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/ModularBuildingConstants.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_set>

namespace ian {
namespace {

struct BuildingFootprint {
    double halfExtentX;
    double halfExtentZ;
};

struct SelectionBounds {
    double halfExtentX;
    double halfExtentZ;
    double minimumY;
    double maximumY;
};

constexpr bool isProducer(BuildingType type) {
    return type == BuildingType::CrystalMine ||
           type == BuildingType::LumberMill ||
           type == BuildingType::Quarry;
}

BuildingFootprint footprint(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
    case BuildingType::Turret:
    case BuildingType::GunTurret:
    case BuildingType::Cannon:
    case BuildingType::TeslaTower:
    case BuildingType::LaserTower:
    case BuildingType::Mortar:
    case BuildingType::RepairStation:
    case BuildingType::WindBlower:
    case BuildingType::DecoyBeacon:
    case BuildingType::MissileLauncher:
    case BuildingType::Flamethrower:
    case BuildingType::Catapult:
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        return {1.0, 1.0};
    case BuildingType::CrystalMine:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
    case BuildingType::Wall:
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
    case BuildingType::Gate:
        return {0.5, 0.5};
    }
    return {};
}

SelectionBounds selectionBounds(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return {0.88, 0.88, 0.0, 1.5};
    case BuildingType::Wall:
        return {0.42, 0.42, 0.0, 2.05};
    case BuildingType::Turret:
        return {0.76, 0.76, 0.0, 2.15};
    case BuildingType::GunTurret:
        return {0.56, 0.56, 0.0, 1.12};
    case BuildingType::CrystalMine:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return {0.44, 0.44, 0.0, 0.8};
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        return {0.9, 0.9, 0.0, 1.8};
    case BuildingType::Cannon:
        return {0.82, 0.82, 0.0, 2.05};
    case BuildingType::TeslaTower: return {0.6, 0.6, 0.0, 1.44};
    case BuildingType::LaserTower: return {0.6, 0.6, 0.0, 1.40};
    case BuildingType::Mortar: return {0.6, 0.6, 0.0, 1.45};
    case BuildingType::DecoyBeacon: return {0.64, 0.64, 0.0, 1.47};
    case BuildingType::WindBlower: return {0.61, 0.61, 0.0, 1.34};
    case BuildingType::RepairStation: return {0.6, 0.71, 0.0, 1.5};
    case BuildingType::MissileLauncher: return {0.6, 0.6, 0.0, 1.54};
    case BuildingType::Flamethrower:
    case BuildingType::Catapult:
        return {0.82, 0.82, 0.0, 2.15};
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return {0.44, 0.44, 0.0, 0.45};
    case BuildingType::Gate:
        return {0.46, 0.46, 0.0, 2.05};
    }
    return {};
}

std::optional<double> rayBoxDistance(
    Vec3 origin, Vec3 direction, Vec3 center,
    SelectionBounds bounds) {
    double nearDistance = 0.0;
    double farDistance =
        std::numeric_limits<double>::infinity();
    const auto intersectAxis =
        [&nearDistance, &farDistance](
            double rayOrigin, double rayDirection,
            double minimum, double maximum) {
            constexpr double DirectionEpsilon = 1e-9;
            if (std::abs(rayDirection) <
                DirectionEpsilon) {
                return rayOrigin >= minimum &&
                       rayOrigin <= maximum;
            }
            double near =
                (minimum - rayOrigin) / rayDirection;
            double far =
                (maximum - rayOrigin) / rayDirection;
            if (near > far) {
                std::swap(near, far);
            }
            nearDistance =
                std::max(nearDistance, near);
            farDistance =
                std::min(farDistance, far);
            return nearDistance <= farDistance;
        };

    if (!intersectAxis(
            origin.x, direction.x,
            center.x - bounds.halfExtentX,
            center.x + bounds.halfExtentX) ||
        !intersectAxis(
            origin.y, direction.y,
            center.y + bounds.minimumY,
            center.y + bounds.maximumY) ||
        !intersectAxis(
            origin.z, direction.z,
            center.z - bounds.halfExtentZ,
            center.z + bounds.halfExtentZ)) {
        return std::nullopt;
    }
    return nearDistance;
}

std::size_t buildingTypeIndex(BuildingType type) {
    return static_cast<std::size_t>(type);
}

bool isTowerType(BuildingType type) {
    return type == BuildingType::Turret ||
           type == BuildingType::GunTurret ||
           type == BuildingType::Cannon ||
           type == BuildingType::Catapult ||
           type == BuildingType::Flamethrower || type == BuildingType::MissileLauncher || type == BuildingType::TeslaTower || type == BuildingType::LaserTower || type == BuildingType::Mortar;
}

const auto& defaultBuildingDefinitions() {
    static const auto Definitions = GameBalance::defaults().buildings;
    return Definitions;
}

ResourceCost upgradeCostFor(
    const BuildingInstance& building, ResourceCost base,
    const EconomyBalanceDefinition& economy) {
    static_assert(MaxBuildingLevel - 1 ==
                  std::tuple_size_v<decltype(economy.coreUpgradeCrystals)>);
    static_assert(MaxBuildingLevel - 1 ==
                  std::tuple_size_v<decltype(economy.buildingUpgradeCrystalBonus)>);
    if (building.level < 1 || building.level >= buildingLevelLimit(building.type)) {
        return {};
    }
    if (building.type == BuildingType::Core) return {};
    const int step = static_cast<int>(building.level) - 2;
    const double multiplier = building.level <= 2
        ? economy.buildingUpgradeCostMultiplier[building.level - 1]
        : economy.buildingUpgradeCostMultiplier.back() +
              0.45 * step + 0.10 * step * step;
    const bool crystalFree = building.type == BuildingType::Wall ||
                             building.type == BuildingType::Gate ||
                             building.type == BuildingType::Turret ||
                             building.type == BuildingType::SlowTrap ||
                             building.type == BuildingType::SpikeTrap;
    const int crystalBonus = crystalFree ? 0 :
        economy.buildingUpgradeCrystalBonus[std::min<std::size_t>(
            building.level - 1, economy.buildingUpgradeCrystalBonus.size() - 1)];
    const auto roundedCost = [multiplier](int amount) {
        if (amount <= 0) return 0;
        // A value such as 25 * 4.4 can land one ULP above 110.
        // Do not charge an extra five resources at that exact boundary.
        return 5 * static_cast<int>(std::ceil(
            std::nextafter(static_cast<double>(amount) * multiplier / 5.0, 0.0)));
    };
    return {
        roundedCost(base.wood),
        roundedCost(base.stone),
        crystalFree ? 0 : roundedCost(base.crystals) + crystalBonus,
    };
}

ResourceCost repairCostFor(
    const BuildingInstance& building, ResourceCost base,
    const EconomyBalanceDefinition& economy) {
    if (building.maxHealth <= 0.0 ||
        building.health >= building.maxHealth) {
        return {};
    }
    const double missingFraction =
        (building.maxHealth - building.health) /
        building.maxHealth;
    const auto scaled =
        [missingFraction, &economy](int amount) {
            return static_cast<int>(std::ceil(
                static_cast<double>(amount) *
                missingFraction *
                economy.repairCostFraction));
        };
    return {
        scaled(base.wood),
        scaled(base.stone),
        0,
    };
}

ResourceCost sellRefundFor(
    ResourceCost base,
    const EconomyBalanceDefinition& economy) {
    const auto scaled =
        [&economy](int amount) {
            return static_cast<int>(
                static_cast<double>(amount) *
                economy.sellRefundFraction);
        };
    return {
        scaled(base.wood),
        scaled(base.stone),
        scaled(base.crystals),
    };
}

int addCostComponents(int left, int right) {
    return static_cast<int>(std::min<long long>(
        std::numeric_limits<int>::max(),
        static_cast<long long>(left) + right));
}

} // namespace

double buildingMaxHealthAtLevel(
    double baseHealth, std::uint8_t level) {
    constexpr std::array<int, MaxBuildingLevel> LevelPercent{
        100, 130, 160, 190, 220, 250, 280, 300};
    const int safeLevel = std::max<int>(level, 1);
    const std::size_t index = static_cast<std::size_t>(
        std::min(safeLevel, static_cast<int>(MaxBuildingLevel)) - 1);
    const int extraPercent = 20 * std::max(0, safeLevel - static_cast<int>(MaxBuildingLevel));
    return 5.0 * std::round(
        baseHealth * (LevelPercent[index] + extraPercent) / 500.0);
}

ResourceCost buildingCost(BuildingType type) {
    const auto& definition = defaultBuildingDefinitions()[buildingTypeIndex(type)];
    return {definition.wood, definition.stone, definition.crystals};
}

BuildingLimitCategory buildingLimitCategory(
    BuildingType type) {
    switch (type) {
    case BuildingType::Turret:
    case BuildingType::GunTurret:
    case BuildingType::Cannon:
    case BuildingType::TeslaTower:
    case BuildingType::LaserTower:
    case BuildingType::Mortar:
    case BuildingType::RepairStation:
    case BuildingType::WindBlower:
    case BuildingType::DecoyBeacon:
    case BuildingType::MissileLauncher:
    case BuildingType::Flamethrower:
    case BuildingType::Catapult:
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return BuildingLimitCategory::Structure;
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
    case BuildingType::Core:
    case BuildingType::Wall:
    case BuildingType::Gate:
    case BuildingType::CrystalMine:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return BuildingLimitCategory::None;
    }
    return BuildingLimitCategory::None;
}

int buildingLimitForCoreLevel(
    BuildingType type, std::uint8_t coreLevel) {
    if (buildingLimitCategory(type) == BuildingLimitCategory::None)
        return std::numeric_limits<int>::max();
    return 5 + 2 * (std::max<int>(coreLevel, 1) - 1);
}

ResourceCost buildingUpgradeCost(const BuildingInstance& building) {
    return upgradeCostFor(
        building, buildingCost(building.type),
        GameBalance::defaults().economy);
}

ResourceCost buildingRepairCost(const BuildingInstance& building) {
    return repairCostFor(
        building, buildingCost(building.type),
        GameBalance::defaults().economy);
}

ResourceCost buildingSellRefund(const BuildingInstance& building) {
    return sellRefundFor(
        buildingCost(building.type),
        GameBalance::defaults().economy);
}

bool buildingBlocksMovement(BuildingType type) {
    return type != BuildingType::SlowTrap &&
           type != BuildingType::SpikeTrap;
}

bool buildingBlocksMovement(const BuildingInstance& building) {
    return buildingBlocksMovement(building.type) &&
           !(building.type == BuildingType::Gate && building.open);
}

int coreResourceCapacity(
    ResourceCapacityType resource, std::uint8_t coreLevel) {
    // Each level has enough room to pay for the following Core upgrade.
    // The player inventory before placing the Core remains deliberately
    // smaller and is handled by Simulation::resourceCapacity().
    constexpr std::array<int, MaxBuildingLevel> Wood{
        250, 250, 400, 600, 850, 1150, 1500, 1900};
    constexpr std::array<int, MaxBuildingLevel> Stone{
        150, 150, 250, 375, 525, 700, 900, 1150};
    constexpr std::array<int, MaxBuildingLevel> Crystal{
        100, 150, 230, 320, 420, 540, 680, 850};
    const std::size_t index = static_cast<std::size_t>(
        std::clamp<int>(coreLevel, 1, MaxBuildingLevel) - 1);
    switch (resource) {
    case ResourceCapacityType::Wood:
        return Wood[index];
    case ResourceCapacityType::Stone:
        return Stone[index];
    case ResourceCapacityType::Crystal:
        return Crystal[index];
    }
    return 0;
}

double buildingFootprintHalfExtent(BuildingType type) {
    return footprint(type).halfExtentX;
}

Vec3 buildingWorldPosition(BuildingType type,
                           GridPosition position) {
    const double cellCenterOffset =
        buildingFootprintHalfExtent(type) == 1.0
            ? 0.0
            : 0.5;
    return {
        static_cast<double>(position.x) + cellCenterOffset,
        0.0,
        static_cast<double>(position.z) + cellCenterOffset,
    };
}

Vec3 buildingWorldPosition(const BuildingInstance& building) {
    Vec3 result = buildingWorldPosition(
        building.type, building.gridPosition);
    result.y = building.baseHeight;
    return result;
}

Vec3 buildingWorldPosition(
    const BuildingDamageResult& building) {
    Vec3 result = buildingWorldPosition(
        building.type, building.gridPosition);
    result.y = building.baseHeight;
    return result;
}

std::uint8_t wallConnectionMask(std::span<const BuildingInstance> buildings,
                                GridPosition position,
                                double baseHeight) {
    constexpr std::uint8_t AllConnections =
        WallConnectionNorth | WallConnectionEast |
        WallConnectionSouth | WallConnectionWest;
    std::uint8_t mask = 0;
    for (const auto& building : buildings) {
        if (building.type != BuildingType::Wall && building.type != BuildingType::Gate) {
            continue;
        }
        if (std::abs(
                building.baseHeight - baseHeight) >
            0.1) {
            continue;
        }
        const int deltaX = building.gridPosition.x - position.x;
        const int deltaZ = building.gridPosition.z - position.z;
        if (building.type == BuildingType::Gate) {
            const bool gateRunsEastWest = (building.rotation % 2U) == 0U;
            if ((deltaX != 0 && !gateRunsEastWest) ||
                (deltaZ != 0 && gateRunsEastWest)) {
                continue;
            }
        }
        if (deltaX == 0 && deltaZ == -1) {
            mask |= WallConnectionNorth;
        } else if (deltaX == 1 && deltaZ == 0) {
            mask |= WallConnectionEast;
        } else if (deltaX == 0 && deltaZ == 1) {
            mask |= WallConnectionSouth;
        } else if (deltaX == -1 && deltaZ == 0) {
            mask |= WallConnectionWest;
        }
        if (mask == AllConnections) {
            break;
        }
    }
    return mask;
}

std::uint8_t wallFallbackRotation(
    std::span<const BuildingInstance> buildings,
    const BuildingInstance& wall) {
    constexpr int MaximumLineGap = 12;
    int nearestHorizontal = std::numeric_limits<int>::max();
    int nearestVertical = std::numeric_limits<int>::max();
    for (const auto& candidate : buildings) {
        if (candidate.id == wall.id ||
            (candidate.type != BuildingType::Wall &&
             candidate.type != BuildingType::Gate)) {
            continue;
        }
        if (std::abs(
                candidate.baseHeight -
                wall.baseHeight) > 0.1) {
            continue;
        }
        const int deltaX = std::abs(
            candidate.gridPosition.x -
            wall.gridPosition.x);
        const int deltaZ = std::abs(
            candidate.gridPosition.z -
            wall.gridPosition.z);
        if (deltaZ == 0 && deltaX > 0 &&
            deltaX <= MaximumLineGap) {
            nearestHorizontal =
                std::min(nearestHorizontal, deltaX);
        }
        if (deltaX == 0 && deltaZ > 0 &&
            deltaZ <= MaximumLineGap) {
            nearestVertical =
                std::min(nearestVertical, deltaZ);
        }
    }
    if (nearestHorizontal < nearestVertical) {
        return 0U;
    }
    if (nearestVertical < nearestHorizontal) {
        return 1U;
    }
    return static_cast<std::uint8_t>(wall.rotation % 2U);
}

GridPosition aimedBuildingGridPosition(
    Vec3 playerPosition, double yaw, double pitch,
    double minimumDistance, double maximumDistance,
    BuildingType type, double placementPlaneHeight) {
    double distance = maximumDistance;
    const double verticalDirection = std::sin(pitch);
    const double horizontalDirection = std::cos(pitch);
    if (std::abs(verticalDirection) > 1e-3) {
        const double rayDistance =
            (placementPlaneHeight - playerPosition.y) /
            verticalDirection;
        if (rayDistance > 0.0) {
            distance = rayDistance * horizontalDirection;
        }
    }
    distance = std::clamp(distance, minimumDistance, maximumDistance);

    const double aimedX =
        playerPosition.x + std::sin(yaw) * distance;
    const double aimedZ =
        playerPosition.z - std::cos(yaw) * distance;
    if (buildingFootprintHalfExtent(type) == 1.0) {
        return {
            static_cast<int>(std::round(aimedX)),
            static_cast<int>(std::round(aimedZ)),
        };
    }
    return {
        static_cast<int>(std::floor(aimedX)),
        static_cast<int>(std::floor(aimedZ)),
    };
}

BuildingSystem::BuildingSystem(
    std::array<BuildingBalanceDefinition, GameBalance::BuildingTypeCount> definitions,
    EconomyBalanceDefinition economy, int coreBuildRadius)
    : definitions_(definitions), economy_(economy), coreBuildRadius_(coreBuildRadius) {
    blueprintLevels_.fill(1);
    buildingLimitBonus_ = 0;
}

void BuildingSystem::setCoreBuildRadius(int radius) {
    coreBuildRadius_ = std::max(1, radius);
}

int BuildingSystem::coreBuildRadius() const {
    return coreBuildRadius_;
}

void BuildingSystem::setCoreCardBonuses(int healthBaseLevel, double healthPower,
                                       int buildingBaseLevel, double buildingPower) {
    coreHealthBaseLevel_ = std::clamp(healthBaseLevel, 1, static_cast<int>(MaxStructureLevel));
    coreHealthPower_ = std::max(0.0, healthPower);
    coreBuildingBaseLevel_ = std::clamp(buildingBaseLevel, 1, static_cast<int>(MaxStructureLevel));
    coreBuildingPower_ = std::max(0.0, buildingPower);
    const auto iterator = std::ranges::find(buildings_, BuildingType::Core, &BuildingInstance::type);
    if (iterator == buildings_.end()) return;
    const double damageTaken = iterator->maxHealth - iterator->health;
    iterator->maxHealth = buildingMaxHealthAtLevel(
        definition(BuildingType::Core).maxHealth,
        static_cast<std::uint8_t>(coreHealthBaseLevel_)) *
        (1.0 + 0.15 * coreHealthPower_) * maxHealthMultiplier_;
    iterator->health = std::clamp(iterator->maxHealth - damageTaken, 0.0, iterator->maxHealth);
}

std::optional<BuildingInstance> BuildingSystem::upgradeCoreWithCard() {
    const auto iterator = std::ranges::find(buildings_, BuildingType::Core, &BuildingInstance::type);
    if (iterator == buildings_.end() || iterator->level >= MaxStructureLevel) return std::nullopt;
    ++iterator->level;
    return *iterator;
}

bool BuildingSystem::restoreBuildings(
    std::span<const BuildingInstance> buildings,
    std::span<const std::uint8_t> blueprintLevels,
    int coreBuildRadius) {
    constexpr std::uint32_t FirstBuildingId = 1000U;
    constexpr std::uint32_t ReservedIdHeadroom = 4096U;
    std::unordered_set<std::uint64_t> ids;
    ids.reserve(buildings.size());
    std::uint32_t nextIndex = FirstBuildingId;
    for (const BuildingInstance& building : buildings) {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(building.id.generation) << 32U) |
            static_cast<std::uint64_t>(building.id.index);
        if (building.id.index < FirstBuildingId ||
            building.id.index >=
                std::numeric_limits<std::uint32_t>::max() -
                    ReservedIdHeadroom ||
            building.id.generation == 0U ||
            !ids.insert(key).second) {
            return false;
        }
        nextIndex = std::max(nextIndex, building.id.index + 1U);
    }
    if (std::ranges::any_of(
            blueprintLevels,
            [](std::uint8_t level) {
                return level == 0U;
            })) {
        return false;
    }

    buildings_.clear();
    buildings_.reserve(buildings.size());
    std::ranges::copy_if(
        buildings, std::back_inserter(buildings_),
        [](const BuildingInstance& building) {
            return !isRemovedStorageBuilding(building.type);
        });
    nextIndex_ = nextIndex;
    blueprintLevels_.fill(1U);
    const std::size_t count = std::min(
        blueprintLevels_.size(), blueprintLevels.size());
    for (std::size_t index = 0; index < count; ++index) {
        blueprintLevels_[index] = std::max<std::uint8_t>(
            1U, blueprintLevels[index]);
    }
    setCoreBuildRadius(coreBuildRadius);
    return true;
}

const std::array<std::uint8_t, GameBalance::BuildingTypeCount>&
BuildingSystem::blueprintLevels() const {
    return blueprintLevels_;
}

const BuildingBalanceDefinition& BuildingSystem::definition(BuildingType type) const {
    return definitions_[buildingTypeIndex(type)];
}

ResourceCost BuildingSystem::cost(BuildingType type) const {
    const auto& configured = definition(type);
    return {configured.wood, configured.stone, configured.crystals};
}

ResourceCost BuildingSystem::configuredCost(BuildingType type) const {
    const ResourceCost base = cost(type);
    ResourceCost result = base;
    if (type == BuildingType::Core) return result;

    const int existingCount = static_cast<int>(std::count_if(
        buildings_.begin(), buildings_.end(),
        [type](const BuildingInstance& building) {
            return building.type == type;
        }));
    if (existingCount == 0) return result;

    if (buildingLimitCategory(type) == BuildingLimitCategory::Structure || isProducer(type)) {
        const double multiplier = std::pow(isProducer(type) ? 1.15 : 1.3, existingCount);
        const auto scaled = [multiplier](int amount) {
            if (amount <= 0) return 0;
            const double rounded = std::round(amount * multiplier / 5.0) * 5.0;
            const int maximum = std::numeric_limits<int>::max() / 5 * 5;
            return static_cast<int>(std::clamp(rounded, 5.0, static_cast<double>(maximum)));
        };
        return {scaled(base.wood), scaled(base.stone), scaled(base.crystals)};
    }

    // Walls are built in long perimeter lines: accumulate a one-percent
    // surcharge before rounding, rather than charging at least one resource
    // per copy (which made a ten-wood wall grow by ten percent per copy).
    // Gates retain their former small surcharge as perimeter pieces.
    const int baseTotal = addCostComponents(
        addCostComponents(base.wood, base.stone), base.crystals);
    const int incrementPerCopy = std::max(
        1, static_cast<int>(std::lround(baseTotal * 0.04)));
    const int increment = static_cast<int>(std::min<long long>(
        std::numeric_limits<int>::max(),
        type == BuildingType::Wall
            ? static_cast<long long>(baseTotal) * existingCount / 100
            : static_cast<long long>(incrementPerCopy) * existingCount));
    if (base.wood > 0) {
        result.wood = addCostComponents(result.wood, increment);
    } else if (base.stone > 0) {
        result.stone = addCostComponents(result.stone, increment);
    } else if (base.crystals > 0) {
        result.crystals = addCostComponents(result.crystals, increment);
    }
    return result;
}

double BuildingSystem::baseMaxHealth(BuildingType type) const {
    return definition(type).maxHealth;
}

ResourceCost BuildingSystem::repairCost(const BuildingInstance& building) const {
    return repairCostFor(
        building, cost(building.type), economy_);
}

ResourceCost BuildingSystem::sellRefund(const BuildingInstance& building) const {
    return sellRefundFor(cost(building.type), economy_);
}

ResourceCost BuildingSystem::upgradeCost(const BuildingInstance& building) const {
    if (combatCardProgressionEnabled_ &&
        building.type != BuildingType::Core) return {};
    return upgradeCostFor(
        building, cost(building.type), economy_);
}

void BuildingSystem::reset() {
    buildings_.clear();
    blueprintLevels_.fill(1);
    coreHealthBaseLevel_ = 1;
    coreBuildingBaseLevel_ = 1;
    coreHealthPower_ = 0.0;
    coreBuildingPower_ = 0.0;
}

void BuildingSystem::setSandboxMode(bool enabled) {
    sandboxMode_ = enabled;
}

void BuildingSystem::setCombatCardProgressionEnabled(bool enabled) {
    combatCardProgressionEnabled_ = enabled;
}

bool BuildingSystem::setCombatBlueprintLevel(BuildingType type, std::uint8_t level) {
    if ((type == BuildingType::Core || isRemovedStorageBuilding(type)) ||
        level < 1U) return false;
    blueprintLevels_[static_cast<std::size_t>(type)] = level;
    if (type == BuildingType::Wall)
        blueprintLevels_[static_cast<std::size_t>(BuildingType::Gate)] = level;
    bool changed = false;
    for (auto& building : buildings_) {
        const bool sharedDoor = type == BuildingType::Wall &&
            building.type == BuildingType::Gate;
        if (building.type != type && !sharedDoor) continue;
        const double nextMaxHealth = buildingMaxHealthAtLevel(
            definition(type).maxHealth, level) * maxHealthMultiplier_ *
            buildingAnvilMultiplier(building);
        const bool normalizeDoor = sharedDoor &&
            std::abs(building.maxHealth - nextMaxHealth) > 1e-6;
        if (building.level == level && !normalizeDoor) continue;
        const double damageTaken = building.maxHealth - building.health;
        const double healthFraction = building.maxHealth > 0.0
            ? building.health / building.maxHealth : 1.0;
        const bool sameLevel = building.level == level;
        building.level = level;
        building.maxHealth = nextMaxHealth;
        building.health = sameLevel && normalizeDoor
            ? std::clamp(nextMaxHealth * healthFraction, 0.0, nextMaxHealth)
            : std::clamp(nextMaxHealth - damageTaken, 0.0, nextMaxHealth);
        changed = true;
    }
    return changed;
}

void BuildingSystem::setMaxHealthMultiplier(double multiplier) {
    const double next = std::max(1.0, multiplier);
    if (std::abs(next - maxHealthMultiplier_) <= 1e-9) {
        return;
    }
    for (BuildingInstance& building : buildings_) {
        const double previous = building.maxHealth;
        const double towerMultiplier =
            buildingAnvilMultiplier(building);
        building.maxHealth = buildingMaxHealthAtLevel(
            definition(building.type).maxHealth,
            static_cast<std::uint8_t>(building.type == BuildingType::Core ? coreHealthBaseLevel_ : building.level)) *
            (building.type == BuildingType::Core ? 1.0 + 0.15 * coreHealthPower_ : 1.0) *
            next * towerMultiplier;
        building.health += building.maxHealth - previous;
    }
    maxHealthMultiplier_ = next;
}

double BuildingSystem::restoreHealthFraction(double fraction) {
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    double restored = 0.0;
    for (BuildingInstance& building : buildings_) {
        const double previous = building.health;
        building.health = std::min(
            building.maxHealth,
            building.health + building.maxHealth * clamped);
        restored += building.health - previous;
    }
    return restored;
}

void BuildingSystem::setNewTowerBonusEnabled(bool enabled) {
    newTowerBonusEnabled_ = enabled;
    newTowerBonusStacks_ = enabled ? 1U : 0U;
}

PlacementResult BuildingSystem::validate(BuildingType type, GridPosition position, int wood,
                                         int stone, int crystals,
                                         double baseHeight) const {
    const ResourceCost requiredCost = configuredCost(type);
    if (isRemovedStorageBuilding(type)) {
        return {PlacementError::LimitReached, requiredCost};
    }
    if (type == BuildingType::Core && hasCore()) {
        return {PlacementError::CoreAlreadyPlaced, requiredCost};
    }
    if (type != BuildingType::Core && !hasCore()) {
        return {PlacementError::CoreRequired, requiredCost};
    }
    if (!sandboxMode_ && placementCount(type) >= placementLimit(type)) {
        return {PlacementError::LimitReached, requiredCost};
    }
    if (overlaps(type, position, baseHeight)) {
        return {PlacementError::Occupied, requiredCost};
    }
    if (wood < requiredCost.wood || stone < requiredCost.stone ||
        crystals < requiredCost.crystals) {
        return {PlacementError::InsufficientResources, requiredCost};
    }

    if (type != BuildingType::Core && !sandboxMode_) {
        const auto core = std::find_if(buildings_.begin(), buildings_.end(),
                                       [](const BuildingInstance& building) {
                                           return building.type == BuildingType::Core;
                                       });
        const Vec3 center = buildingWorldPosition(type, position);
        const Vec3 coreCenter = buildingWorldPosition(*core);
        const double deltaX = center.x - coreCenter.x;
        const double deltaZ = center.z - coreCenter.z;
        if ((deltaX * deltaX) + (deltaZ * deltaZ) >
            coreBuildRadius_ * coreBuildRadius_) {
            return {PlacementError::OutsideCoreArea, requiredCost};
        }
    }

    return {PlacementError::None, requiredCost};
}

void BuildingSystem::setBuildingLimitBonus(int bonus) {
    buildingLimitBonus_ = std::clamp(bonus, 0, std::numeric_limits<int>::max() - 32);
}

int BuildingSystem::placedBuildingCount() const {
    return static_cast<int>(std::ranges::count_if(buildings_, [](const BuildingInstance& building) {
        return buildingLimitCategory(building.type) == BuildingLimitCategory::Structure;
    }));
}

int BuildingSystem::totalBuildingLimit() const {
    if (sandboxMode_) return std::numeric_limits<int>::max();
    const auto coreBuilding = core();
    const int base = buildingLimitForCoreLevel(BuildingType::Turret,
        static_cast<std::uint8_t>(coreBuilding ? coreBuildingBaseLevel_ : 1));
    const long long total = static_cast<long long>(base) +
        static_cast<long long>(std::floor(2.0 * coreBuildingPower_)) + buildingLimitBonus_;
    return static_cast<int>(std::min<long long>(total, std::numeric_limits<int>::max()));
}

int BuildingSystem::placementCount(BuildingType type) const {
    if (buildingLimitCategory(type) == BuildingLimitCategory::Structure)
        return placedBuildingCount();
    return static_cast<int>(std::ranges::count(buildings_, type, &BuildingInstance::type));
}

int BuildingSystem::placementLimit(BuildingType type) const {
    if (sandboxMode_) return std::numeric_limits<int>::max();
    if (isProducer(type)) return std::numeric_limits<int>::max();
    if (buildingLimitCategory(type) == BuildingLimitCategory::None) return definition(type).maxCount;
    return hasCore() ? std::numeric_limits<int>::max() : 0;
}

std::optional<PlacedBuilding> BuildingSystem::place(BuildingType type, GridPosition position,
                                                    std::uint8_t rotation, int wood, int stone,
                                                    int crystals, double baseHeight,
                                                    int platformStorey,
                                                    double foundationBottomHeight) {
    const PlacementResult validation =
        validate(type, position, wood, stone, crystals,
                 baseHeight);
    if (!validation.valid()) {
        return std::nullopt;
    }

    const bool anvilEnhanced =
        newTowerBonusEnabled_ && isTowerType(type);
    const std::uint8_t anvilStacks = anvilEnhanced
        ? newTowerBonusStacks_ : 0U;
    const std::uint8_t level = combatCardProgressionEnabled_ &&
        type != BuildingType::Core
        ? blueprintLevels_[static_cast<std::size_t>(type)] : 1U;
    const double health = buildingMaxHealthAtLevel(
        definition(type).maxHealth, level) *
        maxHealthMultiplier_ *
        buildingAnvilMultiplier(anvilStacks);
    BuildingInstance building{
        .id = {nextIndex_++, 1},
        .type = type,
        .gridPosition = position,
        .rotation = static_cast<std::uint8_t>(
            rotation % buildingRotationStepCount(type)),
        .level = level,
        .health = health,
        .maxHealth = health,
        .open = false,
        .baseHeight = baseHeight,
        .platformStorey = platformStorey,
            .foundationBottomHeight =
            foundationBottomHeight,
        .anvilEnhanced = anvilEnhanced,
        .anvilStacks = anvilStacks,
    };
    buildings_.push_back(building);
    return PlacedBuilding{building, validation.cost};
}

std::optional<BuildingDamageResult> BuildingSystem::damage(EntityId id, double amount) {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end() || amount <= 0.0) {
        return std::nullopt;
    }

    iterator->health = std::max(0.0, iterator->health - amount);
    const BuildingDamageResult result{
        .id = iterator->id,
        .type = iterator->type,
        .gridPosition = iterator->gridPosition,
        .remainingHealth = iterator->health,
        .destroyed = iterator->health <= 0.0,
        .baseHeight = iterator->baseHeight,
    };
    if (result.destroyed) {
        buildings_.erase(iterator);
    }
    return result;
}

std::optional<BuildingInstance>
BuildingSystem::rotateDirectionalDefense(
    EntityId id, int steps) {
    const auto iterator = std::find_if(
        buildings_.begin(), buildings_.end(),
        [id](const BuildingInstance& building) {
            return building.id == id;
        });
    if (iterator == buildings_.end() ||
        !isDirectionalDefense(iterator->type) || steps == 0) {
        return std::nullopt;
    }

    const int rotation =
        static_cast<int>(iterator->rotation) + steps;
    const int rotationSteps = static_cast<int>(
        buildingRotationStepCount(iterator->type));
    iterator->rotation = static_cast<std::uint8_t>(
        (rotation % rotationSteps + rotationSteps) %
        rotationSteps);
    return *iterator;
}

std::optional<BuildingInstance>
BuildingSystem::remove(EntityId id) {
    const auto iterator = std::find_if(
        buildings_.begin(), buildings_.end(),
        [id](const BuildingInstance& building) {
            return building.id == id;
        });
    if (iterator == buildings_.end()) {
        return std::nullopt;
    }
    const BuildingInstance removed = *iterator;
    buildings_.erase(iterator);
    return removed;
}

std::optional<BuildingInstance> BuildingSystem::toggleGate(EntityId id) {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end() || iterator->type != BuildingType::Gate) {
        return std::nullopt;
    }
    iterator->open = !iterator->open;
    return *iterator;
}

std::optional<BuildingInstance> BuildingSystem::convertWallToGate(EntityId id) {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
        [id](const BuildingInstance& building) { return building.id == id; });
    if (iterator == buildings_.end() || iterator->type != BuildingType::Wall)
        return std::nullopt;
    const std::uint8_t connections = wallConnectionMask(buildings_,
        iterator->gridPosition, iterator->baseHeight);
    const bool northSouth = (connections &
        (WallConnectionNorth | WallConnectionSouth)) != 0;
    const bool eastWest = (connections &
        (WallConnectionEast | WallConnectionWest)) != 0;
    if (northSouth != eastWest)
        iterator->rotation = northSouth ? 1U : 0U;
    iterator->type = BuildingType::Gate;
    iterator->open = false;
    return *iterator;
}

std::optional<EntityId> BuildingSystem::raycast(Vec3 origin, Vec3 direction,
                                                double maxDistance) const {
    std::optional<EntityId> result;
    double closestDistance = maxDistance;
    for (const auto& building : buildings_) {
        const Vec3 center = buildingWorldPosition(building);
        const auto distance = rayBoxDistance(
            origin, direction, center,
            selectionBounds(building.type));
        if (distance && *distance < closestDistance) {
            result = building.id;
            closestDistance = *distance;
        }
    }
    return result;
}

RepairResult BuildingSystem::validateRepair(EntityId id, int wood, int stone, int crystals) const {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end()) {
        return {.error = BuildingActionError::NotFound};
    }
    if (iterator->type == BuildingType::Core) {
        return {.error = BuildingActionError::Unsupported, .building = *iterator};
    }
    if (iterator->health >= iterator->maxHealth) {
        return {.error = BuildingActionError::FullHealth, .building = *iterator};
    }

    const ResourceCost requiredCost = repairCost(*iterator);
    if (wood < requiredCost.wood || stone < requiredCost.stone ||
        crystals < requiredCost.crystals) {
        return {
            .error = BuildingActionError::InsufficientResources,
            .building = *iterator,
            .cost = requiredCost,
        };
    }
    return {
        .error = BuildingActionError::None,
        .building = *iterator,
        .cost = requiredCost,
    };
}

RepairResult BuildingSystem::repair(EntityId id, int wood, int stone, int crystals) {
    const RepairResult validation = validateRepair(id, wood, stone, crystals);
    if (!validation.valid()) {
        return validation;
    }

    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    const double repairedHealth = iterator->maxHealth - iterator->health;
    iterator->health = iterator->maxHealth;
    return {
        .error = BuildingActionError::None,
        .building = *iterator,
        .cost = validation.cost,
        .repairedHealth = repairedHealth,
    };
}

double BuildingSystem::restoreHealth(EntityId id, double amount) {
    if (!std::isfinite(amount) || amount <= 0) return 0;
    const auto target = std::ranges::find(buildings_,id,&BuildingInstance::id);
    if (target == buildings_.end()) return 0;
    const double healing = std::min(amount,std::max(0.0,target->maxHealth-target->health));
    target->health += healing;
    return healing;
}


SellResult BuildingSystem::sell(EntityId id) {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end()) {
        return {.error = BuildingActionError::NotFound};
    }
    if (iterator->type == BuildingType::Core) {
        return {.error = BuildingActionError::Unsupported, .building = *iterator};
    }

    const BuildingInstance sold = *iterator;
    const ResourceCost refund = sellRefund(sold);
    buildings_.erase(iterator);
    return {
        .error = BuildingActionError::None,
        .building = sold,
        .refund = refund,
    };
}

UpgradeResult BuildingSystem::validateUpgrade(EntityId id, int wood, int stone, int crystals) const {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end()) {
        return {.error = UpgradeError::NotFound};
    }
    if (iterator->level >= buildingLevelLimit(iterator->type)) {
        return {.error = UpgradeError::MaxLevel, .building = *iterator};
    }
    if (combatCardProgressionEnabled_) {
        return {.error = UpgradeError::CardRequired, .building = *iterator};
    }

    const ResourceCost requiredCost = upgradeCost(*iterator);
    if (wood < requiredCost.wood || stone < requiredCost.stone ||
        crystals < requiredCost.crystals) {
        return {
            .error = UpgradeError::InsufficientResources,
            .building = *iterator,
            .cost = requiredCost,
        };
    }
    return {
        .error = UpgradeError::None,
        .building = *iterator,
        .cost = requiredCost,
    };
}

UpgradeResult BuildingSystem::upgrade(EntityId id, int wood, int stone, int crystals) {
    const UpgradeResult validation = validateUpgrade(id, wood, stone, crystals);
    if (!validation.valid()) {
        return validation;
    }

    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    const double previousMaxHealth = iterator->maxHealth;
    ++iterator->level;
    if (iterator->type == BuildingType::Core) {
        coreHealthBaseLevel_ = iterator->level;
        coreBuildingBaseLevel_ = iterator->level;
        setCoreBuildRadius(coreBuildRadius() + CoreBuildRadiusPerLevel);
    }
    iterator->maxHealth = buildingMaxHealthAtLevel(
        definition(iterator->type).maxHealth,
        iterator->level) *
        maxHealthMultiplier_ *
        (buildingAnvilMultiplier(*iterator));
    iterator->health += iterator->maxHealth - previousMaxHealth;
    return {
        .error = UpgradeError::None,
        .building = *iterator,
        .cost = validation.cost,
    };
}

bool BuildingSystem::hasCore() const {
    return std::any_of(buildings_.begin(), buildings_.end(), [](const BuildingInstance& building) {
        return building.type == BuildingType::Core;
    });
}

std::optional<BuildingInstance> BuildingSystem::core() const {
    const auto iterator =
        std::find_if(buildings_.begin(), buildings_.end(), [](const BuildingInstance& building) {
            return building.type == BuildingType::Core;
        });
    return iterator == buildings_.end() ? std::nullopt
                                        : std::optional<BuildingInstance>{*iterator};
}

const std::vector<BuildingInstance>& BuildingSystem::buildings() const {
    return buildings_;
}

bool BuildingSystem::overlaps(
    BuildingType type, GridPosition position,
    double baseHeight) const {
    const BuildingFootprint candidate = footprint(type);
    const Vec3 candidateCenter =
        buildingWorldPosition(type, position);
    return std::any_of(buildings_.begin(), buildings_.end(),
                       [candidateCenter, candidate,
                        baseHeight](const BuildingInstance& building) {
                           const Vec3 buildingCenter =
                               buildingWorldPosition(building);
                           const bool sameLevel =
                               std::abs(
                                   baseHeight -
                                   building.baseHeight) <
                               2.75;
                           return sameLevel &&
                                  std::abs(
                                      candidateCenter.x -
                                      buildingCenter.x) <
                                      candidate.halfExtentX +
                                          footprint(building.type)
                                              .halfExtentX &&
                                  std::abs(
                                      candidateCenter.z -
                                      buildingCenter.z) <
                                      candidate.halfExtentZ +
                                          footprint(building.type)
                                              .halfExtentZ;
                       });
}

} // namespace ian
