#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "game/GameBalance.hpp"

#include <span>
#include <vector>

namespace ian {

struct CrystalMineRuntime {
    EntityId buildingId;
    BuildingType buildingType{BuildingType::CrystalMine};
    std::uint8_t level{1};
    double productionProgress{};
    double healthEfficiency{1.0};
    int stored{}; // Legacy save stock; new production goes directly to global resources.
};

struct CrystalProduced {
    EntityId mineId;
    BuildingType buildingType;
    int amount;
};

class CrystalMineSystem {
  public:
    explicit CrystalMineSystem(
        EconomyBalanceDefinition definition = GameBalance::defaults().economy);

    void reset();
    void setProductionSpeedMultiplier(double multiplier);
    void setTypeProductionSpeedMultipliers(double lumberMill, double quarry, double crystalMine);
    void setWoodYieldMultiplier(double multiplier);
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    [[nodiscard]] bool restoreRuntimes(
        const std::vector<CrystalMineRuntime>& saved);
    std::span<const CrystalProduced> tick(double deltaSeconds);
    [[nodiscard]] int collect(EntityId buildingId, int maximumAmount);

    [[nodiscard]] int productionAmount(
        std::uint8_t level,
        BuildingType type = BuildingType::CrystalMine) const;
    [[nodiscard]] double productionInterval(
        BuildingType type = BuildingType::CrystalMine,
        std::uint8_t level = 1,
        double healthEfficiency = 1.0) const;
    [[nodiscard]] const std::vector<CrystalMineRuntime>& mines() const;

  private:
    std::vector<CrystalMineRuntime> mines_;
    std::vector<CrystalProduced> productionBuffer_;
    EconomyBalanceDefinition definition_;
    double productionSpeedMultiplier_{1.0};
    double lumberMillSpeedMultiplier_{1.0};
    double quarrySpeedMultiplier_{1.0};
    double crystalMineSpeedMultiplier_{1.0};
    double woodYieldMultiplier_{1.0};
};

} // namespace ian
