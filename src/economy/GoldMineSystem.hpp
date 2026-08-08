#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "game/GameBalance.hpp"

#include <span>
#include <vector>

namespace ian {

struct GoldMineRuntime {
    EntityId buildingId;
    BuildingType buildingType{BuildingType::GoldMine};
    std::uint8_t level{1};
    double productionProgress{};
    bool operational{true};
};

struct GoldProduced {
    EntityId mineId;
    BuildingType buildingType;
    int amount;
};

class GoldMineSystem {
  public:
    explicit GoldMineSystem(
        EconomyBalanceDefinition definition = GameBalance::defaults().economy);

    void reset();
    void setProductionSpeedMultiplier(double multiplier);
    void setWoodYieldMultiplier(double multiplier);
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const GoldProduced> tick(double deltaSeconds);

    [[nodiscard]] int productionAmount(
        std::uint8_t level,
        BuildingType type = BuildingType::GoldMine) const;
    [[nodiscard]] double productionInterval(
        BuildingType type = BuildingType::GoldMine) const;
    [[nodiscard]] const std::vector<GoldMineRuntime>& mines() const;

  private:
    std::vector<GoldMineRuntime> mines_;
    std::vector<GoldProduced> productionBuffer_;
    EconomyBalanceDefinition definition_;
    double productionSpeedMultiplier_{1.0};
    double woodYieldMultiplier_{1.0};
};

} // namespace ian
