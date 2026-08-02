#include "economy/GoldMineSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

GoldMineSystem::GoldMineSystem(EconomyBalanceDefinition definition)
    : definition_(definition) {
    mines_.reserve(4);
    productionBuffer_.reserve(4);
}

void GoldMineSystem::reset() {
    mines_.clear();
    productionBuffer_.clear();
}

void GoldMineSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(mines_, [&buildings](const GoldMineRuntime& mine) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&mine](const BuildingInstance& building) {
                                return building.id == mine.buildingId &&
                                       building.type == mine.buildingType;
                            });
    });

    for (const auto& building : buildings) {
        if (building.type != BuildingType::GoldMine &&
            building.type != BuildingType::LumberMill &&
            building.type != BuildingType::Quarry) {
            continue;
        }
        const auto runtime = std::find_if(
            mines_.begin(), mines_.end(),
            [&building](const GoldMineRuntime& mine) {
                return mine.buildingId == building.id;
            });
        if (runtime == mines_.end()) {
            mines_.push_back({
                .buildingId = building.id,
                .buildingType = building.type,
                .level = building.level,
                .operational =
                    building.health >= building.maxHealth,
            });
        } else {
            runtime->level = building.level;
            runtime->buildingType = building.type;
            runtime->operational =
                building.health >= building.maxHealth;
        }
    }
}

std::span<const GoldProduced> GoldMineSystem::tick(double deltaSeconds) {
    productionBuffer_.clear();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return productionBuffer_;
    }
    for (auto& mine : mines_) {
        if (!mine.operational) {
            continue;
        }
        const double interval =
            productionInterval(mine.buildingType);
        const double totalProgress =
            mine.productionProgress + deltaSeconds;
        double completedIntervals = 0.0;
        if (std::isfinite(totalProgress)) {
            completedIntervals =
                std::floor(totalProgress / interval);
            mine.productionProgress =
                std::fmod(totalProgress, interval);
        } else {
            completedIntervals =
                std::numeric_limits<double>::infinity();
            mine.productionProgress = std::fmod(
                std::fmod(mine.productionProgress, interval) +
                    std::fmod(deltaSeconds, interval),
                interval);
        }
        const int amountPerInterval = productionAmount(
            mine.level, mine.buildingType);
        const double maximumIntervals =
            static_cast<double>(
                std::numeric_limits<int>::max() /
                amountPerInterval);
        const int produced =
            completedIntervals > maximumIntervals
                ? std::numeric_limits<int>::max()
                : static_cast<int>(completedIntervals) *
                      amountPerInterval;
        if (produced > 0) {
            productionBuffer_.push_back(
                {mine.buildingId, mine.buildingType, produced});
        }
    }
    return productionBuffer_;
}

int GoldMineSystem::productionAmount(
    std::uint8_t level, BuildingType type) const {
    int baseAmount = definition_.goldMineAmount;
    if (type == BuildingType::LumberMill) {
        baseAmount = 3;
    } else if (type == BuildingType::Quarry) {
        baseAmount = 2;
    }
    return static_cast<int>(std::lround(
        static_cast<double>(baseAmount) *
        (1.0 + 0.15 * static_cast<double>(level - 1))));
}

double GoldMineSystem::productionInterval(
    BuildingType type) const {
    if (type == BuildingType::LumberMill) {
        return 8.0;
    }
    if (type == BuildingType::Quarry) {
        return 10.0;
    }
    return definition_.goldMineInterval;
}

const std::vector<GoldMineRuntime>& GoldMineSystem::mines() const {
    return mines_;
}

} // namespace ian
