#include "buildings/BuildingSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

struct Footprint {
    double halfExtentX;
    double halfExtentZ;
};

Footprint footprint(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return {1.0, 1.0};
    case BuildingType::Wall:
        return {0.5, 0.5};
    case BuildingType::Turret:
        return {0.5, 0.5};
    case BuildingType::GoldMine:
        return {0.5, 0.5};
    case BuildingType::Cannon:
        return {0.5, 0.5};
    case BuildingType::SlowTrap:
        return {0.5, 0.5};
    case BuildingType::Gate:
        return {0.5, 0.5};
    }
    return {};
}

std::size_t buildingTypeIndex(BuildingType type) {
    return static_cast<std::size_t>(type);
}

const auto& defaultBuildingDefinitions() {
    static const auto Definitions = GameBalance::defaults().buildings;
    return Definitions;
}

bool rectanglesOverlap(GridPosition leftPosition, Footprint left, GridPosition rightPosition,
                       Footprint right) {
    return static_cast<double>(std::abs(leftPosition.x - rightPosition.x)) <
               left.halfExtentX + right.halfExtentX &&
           static_cast<double>(std::abs(leftPosition.z - rightPosition.z)) <
               left.halfExtentZ + right.halfExtentZ;
}

} // namespace

ResourceCost buildingCost(BuildingType type) {
    const auto& definition = defaultBuildingDefinitions()[buildingTypeIndex(type)];
    return {definition.wood, definition.stone, definition.gold};
}

ResourceCost buildingUpgradeCost(const BuildingInstance& building) {
    const auto economy = GameBalance::defaults().economy;
    if (building.type == BuildingType::Core) {
        if (building.level >= 1 && building.level <= 2) {
            return {0, 0, economy.coreUpgradeGold[building.level - 1]};
        }
        return {};
    }

    const ResourceCost base = buildingCost(building.type);
    if (building.level >= 1 && building.level <= 2) {
        const auto index = static_cast<std::size_t>(building.level - 1);
        const double multiplier = economy.buildingUpgradeCostMultiplier[index];
        return {
            static_cast<int>(std::ceil(static_cast<double>(base.wood) * multiplier)),
            static_cast<int>(std::ceil(static_cast<double>(base.stone) * multiplier)),
            static_cast<int>(std::ceil(static_cast<double>(base.gold) * multiplier)) +
                economy.buildingUpgradeGoldBonus[index],
        };
    }
    return {};
}

ResourceCost buildingRepairCost(const BuildingInstance& building) {
    if (building.maxHealth <= 0.0 || building.health >= building.maxHealth) {
        return {};
    }
    const double missingFraction = (building.maxHealth - building.health) / building.maxHealth;
    const ResourceCost baseCost = buildingCost(building.type);
    return {
        static_cast<int>(std::ceil(static_cast<double>(baseCost.wood) * missingFraction *
                                   GameBalance::defaults().economy.repairCostFraction)),
        static_cast<int>(std::ceil(static_cast<double>(baseCost.stone) * missingFraction *
                                   GameBalance::defaults().economy.repairCostFraction)),
        static_cast<int>(std::ceil(static_cast<double>(baseCost.gold) * missingFraction *
                                   GameBalance::defaults().economy.repairCostFraction)),
    };
}

ResourceCost buildingSellRefund(const BuildingInstance& building) {
    const ResourceCost cost = buildingCost(building.type);
    const double fraction = GameBalance::defaults().economy.sellRefundFraction;
    return {
        static_cast<int>(static_cast<double>(cost.wood) * fraction),
        static_cast<int>(static_cast<double>(cost.stone) * fraction),
        static_cast<int>(static_cast<double>(cost.gold) * fraction),
    };
}

bool buildingBlocksMovement(BuildingType type) {
    return type != BuildingType::SlowTrap;
}

bool buildingBlocksMovement(const BuildingInstance& building) {
    return buildingBlocksMovement(building.type) &&
           !(building.type == BuildingType::Gate && building.open);
}

