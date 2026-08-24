#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/ModularBuildingConstants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ian {
namespace {

struct Footprint {
    double halfExtentX;
    double halfExtentZ;
};

struct SelectionBounds {
    double halfExtentX;
    double halfExtentZ;
    double minimumY;
    double maximumY;
};

Footprint footprint(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
    case BuildingType::Turret:
    case BuildingType::GunTurret:
    case BuildingType::Cannon:
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
        return {0.9, 0.9, 0.0, 2.6};
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
           type == BuildingType::Catapult;
}

const auto& defaultBuildingDefinitions() {
    static const auto Definitions = GameBalance::defaults().buildings;
    return Definitions;
}

ResourceCost upgradeCostFor(
    const BuildingInstance& building, ResourceCost base,
    const EconomyBalanceDefinition& economy) {
    if (building.type == BuildingType::Core) {
        if (building.level >= 1 && building.level <= 2) {
            return {
                0, 0,
                economy.coreUpgradeCrystals[building.level - 1],
            };
        }
        if (building.level < MaxBuildingLevel) {
            return {
                0, 0, 50 * static_cast<int>(building.level),
            };
        }
        return {};
    }
    if (building.level >= 1 && building.level <= 2) {
        const auto index =
            static_cast<std::size_t>(building.level - 1);
        const double multiplier =
            economy.buildingUpgradeCostMultiplier[index];
        return {
            static_cast<int>(std::ceil(
                static_cast<double>(base.wood) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.stone) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.crystals) * multiplier)) +
                economy.buildingUpgradeCrystalBonus[index],
        };
    }
    if (building.level < MaxBuildingLevel) {
        const double multiplier =
            1.0 +
            0.25 * static_cast<double>(building.level - 2);
        const int crystalBonus =
            economy.buildingUpgradeCrystalBonus.back() +
            10 * static_cast<int>(building.level - 2);
        return {
            static_cast<int>(std::ceil(
                static_cast<double>(base.wood) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.stone) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.crystals) * multiplier)) +
                crystalBonus,
        };
    }
    return {};
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
        scaled(base.crystals),
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

int scaledCostComponent(int value, double multiplier) {
    if (value <= 0) return 0;
    return static_cast<int>(std::min<double>(
        static_cast<double>(std::numeric_limits<int>::max()),
        std::ceil(static_cast<double>(value) * multiplier)));
}

int addCostComponents(int left, int right) {
    return static_cast<int>(std::min<long long>(
        std::numeric_limits<int>::max(),
        static_cast<long long>(left) + right));
}

} // namespace

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
    case BuildingType::Catapult:
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return BuildingLimitCategory::Defense;
    case BuildingType::CrystalMine:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return BuildingLimitCategory::Producer;
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        return BuildingLimitCategory::Storage;
    case BuildingType::Core:
    case BuildingType::Wall:
    case BuildingType::Gate:
        return BuildingLimitCategory::None;
    }
    return BuildingLimitCategory::None;
}

