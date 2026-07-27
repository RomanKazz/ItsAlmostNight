#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "game/GameBalance.hpp"

#include <span>
#include <vector>

namespace ian {

struct GoldMineRuntime {
    EntityId buildingId;
    std::uint8_t level{1};
    double productionProgress{};
};

struct GoldProduced {
    EntityId mineId;
    int amount;
};

class GoldMineSystem {
  public:
    explicit GoldMineSystem(
        EconomyBalanceDefinition definition = GameBalance::defaults().economy);

    void reset();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const GoldProduced> tick(double deltaSeconds);

    [[nodiscard]] const std::vector<GoldMineRuntime>& mines() const;

  private:
    std::vector<GoldMineRuntime> mines_;
    std::vector<GoldProduced> productionBuffer_;
    EconomyBalanceDefinition definition_;
};

} // namespace ian