std::uint8_t wallConnectionMask(std::span<const BuildingInstance> buildings,
                                GridPosition position) {
    std::uint8_t mask = 0;
    for (const auto& building : buildings) {
        if (building.type != BuildingType::Wall && building.type != BuildingType::Gate) {
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

GridPosition aimedBuildingGridPosition(Vec3 playerPosition, double yaw, double pitch,
                                       double minimumDistance, double maximumDistance) {
    double distance = maximumDistance;
    if (pitch < -1e-3) {
        distance = playerPosition.y / std::tan(-pitch);
    }
    distance = std::clamp(distance, minimumDistance, maximumDistance);

    return {
        static_cast<int>(std::lround(playerPosition.x + std::sin(yaw) * distance)),
        static_cast<int>(std::lround(playerPosition.z - std::cos(yaw) * distance)),
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
    if (building.maxHealth <= 0.0 || building.health >= building.maxHealth) {
        return {};
    }
    const double missingFraction = (building.maxHealth - building.health) / building.maxHealth;
    const ResourceCost baseCost = cost(building.type);
    return {
        static_cast<int>(std::ceil(static_cast<double>(baseCost.wood) * missingFraction *
                                   economy_.repairCostFraction)),
        static_cast<int>(std::ceil(static_cast<double>(baseCost.stone) * missingFraction *
                                   economy_.repairCostFraction)),
        static_cast<int>(std::ceil(static_cast<double>(baseCost.gold) * missingFraction *
                                   economy_.repairCostFraction)),
    };
}

ResourceCost BuildingSystem::sellRefund(const BuildingInstance& building) const {
    const ResourceCost baseCost = cost(building.type);
    return {
        static_cast<int>(static_cast<double>(baseCost.wood) * economy_.sellRefundFraction),
        static_cast<int>(static_cast<double>(baseCost.stone) * economy_.sellRefundFraction),
        static_cast<int>(static_cast<double>(baseCost.gold) * economy_.sellRefundFraction),
    };
}

ResourceCost BuildingSystem::upgradeCost(const BuildingInstance& building) const {
    if (building.type == BuildingType::Core) {
        if (building.level >= 1 && building.level <= 2) {
            return {0, 0, economy_.coreUpgradeGold[building.level - 1]};
        }
        return {};
    }
    const ResourceCost base = cost(building.type);
    if (building.level >= 1 && building.level <= 2) {
        const auto index = static_cast<std::size_t>(building.level - 1);
        const double multiplier = economy_.buildingUpgradeCostMultiplier[index];
        return {
            static_cast<int>(std::ceil(static_cast<double>(base.wood) * multiplier)),
            static_cast<int>(std::ceil(static_cast<double>(base.stone) * multiplier)),
            static_cast<int>(std::ceil(static_cast<double>(base.gold) * multiplier)) +
                economy_.buildingUpgradeGoldBonus[index],
        };
    }
    return {};
}

void BuildingSystem::reset() {
    buildings_.clear();
    nextIndex_ = 1000;
}

PlacementResult BuildingSystem::validate(BuildingType type, GridPosition position, int wood,
                                         int stone, int gold) const {
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
    if (wood < requiredCost.wood || stone < requiredCost.stone ||
        gold < requiredCost.gold) {
        return {PlacementError::InsufficientResources, requiredCost};
    }
    if (overlaps(type, position)) {
        return {PlacementError::Occupied, requiredCost};
    }

    if (type != BuildingType::Core) {
        const auto core = std::find_if(buildings_.begin(), buildings_.end(),
                                       [](const BuildingInstance& building) {
                                           return building.type == BuildingType::Core;
                                       });
        const int deltaX = position.x - core->gridPosition.x;
        const int deltaZ = position.z - core->gridPosition.z;
        if ((deltaX * deltaX) + (deltaZ * deltaZ) >
            coreBuildRadius_ * coreBuildRadius_) {
            return {PlacementError::OutsideCoreArea, requiredCost};
        }
    }

    return {PlacementError::None, requiredCost};
}

std::optional<PlacedBuilding> BuildingSystem::place(BuildingType type, GridPosition position,
                                                    std::uint8_t rotation, int wood, int stone,
                                                    int gold) {
    const PlacementResult validation = validate(type, position, wood, stone, gold);
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
    };
    if (result.destroyed) {
        buildings_.erase(iterator);
    }
    return result;
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
        const double centerY = building.type == BuildingType::Core ? 1.2 : 1.0;
        const double radius = building.type == BuildingType::Core ? 1.6 : 0.8;
        const double offsetX = origin.x - static_cast<double>(building.gridPosition.x);
        const double offsetY = origin.y - centerY;
        const double offsetZ = origin.z - static_cast<double>(building.gridPosition.z);
        const double halfB =
            (offsetX * direction.x) + (offsetY * direction.y) + (offsetZ * direction.z);
        const double c = (offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ) -
                         (radius * radius);
        const double discriminant = (halfB * halfB) - c;
        if (discriminant < 0.0) {
            continue;
        }
        const double distance = -halfB - std::sqrt(discriminant);
        if (distance >= 0.0 && distance <= closestDistance) {
            result = building.id;
            closestDistance = distance;
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
    if (iterator->level >= 3) {
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
        (1.0 + 0.5 * static_cast<double>(iterator->level - 1));
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

bool BuildingSystem::overlaps(BuildingType type, GridPosition position) const {
    const Footprint candidate = footprint(type);
    return std::any_of(buildings_.begin(), buildings_.end(),
                       [position, candidate](const BuildingInstance& building) {
                           return rectanglesOverlap(position, candidate, building.gridPosition,
                                                    footprint(building.type));
                       });
}

} // namespace ian
