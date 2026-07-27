#pragma once

#include "core/FixedStep.hpp"
#include "game/Simulation.hpp"
#include "graphics/EnvironmentSystem.hpp"
#include "graphics/Renderer.hpp"
#include "ui/GameUi.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ian {

enum class PresentationEffectType {
    Hit,
    ResourceBurst,
    Explosion,
    Debris,
    RamImpact,
};

struct PresentationEffect {
    PresentationEffectType type;
    Vec3 position;
    double remaining;
    double duration;
};

struct DamageIndicator {
    double relativeAngle;
    double remaining;
    double duration;
    bool severe;
};

struct ArrowVisual {
    Vec3 origin;
    Vec3 target;
    double remaining;
    double duration;
};

class App {
  public:
    App();
    int run();

  private:
    void processInput();
    void update();
    void render();
    void addEffect(PresentationEffectType type, Vec3 position, double duration);
    void addCameraShake(double duration, double strength);
    void addDamageIndicator(Vec3 sourcePosition,
                            const SimulationSnapshot& snapshot, bool severe);

    FixedStep fixedStep_;
    Simulation simulation_;
    EnvironmentSystem environment_;
    std::optional<Renderer> renderer_;
    GameUi ui_;
    bool pendingStartFromUi_{};
    PlayerCommand input_;
    double pendingYaw_{};
    double pendingPitch_{};
    bool pendingJump_{};
    bool pendingPickaxe_{};
    bool pendingRifleShot_{};
    std::optional<BuildingType> pendingBuildingSelection_;
    bool pendingBuildingCancel_{};
    std::optional<PlaceBuildingCommand> pendingBuildingPlacement_;
    int pendingBuildingRotation_{};
    double buildingRotationWheelAccumulator_{};
    double buildingRotationCooldownRemaining_{};
    bool pendingStartWave_{};
    bool pendingUnlimitedResources_{};
    std::optional<UpgradeBuildingCommand> pendingBuildingUpgrade_;
    std::optional<RepairBuildingCommand> pendingBuildingRepair_;
    std::optional<SellBuildingCommand> pendingBuildingSale_;
    bool pendingWeaponToggle_{};
    bool pendingWeaponUpgrade_{};
    bool pendingBombThrow_{};
    bool pendingDefeatAllEnemies_{};
    bool pendingToggleInvulnerability_{};
    bool pendingDamageCore_{};
    bool pendingSpawnEnemy_{};
    std::optional<ToggleGateCommand> pendingGateToggle_;
    std::string statusMessage_;
    double statusMessageRemaining_{};
    std::vector<PresentationEffect> effects_;
    std::vector<ArrowVisual> arrowVisuals_;
    double cameraShakeRemaining_{};
    double cameraShakeStrength_{};
    std::vector<DamageIndicator> damageIndicators_;
    double playerDamageFlashRemaining_{};
    EnemyType debugSpawnType_{EnemyType::Basic};
    bool slowMotion_{};
    bool showColliders_{};
    bool showFlowField_{};
    bool showSpatialHash_{};
    bool hideHud_{};
    double simulationTickMilliseconds_{};
    double peakSimulationTickMilliseconds_{};
};

} // namespace ian
