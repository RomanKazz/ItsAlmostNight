#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "combat/AttackProjectile.hpp"
#include "combat/LightningTargetCache.hpp"
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
    double yawVelocity{};
    double pitchVelocity{};
    double previousYaw{};
    double previousPitch{};
    bool manualTurnPending{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
    std::uint8_t nextMuzzle{};
    AttackEvent attack;
    int sequenceRemaining{};
    double sequenceDelay{};
    double sequenceInterval{};
    double sequenceYaw{};
    double sequencePitch{};
    double flameRemaining{};
    double flameAge{};
    double flameTailRemaining{};
    Vec3 flameOrigin{};
};

struct TowerShot {
    EntityId towerId;
    EntityId targetId;
    EnemyType targetType{EnemyType::Basic};
    std::uint8_t targetEliteAffixes{};
    Vec3 origin;
    Vec3 hitPosition;
    BuildingType type{BuildingType::Turret};
    double damage{};
    double payloadDamage{};
    std::uint8_t muzzleIndex{};
    bool secondaryImpact{};
    bool critical{};
    bool killed;
    FinalAttackStats stats;
    bool shrapnel{};
    bool periodicBurn{};
};

struct TowerLaunch {
    EntityId sourceId;
    BuildingType type;
    Vec3 origin;
    std::uint8_t muzzleIndex{};
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

    [[nodiscard]] static AttackStats baseStats(BuildingType type, std::uint8_t level);
    [[nodiscard]] FinalAttackStats finalStats(const BuildingInstance& building) const;
    void setPreferMarkedTargets(bool enabled) { preferMarkedTargets_ = enabled; }
    void setAttackModifiers(const AttackModifiers& modifiers) { modifiers_ = modifiers; }
    [[nodiscard]] const std::vector<AttackProjectile>& projectiles() const { return projectiles_.projectiles(); }
    [[nodiscard]] std::span<const AttackEvent> attacks() const { return attackBuffer_; }
    [[nodiscard]] std::size_t droppedProjectiles() const { return projectiles_.dropped(); }
    [[nodiscard]] std::size_t activeFlameBurns() const { return flameBurns_.size(); }
    void setShrapnelCount(int count) { shrapnelCount_ = std::max(0, count); }
    [[nodiscard]] std::span<const TowerLaunch> launches() const { return launches_; }
    [[nodiscard]] std::span<const ProjectileBlast> blasts() const { return projectiles_.blasts(); }
    void clearProjectiles();
    void reset();
    void setTypeBonuses(BuildingType type, double damage, double fireRate);
    void setModifiers(double damage, double range,
                           double fireRate, double highGroundDamage,
                           int quantityBonus = 0,
                           int pierceCount = 0);
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const TowerShot> tick(double deltaSeconds,
                                    const std::vector<BuildingInstance>& buildings,
                                    EnemySystem& enemies);

    [[nodiscard]] const std::vector<TowerRuntime>& towers() const;

  private:
    std::vector<TowerRuntime> towers_;
    std::vector<TowerShot> shotBuffer_;
    AttackModifiers modifiers_;
    bool preferMarkedTargets_{};
    AttackModifiers typeModifiers_;
    BuildingType bonusType_{BuildingType::Core};
    double highGroundDamageMultiplier_{1.0};
    AttackProjectileSystem projectiles_;
    std::vector<AttackEvent> attackBuffer_;
    std::uint64_t shotSerial_{};
    int shrapnelCount_{};
    AttackCollisionQuery shrapnelQuery_;
    std::vector<TowerLaunch> launches_;
    struct FlameBurn {
        EntityId targetId;
        EntityId sourceId;
        FinalAttackStats stats;
        double remaining{};
        double tickRemaining{0.5};
    };
    std::vector<FlameBurn> flameBurns_;
    std::array<int, EnemySystem::MaxEnemies> flameBurnSlots_{};
    AttackCollisionQuery flameQuery_;
    AttackCollisionQuery teslaQuery_;
    LightningTargetCache teslaTargetCache_;
    std::vector<EntityId> teslaVisited_;
    void updateFlameBurns(double deltaSeconds, EnemySystem& enemies);
    void applyFlameBurn(EntityId target, EntityId source, const FinalAttackStats& stats,
        const EnemySystem& enemies);

};

} // namespace ian
