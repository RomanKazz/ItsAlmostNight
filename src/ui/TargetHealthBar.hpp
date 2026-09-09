#pragma once

#include "core/Types.hpp"

#include <raylib.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace ian {

struct SimulationSnapshot;
struct EnemyInstance;
class TerrainHeightfield;
class GameUi;
enum class UiResourceIcon;

using EnemyBoundsProvider =
    std::function<std::optional<BoundingBox>(const EnemyInstance&)>;

class TargetHealthBar {
  public:
    void draw(const SimulationSnapshot& snapshot,
              const Camera3D& camera,
              const TerrainHeightfield& terrain,
              const GameUi& ui,
              EnemyBoundsProvider enemyBoundsProvider = {});
    void notifyRepair(EntityId id);
    void notifyEnemyHit(EntityId id);
    void notifyResourceHit(EntityId id);
    void reset();

  private:
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

    void drawBillboard(Target target, Vector3 anchorPosition,
                       double health, double maxHealth,
                       Color fillColor, const Camera3D& camera,
                       const GameUi& ui,
                       std::optional<UiResourceIcon> resourceIcon =
                           std::nullopt,
                       int buildingLevel = 0,
                       float opacity = 1.0F);

    struct Visual {
        Target target;
        Vector3 anchorPosition{};
        double health{};
        double maxHealth{};
        Color fillColor{WHITE};
        std::optional<UiResourceIcon> resourceIcon;
        int buildingLevel{};
    };

    struct HealthVisibility {
        double remaining{};
        std::uint64_t lastSeenFrame{};
    };

    std::optional<Target> target_;
    double displayedHealth_{};
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
};

} // namespace ian
