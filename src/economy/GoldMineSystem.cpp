#include "economy/GoldMineSystem.hpp"

#include <algorithm>

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
                                       building.type == BuildingType::GoldMine;
                            });
    });

    for (const auto& building : buildings) {
        if (building.type != BuildingType::GoldMine) {
            continue;
        }
        const bool exists =
            std::any_of(mines_.begin(), mines_.end(), [&building](const GoldMineRuntime& mine) {
                return mine.buildingId == building.id;
            });
        if (!exists) {
            mines_.push_back({.buildingId = building.id, .level = building.level});
        } else {
            const auto runtime =
                std::find_if(mines_.begin(), mines_.end(), [&building](const GoldMineRuntime& mine) {
                    return mine.buildingId == building.id;
                });
            runtime->level = building.level;
        }
    }
}

std::span<const GoldProduced> GoldMineSystem::tick(double deltaSeconds) {
    productionBuffer_.clear();
    for (auto& mine : mines_) {
        mine.productionProgress += deltaSeconds;
        int produced = 0;
        while (mine.productionProgress >= definition_.goldMineInterval) {
            mine.productionProgress -= definition_.goldMineInterval;
            produced += definition_.goldMineAmount * static_cast<int>(mine.level);
        }
        if (produced > 0) {
            productionBuffer_.push_back({mine.buildingId, produced});
        }
    }
    return productionBuffer_;
}

const std::vector<GoldMineRuntime>& GoldMineSystem::mines() const {
    return mines_;
}

} // namespace ian
