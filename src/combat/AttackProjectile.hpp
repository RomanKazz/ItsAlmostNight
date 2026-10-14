#pragma once
#include "combat/AttackCollision.hpp"
#include "combat/AttackHit.hpp"
#include "combat/ProjectilePool.hpp"

namespace ian {
struct ProjectileConfiguration {
    FinalAttackStats stats;
    double hitRadius{0.06};
    double sourceSurfaceHeight{};
};
struct ProjectileRuntimeState {
    int remainingBounces{};
    int remainingPierce{};
    double elapsedLifetime{};
    double travelledDistance{};
    double previousTravelledDistance{};
    std::vector<EntityId> alreadyHitTargets;
    std::optional<EntityId> currentTarget;
    Vec3 velocity;
    double targetSearchRemaining{};
    std::array<Vec3, 8> trailPositions{};
    std::size_t trailHead{}, trailCount{};
    double trailElapsed{};
    void reset() {
        remainingBounces = remainingPierce = 0;
        elapsedLifetime = 0.0;
        travelledDistance = previousTravelledDistance = 0.0;
        alreadyHitTargets.clear(); currentTarget.reset(); velocity = {}; targetSearchRemaining = 0; trailHead = trailCount = 0; trailElapsed = 0;
    }
};
struct AttackProjectile {
    EntityId id;
    EntityId sourceId;
    BuildingType type{BuildingType::GunTurret};
    ProjectileConfiguration configuration;
    ProjectileRuntimeState runtime;
    Vec3 position;
    Vec3 previousPosition;
    Vec3 launchOrigin;
    bool shrapnel{};
    std::uint8_t muzzleIndex{};
    bool active{};
    void resetForReuse() {
        sourceId = {}; type = BuildingType::GunTurret; configuration = {};
        runtime.reset(); position = previousPosition = launchOrigin = {}; muzzleIndex = 0; active = false; shrapnel = false;
    }
};
struct ProjectileImpact {
    EntityId sourceId;
    BuildingType type;
    AttackHit hit;
    Vec3 position;
    double payloadDamage{};
    std::uint8_t muzzleIndex{};
    bool secondary{};
    FinalAttackStats stats;
    Vec3 launchOrigin;
    bool shrapnel{};
};
struct ProjectileBlast { EntityId sourceId; Vec3 position; double radius; };
[[nodiscard]] inline double missileExplosionRadius(const AttackStats& stats) { return 1.6 * stats.size; }
class AttackProjectileSystem {
  public:
    AttackProjectileSystem() : pool_(100000) {}
    bool spawn(EntityId source, BuildingType type, Vec3 origin, Vec3 direction,
        const FinalAttackStats& stats, double surfaceHeight, std::uint8_t muzzle = 0, bool shrapnel = false, std::optional<EntityId> target = std::nullopt);
    std::span<const ProjectileImpact> tick(double deltaSeconds, EnemySystem& enemies);
    void reset() { pool_.clear(); impacts_.clear(); blasts_.clear(); }
    const std::vector<AttackProjectile>& projectiles() const { return pool_.items(); }
    std::span<const ProjectileBlast> blasts() const { return blasts_; }
    std::size_t dropped() const { return pool_.dropped(); }
  private:
    ProjectilePool<AttackProjectile> pool_;
    AttackCollisionQuery query_;
    std::vector<ProjectileImpact> impacts_;
    std::vector<ProjectileBlast> blasts_;
};
} // namespace ian
