#pragma once

#include "core/Types.hpp"
#include "ui/WorldUiOcclusion.hpp"

#include <raylib.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace ian {

struct SimulationSnapshot;
struct EnemyInstance;
struct BuildingInstance;
class TerrainHeightfield;

using EnemyBoundsProvider =
    std::function<std::optional<BoundingBox>(const EnemyInstance&)>;
using BuildingHeightProvider = std::function<double(const BuildingInstance&)>;

class TargetHealthBar {
  public:
    void draw(const SimulationSnapshot& snapshot,
              const Camera3D& camera,
              const TerrainHeightfield& terrain,
              EnemyBoundsProvider enemyBoundsProvider = {},
              std::optional<EntityId> upgradePromptTarget = std::nullopt,
              BuildingHeightProvider buildingHeightProvider = {},
              std::optional<EntityId> buildingLevelTarget = std::nullopt,
              WorldUiOcclusion enemyOcclusion = {});
    void drawScreenOverlay() const;
    void notifyRepair(EntityId id);
    void notifyEnemyHit(EntityId id, double damage = 0.0);
    void notifyResourceHit(EntityId id);
    void reset();

  private:
    WorldUiOcclusion enemyOcclusion_;
    enum class TargetKind {
        Resource,
        Building,
        Foundation,
        Enemy,
    };

    struct Target {
        TargetKind kind;
        EntityId id;

        bool operator==(const Target&) const = default;
    };

    struct TargetHash {
        std::size_t operator()(const Target& target) const noexcept {
            std::uint64_t value =
                (static_cast<std::uint64_t>(target.id.generation) << 32U) |
                static_cast<std::uint64_t>(target.id.index);
            value ^= (static_cast<std::uint64_t>(target.kind) + 1U) *
                0x9e3779b97f4a7c15ULL;
            value ^= value >> 30U;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27U;
            return static_cast<std::size_t>(value);
        }
    };

    void drawBillboard(Target target, Vector3 anchorPosition,
                       double health, double maxHealth,
                       Color fillColor, const Camera3D& camera,
                       int buildingLevel = 0,
                       float opacity = 1.0F,
                       bool showHealthText = false);

    struct Visual {
        Target target;
        Vector3 anchorPosition{};
        double health{};
        double maxHealth{};
        Color fillColor{WHITE};
        int buildingLevel{};
        bool showHealthText{};
    };

    struct HealthVisibility {
        double remaining{};
        std::uint64_t lastSeenFrame{};
        double pendingDamage{};
    };

    struct HealthAnimation {
        double displayedHealth{};
        std::uint64_t lastSeenFrame{};
    };

    struct ScreenHealthText {
        std::string text;
        Vector2 center{};
        float fontSize{};
        float opacity{};
    };

    std::optional<Visual> activeVisual_;
    float opacity_{};
    std::optional<EntityId> repairTarget_;
    double repairPulseRemaining_{};
    double repairPulseDuration_{0.55};
    std::unordered_map<std::uint64_t, HealthVisibility>
        enemyHealthVisibility_;
    std::uint64_t enemyHealthFrame_{};
    std::unordered_map<std::uint64_t, HealthVisibility>
        resourceHealthVisibility_;
    std::uint64_t resourceHealthFrame_{};
    std::unordered_map<Target, HealthAnimation, TargetHash>
        healthAnimations_;
    std::uint64_t healthAnimationFrame_{};
    std::optional<ScreenHealthText> screenHealthText_;
};

} // namespace ian
