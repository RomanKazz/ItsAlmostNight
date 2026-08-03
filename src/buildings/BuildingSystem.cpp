#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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
    case BuildingType::GoldMine:
    case BuildingType::Cannon:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return {1.0, 1.0};
    case BuildingType::Wall:
    case BuildingType::SlowTrap:
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
    case BuildingType::GoldMine:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return {0.9, 0.9, 0.0, 1.6};
    case BuildingType::Cannon:
        return {0.82, 0.82, 0.0, 2.05};
    case BuildingType::SlowTrap:
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
                economy.coreUpgradeGold[building.level - 1],
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
                static_cast<double>(base.gold) * multiplier)) +
                economy.buildingUpgradeGoldBonus[index],
        };
    }
    if (building.level < MaxBuildingLevel) {
        const double multiplier =
            1.0 +
            0.25 * static_cast<double>(building.level - 2);
        const int goldBonus =
            economy.buildingUpgradeGoldBonus.back() +
            10 * static_cast<int>(building.level - 2);
        return {
            static_cast<int>(std::ceil(
                static_cast<double>(base.wood) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.stone) * multiplier)),
            static_cast<int>(std::ceil(
                static_cast<double>(base.gold) * multiplier)) +
                goldBonus,
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
        scaled(base.gold),
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
        scaled(base.gold),
    };
}

} // namespace

ResourceCost buildingCost(BuildingType type) {
    const auto& definition = defaultBuildingDefinitions()[buildingTypeIndex(type)];
    return {definition.wood, definition.stone, definition.gold};
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
    return type != BuildingType::SlowTrap;
}

bool buildingBlocksMovement(const BuildingInstance& building) {
    return buildingBlocksMovement(building.type) &&
           !(building.type == BuildingType::Gate && building.open);
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
    : definitions_(definitions), economy_(economy), coreBuildRadius_(coreBuildRadius) {}

const BuildingBalanceDefinition& BuildingSystem::definition(BuildingType type) const {
    return definitions_[buildingTypeIndex(type)];
}

ResourceCost BuildingSystem::cost(BuildingType type) const {
    const auto& configured = definition(type);
    return {configured.wood, configured.stone, configured.gold};
}

ResourceCost BuildingSystem::configuredCost(BuildingType type) const {
    return cost(type);
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
}

PlacementResult BuildingSystem::validate(BuildingType type, GridPosition position, int wood,
                                         int stone, int gold,
                                         double baseHeight) const {
    const ResourceCost requiredCost = cost(type);
    if (type == BuildingType::Core && hasCore()) {
        return {PlacementError::CoreAlreadyPlaced, requiredCost};
    }
    const auto typeCount =
        std::count_if(buildings_.begin(), buildings_.end(), [type](const BuildingInstance& building) {
            return building.type == type;
        });
    if (typeCount >= definition(type).maxCount) {
        return {PlacementError::LimitReached, requiredCost};
    }
    if (type != BuildingType::Core && !hasCore()) {
        return {PlacementError::CoreRequired, requiredCost};
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
        gold < requiredCost.gold) {
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

std::optional<PlacedBuilding> BuildingSystem::place(BuildingType type, GridPosition position,
                                                    std::uint8_t rotation, int wood, int stone,
                                                    int gold, double baseHeight,
                                                    int platformStorey,
                                                    double foundationBottomHeight) {
    const PlacementResult validation =
        validate(type, position, wood, stone, gold,
                 baseHeight);
    if (!validation.valid()) {
        return std::nullopt;
    }

    const double health = definition(type).maxHealth;
    BuildingInstance building{
        .id = {nextIndex_++, 1},
        .type = type,
        .gridPosition = position,
        .rotation = static_cast<std::uint8_t>(rotation % 4),
        .level = 1,
        .health = health,
        .maxHealth = health,
        .open = false,
        .baseHeight = baseHeight,
        .platformStorey = platformStorey,
        .foundationBottomHeight =
            foundationBottomHeight,
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

RepairResult BuildingSystem::validateRepair(EntityId id, int wood, int stone, int gold) const {
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
        gold < requiredCost.gold) {
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

RepairResult BuildingSystem::repair(EntityId id, int wood, int stone, int gold) {
    const RepairResult validation = validateRepair(id, wood, stone, gold);
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

UpgradeResult BuildingSystem::validateUpgrade(EntityId id, int wood, int stone, int gold) const {
    const auto iterator = std::find_if(buildings_.begin(), buildings_.end(),
                                       [id](const BuildingInstance& building) {
                                           return building.id == id;
                                       });
    if (iterator == buildings_.end()) {
        return {.error = UpgradeError::NotFound};
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
        gold < requiredCost.gold) {
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

UpgradeResult BuildingSystem::upgrade(EntityId id, int wood, int stone, int gold) {
    const UpgradeResult validation = validateUpgrade(id, wood, stone, gold);
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
        (1.0 + 0.15 * static_cast<double>(iterator->level - 1));
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
