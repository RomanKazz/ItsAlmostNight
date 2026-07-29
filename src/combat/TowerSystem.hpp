#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"

#include <optional>
#include <span>
#include <vector>

namespace ian {

struct TowerRuntime {
    EntityId buildingId;
    std::optional<EntityId> targetId;
    double yaw{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
};

struct TowerShot {
    EntityId towerId;
    EntityId targetId;
    Vec3 origin;
    Vec3 hitPosition;
    bool killed;
};

class TowerSystem {
  public:
    TowerSystem();

    [[nodiscard]] static double attackRange(std::uint8_t level);
    [[nodiscard]] static double attackDamage(std::uint8_t level);
    [[nodiscard]] static double fireInterval(std::uint8_t level);

    void reset();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const TowerShot> tick(double deltaSeconds,
                                    const std::vector<BuildingInstance>& buildings,
                                    EnemySystem& enemies);

    [[nodiscard]] const std::vector<TowerRuntime>& towers() const;

  private:
    std::vector<TowerRuntime> towers_;
    std::vector<TowerShot> shotBuffer_;
};

} // namespace ian
