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

void CrystalMineSystem::setTypeProductionSpeedMultipliers(
    double lumberMill, double quarry, double crystalMine) {
    lumberMillSpeedMultiplier_ = std::max(0.01, lumberMill);
    quarrySpeedMultiplier_ = std::max(0.01, quarry);
    crystalMineSpeedMultiplier_ = std::max(0.01, crystalMine);
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

bool CrystalMineSystem::restoreRuntimes(
    const std::vector<CrystalMineRuntime>& saved) {
    if (!saved.empty() && saved.size() != mines_.size()) {
        return false;
    }
    for (std::size_t index = 0; index < saved.size(); ++index) {
        const CrystalMineRuntime& source = saved[index];
        if (std::any_of(saved.begin() +
                            static_cast<std::ptrdiff_t>(index + 1),
                        saved.end(),
                        [&source](const CrystalMineRuntime& other) {
                            return other.buildingId == source.buildingId;
                        })) {
            return false;
        }
        const auto target = std::find_if(
            mines_.begin(), mines_.end(),
            [&source](const CrystalMineRuntime& mine) {
                return mine.buildingId == source.buildingId &&
                    mine.buildingType == source.buildingType;
            });
        if (target == mines_.end() ||
            !std::isfinite(source.productionProgress) ||
            source.productionProgress < 0.0 ||
            source.productionProgress > 1000.0 ||
            source.stored < 0) {
            return false;
        }
    }
    for (const CrystalMineRuntime& source : saved) {
        const auto target = std::find_if(
            mines_.begin(), mines_.end(),
            [&source](const CrystalMineRuntime& mine) {
                return mine.buildingId == source.buildingId;
            });
        target->productionProgress = source.productionProgress;
        target->stored = source.stored;
    }
    return true;
}

int CrystalMineSystem::collect(
    EntityId buildingId, int maximumAmount) {
    if (maximumAmount <= 0) return 0;
    const auto runtime = std::find_if(
        mines_.begin(), mines_.end(),
        [buildingId](const CrystalMineRuntime& mine) {
            return mine.buildingId == buildingId;
        });
    if (runtime == mines_.end()) return 0;
    const int collected = std::min(runtime->stored, maximumAmount);
    runtime->stored -= collected;
    return collected;
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
        if (amountPerInterval <= 0) continue;
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
    double amount = static_cast<double>(
        baseAmount + (std::max<int>(1, level) - 1) / 2);
    if (type == BuildingType::LumberMill) {
        amount *= woodYieldMultiplier_;
    }
    return static_cast<int>(std::lround(amount));
}

double CrystalMineSystem::productionInterval(
    BuildingType type, std::uint8_t level,
    double healthEfficiency) const {
    const double levelSpeed = 1.0 +
        0.20 * static_cast<double>(std::max<int>(1, level) - 1);
    const double typeSpeed = type == BuildingType::LumberMill ? lumberMillSpeedMultiplier_
        : type == BuildingType::Quarry ? quarrySpeedMultiplier_ : crystalMineSpeedMultiplier_;
    constexpr double BaseProductionSpeed = 2.0 / 3.0;
    const double effectiveSpeed = BaseProductionSpeed * productionSpeedMultiplier_ * typeSpeed *
        levelSpeed * std::clamp(healthEfficiency, 0.01, 1.0);
    const double baseInterval = type == BuildingType::LumberMill ? 6.0
        : type == BuildingType::Quarry ? 8.0 : definition_.crystalMineInterval;
    // Income grows by a fixed 20% of level-one income at every level.
    // Larger integer batches compensate with a longer interval rather than
    // creating an income spike. Keep fractional seconds; rounding to 0.25s
    // would introduce new jumps in the advertised production rate.
    const double batchScale = static_cast<double>(std::max(1, productionAmount(level, type))) /
        static_cast<double>(std::max(1, productionAmount(1, type)));
    return std::max(0.25, baseInterval * batchScale / effectiveSpeed);
}

const std::vector<CrystalMineRuntime>& CrystalMineSystem::mines() const {
    return mines_;
}

} // namespace ian
