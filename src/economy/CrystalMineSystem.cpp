#include "economy/CrystalMineSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

CrystalMineSystem::CrystalMineSystem(EconomyBalanceDefinition definition)
    : definition_(definition) {
    mines_.reserve(4);
    productionBuffer_.reserve(4);
}

void CrystalMineSystem::reset() {
    mines_.clear();
    productionBuffer_.clear();
}

void CrystalMineSystem::setProductionSpeedMultiplier(
    double multiplier) {
    productionSpeedMultiplier_ = std::max(0.01, multiplier);
}

void CrystalMineSystem::setWoodYieldMultiplier(double multiplier) {
    woodYieldMultiplier_ = std::max(0.0, multiplier);
}

void CrystalMineSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(mines_, [&buildings](const CrystalMineRuntime& mine) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&mine](const BuildingInstance& building) {
                                return building.id == mine.buildingId &&
                                       building.type == mine.buildingType;
                            });
    });

    for (const auto& building : buildings) {
        if (building.type != BuildingType::CrystalMine &&
            building.type != BuildingType::LumberMill &&
            building.type != BuildingType::Quarry) {
            continue;
        }
        const auto runtime = std::find_if(
            mines_.begin(), mines_.end(),
            [&building](const CrystalMineRuntime& mine) {
                return mine.buildingId == building.id;
            });
        if (runtime == mines_.end()) {
            const double healthFraction = building.maxHealth > 0.0
                ? std::clamp(building.health / building.maxHealth, 0.0, 1.0)
                : 0.0;
            mines_.push_back({
                .buildingId = building.id,
                .buildingType = building.type,
                .level = building.level,
                .healthEfficiency = healthFraction >= 0.25
                    ? healthFraction
                    : 0.0,
            });
        } else {
            runtime->level = building.level;
            runtime->buildingType = building.type;
            const double healthFraction = building.maxHealth > 0.0
                ? std::clamp(building.health / building.maxHealth, 0.0, 1.0)
                : 0.0;
            runtime->healthEfficiency = healthFraction >= 0.25
                ? healthFraction
                : 0.0;
        }
    }
}

std::span<const CrystalProduced> CrystalMineSystem::tick(double deltaSeconds) {
    productionBuffer_.clear();
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return productionBuffer_;
    }
    for (auto& mine : mines_) {
        if (mine.healthEfficiency <= 0.0) {
            continue;
        }
        const double interval =
            productionInterval(
                mine.buildingType, mine.level,
                mine.healthEfficiency);
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

int CrystalMineSystem::productionAmount(
    std::uint8_t level, BuildingType type) const {
    int baseAmount = definition_.crystalMineAmount;
    if (type == BuildingType::LumberMill) {
        baseAmount = 3;
    } else if (type == BuildingType::Quarry) {
        baseAmount = 2;
    }
    double amount = static_cast<double>(baseAmount) *
        (1.0 + 0.15 * static_cast<double>(level - 1));
    if (type == BuildingType::LumberMill) {
        amount *= woodYieldMultiplier_;
    }
    return static_cast<int>(std::lround(amount));
}

double CrystalMineSystem::productionInterval(
    BuildingType type, std::uint8_t level,
    double healthEfficiency) const {
    const double levelSpeed = 1.0 +
        0.10 * static_cast<double>(std::max<int>(1, level) - 1);
    const double effectiveSpeed = productionSpeedMultiplier_ *
        levelSpeed * std::clamp(healthEfficiency, 0.01, 1.0);
    if (type == BuildingType::LumberMill) {
        return 8.0 / effectiveSpeed;
    }
    if (type == BuildingType::Quarry) {
        return 10.0 / effectiveSpeed;
    }
    return definition_.crystalMineInterval /
        effectiveSpeed;
}

const std::vector<CrystalMineRuntime>& CrystalMineSystem::mines() const {
    return mines_;
}

} // namespace ian