int buildingLimitForCoreLevel(
    BuildingType type, std::uint8_t coreLevel) {
    constexpr std::array DefenseLimits{
        3, 5, 8, 12, 16, 21, 27, 34};
    constexpr std::array PerTypeLimits{
        1, 2, 3, 4, 5, 6, 7, 8};
    const std::size_t index = static_cast<std::size_t>(
        std::clamp<int>(coreLevel, 1, MaxBuildingLevel) - 1);
    switch (buildingLimitCategory(type)) {
    case BuildingLimitCategory::Defense:
        return DefenseLimits[index];
    case BuildingLimitCategory::Producer:
    case BuildingLimitCategory::Storage:
        return PerTypeLimits[index];
    case BuildingLimitCategory::None:
        return std::numeric_limits<int>::max();
    }
    return std::numeric_limits<int>::max();
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

int buildingStorageCapacityPerLevel(BuildingType type) {
    switch (type) {
    case BuildingType::WoodStorage:
        return 150;
    case BuildingType::StoneStorage:
        return 120;
    case BuildingType::CrystalStorage:
        return 80;
    default:
        return 0;
    }
}

int coreResourceCapacity(
    BuildingType storageType, std::uint8_t coreLevel) {
    // Each level has enough room to pay for the following Core upgrade.
    // The player inventory before placing the Core remains deliberately
    // smaller and is handled by Simulation::resourceCapacity().
    constexpr std::array<int, MaxBuildingLevel> Wood{
        100, 180, 300, 450, 650, 900, 1200, 1550};
    constexpr std::array<int, MaxBuildingLevel> Stone{
        60, 110, 180, 280, 400, 550, 720, 900};
    constexpr std::array<int, MaxBuildingLevel> Crystal{
        60, 120, 180, 240, 320, 380, 440, 520};
    const std::size_t index = static_cast<std::size_t>(
        std::clamp<int>(coreLevel, 1, MaxBuildingLevel) - 1);
    if (storageType == BuildingType::WoodStorage) return Wood[index];
    if (storageType == BuildingType::StoneStorage) return Stone[index];
    if (storageType == BuildingType::CrystalStorage) {
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
        const auto snapAxis =
            [](double coordinate) {
                const int cell =
                    static_cast<int>(
                        std::floor(coordinate));
                return snapPlatformFrameAxis(cell) +
                       PlatformFrameWidthCells / 2;
            };
        return {
            snapAxis(aimedX),
            snapAxis(aimedZ),
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
}

void BuildingSystem::setCoreBuildRadius(int radius) {
    coreBuildRadius_ = std::max(1, radius);
}

int BuildingSystem::coreBuildRadius() const {
    return coreBuildRadius_;
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
                return level == 0U || level > MaxBuildingLevel;
            })) {
        return false;
    }

    buildings_.assign(buildings.begin(), buildings.end());
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
    if (!usesGlobalBlueprint(type)) return base;
    const double multiplier = 1.0 +
        0.12 * static_cast<double>(blueprintLevel(type) - 1);
    return {
        scaledCostComponent(base.wood, multiplier),
        scaledCostComponent(base.stone, multiplier),
        scaledCostComponent(base.crystals, multiplier),
    };
}

ResourceCost BuildingSystem::repairCost(const BuildingInstance& building) const {
    return repairCostFor(
        building, cost(building.type), economy_);
}

ResourceCost BuildingSystem::sellRefund(const BuildingInstance& building) const {
    return sellRefundFor(cost(building.type), economy_);
}

ResourceCost BuildingSystem::upgradeCost(const BuildingInstance& building) const {
    return upgradeCostFor(
        building, cost(building.type), economy_);
}

void BuildingSystem::reset() {
    buildings_.clear();
    blueprintLevels_.fill(1);
}

