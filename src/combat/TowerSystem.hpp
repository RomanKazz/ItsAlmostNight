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
    BuildingType type{BuildingType::Turret};
    std::optional<EntityId> targetId;
    double restYaw{};
    double baseYaw{};
    double yaw{};
    double pitch{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
    std::uint8_t nextMuzzle{};
};

struct TowerShot {
    EntityId towerId;
    EntityId targetId;
    Vec3 origin;
    Vec3 hitPosition;
    BuildingType type{BuildingType::Turret};
    double damage{};
    std::uint8_t muzzleIndex{};
    bool secondaryImpact{};
    bool killed;
};

class TowerSystem {
  public:
    TowerSystem();

    [[nodiscard]] static double attackRange(std::uint8_t level);
    [[nodiscard]] static double attackRange(BuildingType type,
                                            std::uint8_t level);
    [[nodiscard]] static double attackDamage(std::uint8_t level);
    [[nodiscard]] static double attackDamage(BuildingType type,
                                             std::uint8_t level);
    [[nodiscard]] static double fireInterval(std::uint8_t level);
    [[nodiscard]] static double fireInterval(BuildingType type,
                                             std::uint8_t level);
    [[nodiscard]] static int piercingCount(BuildingType type,
                                           std::uint8_t level);

    void reset();
    void setSkillModifiers(double damage, double range,
                           double fireRate, double highGroundDamage);
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const TowerShot> tick(double deltaSeconds,
                                    const std::vector<BuildingInstance>& buildings,
                                    EnemySystem& enemies);

    [[nodiscard]] const std::vector<TowerRuntime>& towers() const;

  private:
    std::vector<TowerRuntime> towers_;
    std::vector<TowerShot> shotBuffer_;
    double damageMultiplier_{1.0};
    double rangeMultiplier_{1.0};
    double fireRateMultiplier_{1.0};
    double highGroundDamageMultiplier_{1.0};
};

} // namespace ian
