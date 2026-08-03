#pragma once

#include "core/Types.hpp"

#include <raylib.h>

#include <optional>

namespace ian {

struct SimulationSnapshot;

class TargetHealthBar {
  public:
    void draw(const SimulationSnapshot& snapshot,
              const Camera3D& camera);
    void notifyRepair(EntityId id);
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
                       int buildingLevel = 0);

    std::optional<Target> target_;
    double displayedHealth_{};
    std::optional<EntityId> repairTarget_;
    double repairPulseRemaining_{};
    double repairPulseDuration_{0.55};
};

} // namespace ian