void BuildingSystem::setMaxHealthMultiplier(double multiplier) {
    const double next = std::max(1.0, multiplier);
    if (std::abs(next - maxHealthMultiplier_) <= 1e-9) {
        return;
    }
    for (BuildingInstance& building : buildings_) {
        const double previous = building.maxHealth;
        const double levelMultiplier =
            1.0 + 0.15 * static_cast<double>(building.level - 1);
        const double towerMultiplier =
            building.anvilStacks > 0
                ? 1.0 + 0.10 * building.anvilStacks
                : building.anvilEnhanced ? 1.10 : 1.0;
        building.maxHealth = definition(building.type).maxHealth *
            levelMultiplier * next * towerMultiplier;
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

void BuildingSystem::setNewTowerBonusStacks(int stacks) {
    newTowerBonusStacks_ = static_cast<std::uint8_t>(std::clamp(
        stacks, 0, 255));
    newTowerBonusEnabled_ = newTowerBonusStacks_ > 0U;
}

PlacementResult BuildingSystem::validate(BuildingType type, GridPosition position, int wood,
                                         int stone, int crystals,
                                         double baseHeight) const {
    const ResourceCost requiredCost = configuredCost(type);
    if (type == BuildingType::Core && hasCore()) {
        return {PlacementError::CoreAlreadyPlaced, requiredCost};
    }
    if (type != BuildingType::Core && !hasCore()) {
        return {PlacementError::CoreRequired, requiredCost};
    }
    if (placementCount(type) >= placementLimit(type)) {
        return {PlacementError::LimitReached, requiredCost};
    }
    if (type != BuildingType::Core) {
        const auto coreBuilding = core();
        if (!coreBuilding ||
            static_cast<int>(coreBuilding->level) < definition(type).unlockCoreLevel) {
            return {PlacementError::CoreLevelRequired, requiredCost};
        }
    }
    if (overlaps(type, position, baseHeight)) {
        return {PlacementError::Occupied, requiredCost};
    }
    if (wood < requiredCost.wood || stone < requiredCost.stone ||
        crystals < requiredCost.crystals) {
        return {PlacementError::InsufficientResources, requiredCost};
    }

    if (type != BuildingType::Core) {
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

int BuildingSystem::placementCount(BuildingType type) const {
    const BuildingLimitCategory category =
        buildingLimitCategory(type);
    return static_cast<int>(std::count_if(
        buildings_.begin(), buildings_.end(),
        [type, category](const BuildingInstance& building) {
            if (category == BuildingLimitCategory::Defense) {
                return buildingLimitCategory(building.type) ==
                    BuildingLimitCategory::Defense;
            }
            return building.type == type;
        }));
}

int BuildingSystem::placementLimit(BuildingType type) const {
    int limit = definition(type).maxCount;
    if (buildingLimitCategory(type) ==
        BuildingLimitCategory::None) {
        return limit;
    }
    const auto coreBuilding = core();
    if (!coreBuilding) {
        return 0;
    }
    return std::min(
        limit,
        buildingLimitForCoreLevel(type, coreBuilding->level));
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
    const std::uint8_t level = blueprintLevel(type);
    const double health = definition(type).maxHealth *
        (1.0 + 0.15 * static_cast<double>(level - 1)) *
        maxHealthMultiplier_ *
        (anvilStacks > 0
             ? 1.0 + 0.10 * anvilStacks
             : 1.0);
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
    if (usesGlobalBlueprint(iterator->type)) {
        return {
            .error = UpgradeError::Unsupported,
            .building = *iterator,
        };
    }
    if (iterator->level >= MaxBuildingLevel) {
        return {.error = UpgradeError::MaxLevel, .building = *iterator};
    }
    if (iterator->type != BuildingType::Core) {
        const auto coreBuilding = core();
        if (!coreBuilding || coreBuilding->level <= iterator->level) {
            return {.error = UpgradeError::CoreLevelRequired, .building = *iterator};
        }
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
    iterator->maxHealth =
        definition(iterator->type).maxHealth *
        (1.0 + 0.15 * static_cast<double>(iterator->level - 1)) *
        maxHealthMultiplier_ *
        (iterator->anvilStacks > 0
             ? 1.0 + 0.10 * iterator->anvilStacks
             : iterator->anvilEnhanced ? 1.10 : 1.0);
    iterator->health += iterator->maxHealth - previousMaxHealth;
    return {
        .error = UpgradeError::None,
        .building = *iterator,
        .cost = validation.cost,
    };
}

bool BuildingSystem::usesGlobalBlueprint(BuildingType type) {
    return type == BuildingType::GunTurret ||
           type == BuildingType::Turret ||
           type == BuildingType::Cannon ||
           type == BuildingType::Catapult;
}

std::uint8_t BuildingSystem::blueprintLevel(BuildingType type) const {
    return usesGlobalBlueprint(type)
        ? blueprintLevels_[buildingTypeIndex(type)]
        : 1;
}

int BuildingSystem::blueprintBuildingCount(BuildingType type) const {
    return static_cast<int>(std::count_if(
        buildings_.begin(), buildings_.end(),
        [type](const BuildingInstance& building) {
            return building.type == type;
        }));
}

BuildingInstance BuildingSystem::blueprintPreview(BuildingType type) const {
    const std::uint8_t level = blueprintLevel(type);
    const double health = definition(type).maxHealth *
        (1.0 + 0.15 * static_cast<double>(level - 1)) *
        maxHealthMultiplier_;
    return {
        .id = {},
        .type = type,
        .gridPosition = {},
        .rotation = 0,
        .level = level,
        .health = health,
        .maxHealth = health,
    };
}

ResourceCost BuildingSystem::blueprintUpgradeCost(BuildingType type) const {
    if (!usesGlobalBlueprint(type)) return {};
    const BuildingInstance preview = blueprintPreview(type);
    if (preview.level >= MaxBuildingLevel) return {};
    const ResourceCost research = upgradeCostFor(
        preview, cost(type), economy_);
    const int count = blueprintBuildingCount(type);
    const auto total = [count](int amount) {
        const int retrofitPerBuilding = scaledCostComponent(amount, 0.5);
        const long long retrofit =
            static_cast<long long>(retrofitPerBuilding) * count;
        return addCostComponents(
            amount,
            static_cast<int>(std::min<long long>(
                retrofit, std::numeric_limits<int>::max())));
    };
    return {
        total(research.wood),
        total(research.stone),
        total(research.crystals),
    };
}

UpgradeResult BuildingSystem::validateBlueprintUpgrade(
    BuildingType type, int wood, int stone, int crystals) const {
    if (!usesGlobalBlueprint(type)) {
        return {.error = UpgradeError::Unsupported};
    }
    const BuildingInstance preview = blueprintPreview(type);
    if (preview.level >= MaxBuildingLevel) {
        return {
            .error = UpgradeError::MaxLevel,
            .building = preview,
        };
    }
    const auto coreBuilding = core();
    if (!coreBuilding || coreBuilding->level <= preview.level) {
        return {
            .error = UpgradeError::CoreLevelRequired,
            .building = preview,
        };
    }
    const ResourceCost required = blueprintUpgradeCost(type);
    if (wood < required.wood || stone < required.stone ||
        crystals < required.crystals) {
        return {
            .error = UpgradeError::InsufficientResources,
            .building = preview,
            .cost = required,
        };
    }
    return {
        .error = UpgradeError::None,
        .building = preview,
        .cost = required,
    };
}

BlueprintUpgradeResult BuildingSystem::upgradeBlueprint(
    BuildingType type, int wood, int stone, int crystals) {
    const UpgradeResult validation = validateBlueprintUpgrade(
        type, wood, stone, crystals);
    const std::uint8_t previous = blueprintLevel(type);
    if (!validation.valid()) {
        return {
            .error = validation.error,
            .type = type,
            .previousLevel = previous,
            .level = previous,
            .cost = validation.cost,
        };
    }

    const std::uint8_t next = static_cast<std::uint8_t>(previous + 1);
    blueprintLevels_[buildingTypeIndex(type)] = next;
    int upgradedCount = 0;
    for (BuildingInstance& building : buildings_) {
        if (building.type != type || building.level >= next) continue;
        const double previousMaxHealth = building.maxHealth;
        building.level = next;
        building.maxHealth =
            definition(type).maxHealth *
            (1.0 + 0.15 * static_cast<double>(next - 1)) *
            maxHealthMultiplier_ *
            (building.anvilStacks > 0
                 ? 1.0 + 0.10 * building.anvilStacks
                 : building.anvilEnhanced ? 1.10 : 1.0);
        building.health += building.maxHealth - previousMaxHealth;
        ++upgradedCount;
    }
    return {
        .error = UpgradeError::None,
        .type = type,
        .previousLevel = previous,
        .level = next,
        .upgradedBuildingCount = upgradedCount,
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
    const Footprint candidate = footprint(type);
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
