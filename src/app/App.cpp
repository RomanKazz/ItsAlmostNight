#include "app/App.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace ian {
namespace {

constexpr int ScreenWidth = 1280;
constexpr int ScreenHeight = 720;
constexpr double MouseSensitivity = 0.002;

void drawCentered(const char* text, int y, int fontSize, Color color) {
    const int width = MeasureText(text, fontSize);
    DrawText(text, (GetScreenWidth() - width) / 2, y, fontSize, color);
}

const char* buildingName(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return "Core";
    case BuildingType::Wall:
        return "Wall";
    case BuildingType::Turret:
        return "Turret";
    case BuildingType::GoldMine:
        return "Gold Mine";
    case BuildingType::Cannon:
        return "Cannon";
    case BuildingType::SlowTrap:
        return "Slow Trap";
    case BuildingType::Gate:
        return "Gate";
    }
    return "";
}

const char* placementMessage(PlacementError error) {
    switch (error) {
    case PlacementError::None:
        return "LMB: place   RMB: cancel   Wheel: rotate";
    case PlacementError::CoreAlreadyPlaced:
        return "Core already placed";
    case PlacementError::CoreRequired:
        return "Place Core first";
    case PlacementError::InsufficientResources:
        return "Not enough resources";
    case PlacementError::Occupied:
        return "Space occupied";
    case PlacementError::OutsideCoreArea:
        return "Outside Core area";
    case PlacementError::PlayerOverlap:
        return "Player blocks placement";
    case PlacementError::WorldCollision:
        return "Terrain blocks placement";
    case PlacementError::LimitReached:
        return "Building limit reached";
    case PlacementError::OutOfRange:
        return "Placement is too far";
    case PlacementError::CoreLevelRequired:
        return "Core level II required";
    }
    return "";
}

const char* upgradeErrorMessage(UpgradeError error) {
    switch (error) {
    case UpgradeError::None:
        return "";
    case UpgradeError::NotFound:
        return "Building no longer exists";
    case UpgradeError::MaxLevel:
        return "Building already level III";
    case UpgradeError::Unsupported:
        return "Building cannot be upgraded";
    case UpgradeError::CoreLevelRequired:
        return "Upgrade Core first";
    case UpgradeError::InsufficientResources:
        return "Not enough resources for upgrade";
    }
    return "";
}

const char* buildingActionErrorMessage(BuildingActionError error) {
    switch (error) {
    case BuildingActionError::None:
        return "";
    case BuildingActionError::NotFound:
        return "Building no longer exists";
    case BuildingActionError::FullHealth:
        return "Building already fully repaired";
    case BuildingActionError::Unsupported:
        return "Core cannot be sold";
    case BuildingActionError::InsufficientResources:
        return "Not enough resources for repair";
    }
    return "";
}

const char* weaponUpgradeErrorMessage(WeaponUpgradeError error) {
    switch (error) {
    case WeaponUpgradeError::None:
        return "";
    case WeaponUpgradeError::MaxLevel:
        return "Rifle already level III";
    case WeaponUpgradeError::CoreLevelRequired:
        return "Upgrade Core before Rifle";
    case WeaponUpgradeError::InsufficientGold:
        return "Not enough gold for Rifle upgrade";
    }
    return "";
}

const char* attackDirectionName(AttackDirection direction) {
    switch (direction) {
    case AttackDirection::North:
        return "NORTH";
    case AttackDirection::East:
        return "EAST";
    case AttackDirection::South:
        return "SOUTH";
    case AttackDirection::West:
        return "WEST";
    }
    return "";
}

const char* enemyName(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return "Basic";
    case EnemyType::Fast:
        return "Fast";
    case EnemyType::Heavy:
        return "Heavy";
    case EnemyType::Boss:
        return "Boss";
    }
    return "";
}

std::string tutorialText(const SimulationSnapshot& snapshot) {
    if (!snapshot.tutorialObjective) {
        return {};
    }
    switch (*snapshot.tutorialObjective) {
    case TutorialObjective::MineWood:
        return "OBJECTIVE: Mine trees - Wood " + std::to_string(snapshot.wood) + "/" +
               std::to_string(snapshot.tutorialWoodTarget);
    case TutorialObjective::PlaceCore:
        return "OBJECTIVE: Place Core [1]";
    case TutorialObjective::MineStone:
        return "OBJECTIVE: Mine rocks - Stone " + std::to_string(snapshot.stone) + "/" +
               std::to_string(snapshot.tutorialStoneTarget);
    case TutorialObjective::BuildGoldMine:
        return "OBJECTIVE: Build Gold Mine [4]";
    case TutorialObjective::PrepareForNight:
        return "OBJECTIVE: Build defenses - [N] starts sunset";
    case TutorialObjective::SurviveFirstWave:
        return "OBJECTIVE: Survive first night";
    }
    return {};
}

bool acceptsGameplayInput(RunState state) {
    return state == RunState::Gathering || state == RunState::BuildPhase ||
           state == RunState::Sunset || state == RunState::Wave ||
           state == RunState::WaveComplete;
}

Vector3 colorToVector(Color color) {
    constexpr float ChannelScale = 1.0F / 255.0F;
    return {
        static_cast<float>(color.r) * ChannelScale,
        static_cast<float>(color.g) * ChannelScale,
        static_cast<float>(color.b) * ChannelScale,
    };
}

float smoothstep(float edge0, float edge1, float value) {
    const float normalized =
        std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return normalized * normalized * (3.0F - 2.0F * normalized);
}

void drawBuildGrid(Vector3 playerPosition, double worldLimit) {
    constexpr float FadeStart = 15.0F;
    constexpr float FadeEnd = 25.0F;
    constexpr float GridHeight = 0.025F;
    constexpr float MinorOpacity = 0.15F;
    constexpr float MajorOpacity = 0.28F;
    constexpr int MajorInterval = 5;

    const int worldMinimum =
        static_cast<int>(std::ceil(-worldLimit));
    const int worldMaximum =
        static_cast<int>(std::floor(worldLimit));
    const int minimumX = std::max(
        worldMinimum,
        static_cast<int>(std::floor(playerPosition.x - FadeEnd)));
    const int maximumX = std::min(
        worldMaximum,
        static_cast<int>(std::ceil(playerPosition.x + FadeEnd)));
    const int minimumZ = std::max(
        worldMinimum,
        static_cast<int>(std::floor(playerPosition.z - FadeEnd)));
    const int maximumZ = std::min(
        worldMaximum,
        static_cast<int>(std::ceil(playerPosition.z + FadeEnd)));

    const auto lineColor = [playerPosition](float x, float z,
                                            bool major) {
        const float offsetX = x - playerPosition.x;
        const float offsetZ = z - playerPosition.z;
        const float distance = std::sqrt(offsetX * offsetX +
                                         offsetZ * offsetZ);
        const float fade =
            1.0F - smoothstep(FadeStart, FadeEnd, distance);
        const float opacity =
            fade * (major ? MajorOpacity : MinorOpacity);
        return Color{
            216,
            225,
            218,
            static_cast<unsigned char>(
                std::lround(std::clamp(opacity, 0.0F, 1.0F) *
                            255.0F)),
        };
    };

    for (int x = minimumX; x <= maximumX; ++x) {
        const bool major = x % MajorInterval == 0;
        for (int z = minimumZ; z < maximumZ; ++z) {
            const Color color =
                lineColor(static_cast<float>(x),
                          static_cast<float>(z) + 0.5F, major);
            if (color.a == 0U) {
                continue;
            }
            DrawLine3D({static_cast<float>(x), GridHeight,
                        static_cast<float>(z)},
                       {static_cast<float>(x), GridHeight,
                        static_cast<float>(z + 1)},
                       color);
        }
    }
    for (int z = minimumZ; z <= maximumZ; ++z) {
        const bool major = z % MajorInterval == 0;
        for (int x = minimumX; x < maximumX; ++x) {
            const Color color =
                lineColor(static_cast<float>(x) + 0.5F,
                          static_cast<float>(z), major);
            if (color.a == 0U) {
                continue;
            }
            DrawLine3D({static_cast<float>(x), GridHeight,
                        static_cast<float>(z)},
                       {static_cast<float>(x + 1), GridHeight,
                        static_cast<float>(z)},
                       color);
        }
    }
}

GameBalance loadAppBalance() {
    return loadGameBalance("assets/data/enemies.json", "assets/data/waves.json",
                           "assets/data/buildings.json", "assets/data/weapons.json",
                           "assets/data/economy.json", "assets/data/gameplay.json")
        .balance;
}

MapDefinition loadAppMap() {
    return loadMapDefinition("assets/maps/graybox.json").map;
}

std::array<EnvironmentProfile, 4> loadAppEnvironment() {
    return loadEnvironmentProfiles("assets/data/environment.json").profiles;
}

} // namespace

App::App()
    : simulation_(loadAppBalance(), loadAppMap()),
      environment_(loadAppEnvironment()) {
    effects_.reserve(128);
    arrowVisuals_.reserve(64);
    damageIndicators_.reserve(12);
}

int App::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT |
                   FLAG_MSAA_4X_HINT);
    InitWindow(ScreenWidth, ScreenHeight, "It's Almost Night");
    SetTargetFPS(144);
    renderer_.emplace();
    renderer_->initialize();
    ui_.initialize();

    while (!WindowShouldClose()) {
        processInput();
        update();
        render();
    }

    ui_.shutdown();
    renderer_->shutdown();
    renderer_.reset();
    CloseWindow();
    return 0;
}

void App::processInput() {
    renderer_->processInput();
    const auto snapshot = simulation_.snapshot();
    if (snapshot.state == RunState::MainMenu &&
        (IsKeyPressed(KEY_ENTER) || pendingStartFromUi_)) {
        pendingStartFromUi_ = false;
        simulation_.startRun();
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        buildingRotationWheelAccumulator_ = 0.0;
        buildingRotationCooldownRemaining_ = 0.0;
        cameraShakeRemaining_ = 0.0;
        damageIndicators_.clear();
        playerDamageFlashRemaining_ = 0.0;
        DisableCursor();
    }
    if (IsKeyPressed(KEY_P)) {
        simulation_.togglePause();
        fixedStep_.reset();
        if (simulation_.snapshot().state == RunState::Paused) {
            EnableCursor();
        } else if (simulation_.snapshot().state != RunState::MainMenu) {
            DisableCursor();
        }
    }
    if (snapshot.state != RunState::MainMenu && IsKeyPressed(KEY_R)) {
        simulation_.restartRun();
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        buildingRotationWheelAccumulator_ = 0.0;
        buildingRotationCooldownRemaining_ = 0.0;
        cameraShakeRemaining_ = 0.0;
        damageIndicators_.clear();
        playerDamageFlashRemaining_ = 0.0;
    }
    if (snapshot.state != RunState::MainMenu) {
        if (IsKeyPressed(KEY_T)) {
            slowMotion_ = !slowMotion_;
            fixedStep_.reset();
        }
        if (IsKeyPressed(KEY_C)) {
            showColliders_ = !showColliders_;
        }
        if (IsKeyPressed(KEY_H)) {
            showFlowField_ = !showFlowField_;
        }
        if (IsKeyPressed(KEY_L)) {
            showSpatialHash_ = !showSpatialHash_;
        }
        if (IsKeyPressed(KEY_J)) {
            hideHud_ = !hideHud_;
        }
        if (IsKeyPressed(KEY_Y)) {
            environment_.toggleFrozen();
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            environment_.adjustTime(-0.025F);
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            environment_.adjustTime(0.025F);
        }
        if (IsKeyPressed(KEY_APOSTROPHE)) {
            environment_.cycleProfile();
        }
        if (IsKeyPressed(KEY_BACKSLASH)) {
            environment_.useAutomaticTime();
        }
    }

    if (acceptsGameplayInput(simulation_.snapshot().state)) {
        const auto currentSnapshot = simulation_.snapshot();
        input_.moveForward =
            static_cast<double>(IsKeyDown(KEY_W)) - static_cast<double>(IsKeyDown(KEY_S));
        input_.moveRight =
            static_cast<double>(IsKeyDown(KEY_D)) - static_cast<double>(IsKeyDown(KEY_A));
        input_.sprint = IsKeyDown(KEY_LEFT_SHIFT);

        const Vector2 mouseDelta = GetMouseDelta();
        pendingYaw_ += static_cast<double>(mouseDelta.x) * MouseSensitivity;
        pendingPitch_ -= static_cast<double>(mouseDelta.y) * MouseSensitivity;
        pendingJump_ = pendingJump_ || IsKeyPressed(KEY_SPACE);
        if (IsKeyPressed(KEY_ONE)) {
            pendingBuildingSelection_ = BuildingType::Core;
        }
        if (IsKeyPressed(KEY_TWO)) {
            pendingBuildingSelection_ = BuildingType::Wall;
        }
        if (IsKeyPressed(KEY_THREE)) {
            pendingBuildingSelection_ = BuildingType::Turret;
        }
        if (IsKeyPressed(KEY_FOUR)) {
            pendingBuildingSelection_ = BuildingType::GoldMine;
        }
        if (IsKeyPressed(KEY_FIVE)) {
            pendingBuildingSelection_ = BuildingType::Cannon;
        }
        if (IsKeyPressed(KEY_SIX)) {
            pendingBuildingSelection_ = BuildingType::SlowTrap;
        }
        if (IsKeyPressed(KEY_SEVEN)) {
            pendingBuildingSelection_ = BuildingType::Gate;
        }
        pendingBuildingCancel_ =
            pendingBuildingCancel_ || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        pendingStartWave_ = pendingStartWave_ || IsKeyPressed(KEY_N);
        pendingUnlimitedResources_ =
            pendingUnlimitedResources_ || IsKeyPressed(KEY_O);
        pendingWeaponToggle_ = pendingWeaponToggle_ || IsKeyPressed(KEY_Q);
        pendingWeaponUpgrade_ = pendingWeaponUpgrade_ || IsKeyPressed(KEY_V);
        pendingBombThrow_ = pendingBombThrow_ || IsKeyPressed(KEY_G);
        pendingDefeatAllEnemies_ =
            pendingDefeatAllEnemies_ || IsKeyPressed(KEY_K);
        pendingToggleInvulnerability_ =
            pendingToggleInvulnerability_ || IsKeyPressed(KEY_I);
        pendingDamageCore_ = pendingDamageCore_ || IsKeyPressed(KEY_M);
        pendingSpawnEnemy_ = pendingSpawnEnemy_ || IsKeyPressed(KEY_B);
        if (IsKeyPressed(KEY_Z)) {
            switch (debugSpawnType_) {
            case EnemyType::Basic:
                debugSpawnType_ = EnemyType::Fast;
                break;
            case EnemyType::Fast:
                debugSpawnType_ = EnemyType::Heavy;
                break;
            case EnemyType::Heavy:
                debugSpawnType_ = EnemyType::Boss;
                break;
            case EnemyType::Boss:
                debugSpawnType_ = EnemyType::Basic;
                break;
            }
        }
        if (IsKeyPressed(KEY_U)) {
            if (!currentSnapshot.selectedBuilding && currentSnapshot.aimedBuilding) {
                pendingBuildingUpgrade_ =
                    UpgradeBuildingCommand{*currentSnapshot.aimedBuilding};
            } else if (currentSnapshot.coreId) {
                pendingBuildingUpgrade_ = UpgradeBuildingCommand{*currentSnapshot.coreId};
            }
        }
        if (!currentSnapshot.selectedBuilding && currentSnapshot.aimedBuilding) {
            if (IsKeyPressed(KEY_F)) {
                pendingBuildingRepair_ =
                    RepairBuildingCommand{*currentSnapshot.aimedBuilding};
            }
            if (IsKeyPressed(KEY_X)) {
                pendingBuildingSale_ =
                    SellBuildingCommand{*currentSnapshot.aimedBuilding};
            }
        }
        if (IsKeyPressed(KEY_E) && currentSnapshot.aimedBuilding) {
            pendingGateToggle_ = ToggleGateCommand{*currentSnapshot.aimedBuilding};
        }
        const float wheel = GetMouseWheelMove();
        if (currentSnapshot.selectedBuilding) {
            buildingRotationWheelAccumulator_ = std::clamp(
                buildingRotationWheelAccumulator_ +
                    static_cast<double>(wheel),
                -1.0, 1.0);
            if (buildingRotationCooldownRemaining_ <= 0.0 &&
                std::abs(buildingRotationWheelAccumulator_) >= 1.0) {
                pendingBuildingRotation_ +=
                    buildingRotationWheelAccumulator_ > 0.0 ? 1 : -1;
                buildingRotationWheelAccumulator_ = 0.0;
                buildingRotationCooldownRemaining_ = 0.2;
            }
        } else {
            buildingRotationWheelAccumulator_ = 0.0;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (currentSnapshot.buildingPreview) {
                pendingBuildingPlacement_ = PlaceBuildingCommand{
                    .type = currentSnapshot.buildingPreview->type,
                    .gridPosition = currentSnapshot.buildingPreview->gridPosition,
                    .rotation = currentSnapshot.buildingPreview->rotation,
                };
            } else if (!pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::Rifle) {
                pendingRifleShot_ = true;
            } else if (!pendingBuildingSelection_) {
                pendingPickaxe_ = true;
            }
        }
    } else {
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
    }
}

void App::update() {
    const double frameSeconds = static_cast<double>(GetFrameTime());
    statusMessageRemaining_ =
        std::max(0.0, statusMessageRemaining_ - frameSeconds);
    cameraShakeRemaining_ = std::max(0.0, cameraShakeRemaining_ - frameSeconds);
    playerDamageFlashRemaining_ =
        std::max(0.0, playerDamageFlashRemaining_ - frameSeconds);
    buildingRotationCooldownRemaining_ = std::max(
        0.0, buildingRotationCooldownRemaining_ - frameSeconds);
    if (cameraShakeRemaining_ <= 0.0) {
        cameraShakeStrength_ = 0.0;
    }
    for (auto& effect : effects_) {
        effect.remaining = std::max(0.0, effect.remaining - frameSeconds);
    }
    std::erase_if(effects_, [](const PresentationEffect& effect) {
        return effect.remaining <= 0.0;
    });
    for (auto& arrow : arrowVisuals_) {
        arrow.remaining =
            std::max(0.0, arrow.remaining - frameSeconds);
    }
    std::erase_if(arrowVisuals_, [](const ArrowVisual& arrow) {
        return arrow.remaining <= 0.0;
    });
    for (auto& indicator : damageIndicators_) {
        indicator.remaining = std::max(0.0, indicator.remaining - frameSeconds);
    }
    std::erase_if(damageIndicators_, [](const DamageIndicator& indicator) {
        return indicator.remaining <= 0.0;
    });
    bool consumedTransientInput = false;
    double tickMilliseconds = 0.0;
    std::size_t measuredTicks = 0;
    const double simulationFrameSeconds = slowMotion_ ? frameSeconds * 0.2 : frameSeconds;
    fixedStep_.advance(
        simulationFrameSeconds,
        [this, &consumedTransientInput, &tickMilliseconds,
         &measuredTicks](double deltaSeconds) {
        PlayerCommand tickInput = input_;
        if (!consumedTransientInput) {
            tickInput.lookYaw = pendingYaw_;
            tickInput.lookPitch = pendingPitch_;
            tickInput.jump = pendingJump_;
            tickInput.usePickaxe = pendingPickaxe_;
            tickInput.fireRifle = pendingRifleShot_;
            tickInput.selectBuilding = pendingBuildingSelection_;
            tickInput.cancelBuilding = pendingBuildingCancel_;
            tickInput.placeBuilding = pendingBuildingPlacement_;
            tickInput.rotateBuilding = pendingBuildingRotation_;
            if (pendingStartWave_) {
                tickInput.startWaveEarly = StartWaveEarlyCommand{};
            }
            if (pendingUnlimitedResources_) {
                tickInput.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
            }
            tickInput.upgradeBuilding = pendingBuildingUpgrade_;
            tickInput.repairBuilding = pendingBuildingRepair_;
            tickInput.sellBuilding = pendingBuildingSale_;
            if (pendingWeaponToggle_) {
                tickInput.toggleWeapon = ToggleWeaponCommand{};
            }
            if (pendingWeaponUpgrade_) {
                tickInput.upgradeWeapon = UpgradeWeaponCommand{};
            }
            if (pendingBombThrow_) {
                tickInput.useConsumable = UseConsumableCommand{};
            }
            if (pendingDefeatAllEnemies_) {
                tickInput.defeatAllEnemies = DefeatAllEnemiesCommand{};
            }
            if (pendingToggleInvulnerability_) {
                tickInput.toggleInvulnerability = ToggleInvulnerabilityCommand{};
            }
            if (pendingDamageCore_) {
                tickInput.damageCore = DamageCoreCommand{};
            }
            if (pendingSpawnEnemy_) {
                tickInput.spawnEnemy = SpawnEnemyCommand{debugSpawnType_};
            }
            tickInput.toggleGate = pendingGateToggle_;
            consumedTransientInput = true;
        }
        const auto tickStarted = std::chrono::steady_clock::now();
        simulation_.tick(deltaSeconds, tickInput);
        const auto tickFinished = std::chrono::steady_clock::now();
        tickMilliseconds +=
            std::chrono::duration<double, std::milli>(tickFinished - tickStarted)
                .count();
        ++measuredTicks;
        });
    if (measuredTicks > 0) {
        const double average = tickMilliseconds / static_cast<double>(measuredTicks);
        simulationTickMilliseconds_ =
            simulationTickMilliseconds_ == 0.0
                ? average
                : simulationTickMilliseconds_ * 0.9 + average * 0.1;
        peakSimulationTickMilliseconds_ =
            std::max(peakSimulationTickMilliseconds_, average);
    }
    if (consumedTransientInput) {
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
        pendingJump_ = false;
        pendingPickaxe_ = false;
        pendingRifleShot_ = false;
        pendingBuildingSelection_.reset();
        pendingBuildingCancel_ = false;
        pendingBuildingPlacement_.reset();
        pendingBuildingRotation_ = 0;
        pendingStartWave_ = false;
        pendingUnlimitedResources_ = false;
        pendingBuildingUpgrade_.reset();
        pendingBuildingRepair_.reset();
        pendingBuildingSale_.reset();
        pendingWeaponToggle_ = false;
        pendingWeaponUpgrade_ = false;
        pendingBombThrow_ = false;
        pendingDefeatAllEnemies_ = false;
        pendingToggleInvulnerability_ = false;
        pendingDamageCore_ = false;
        pendingSpawnEnemy_ = false;
        pendingGateToggle_.reset();
    }
    const auto events = simulation_.takeEvents();
    const auto eventSnapshot = simulation_.snapshot();
    for (const auto& event : events) {
        if (event.type == GameEventType::ProjectileHit &&
            event.sourceId) {
            const auto source = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& building) {
                    return building.id == *event.sourceId &&
                           building.type == BuildingType::Turret;
                });
            if (source != eventSnapshot.buildings.end()) {
                const Vec3 origin{
                    static_cast<double>(source->gridPosition.x),
                    1.4,
                    static_cast<double>(source->gridPosition.z),
                };
                const double deltaX = event.position.x - origin.x;
                const double deltaY = event.position.y - origin.y;
                const double deltaZ = event.position.z - origin.z;
                const double distance = std::sqrt(
                    deltaX * deltaX + deltaY * deltaY +
                    deltaZ * deltaZ);
                const double duration =
                    std::clamp(distance / 18.0, 0.08, 0.35);
                arrowVisuals_.push_back({
                    .origin = origin,
                    .target = event.position,
                    .remaining = duration,
                    .duration = duration,
                });
            }
        }
        if (event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ProjectileHit ||
            event.type == GameEventType::EnemyKilled) {
            addEffect(PresentationEffectType::Hit, event.position, 0.22);
        } else if (event.type == GameEventType::ResourceCollected) {
            addEffect(PresentationEffectType::ResourceBurst, event.position, 0.65);
        } else if (event.type == GameEventType::Explosion) {
            addEffect(PresentationEffectType::Explosion, event.position, 0.8);
            addCameraShake(0.25, 0.12);
        } else if (event.type == GameEventType::BuildingDestroyed) {
            addEffect(PresentationEffectType::Debris, event.position, 0.8);
            addCameraShake(0.18, 0.08);
        } else if (event.type == GameEventType::BossRamImpact) {
            addEffect(PresentationEffectType::RamImpact, event.position, 0.7);
            addCameraShake(0.35, 0.2);
        } else if (event.type == GameEventType::CoreDamaged) {
            addCameraShake(0.1, 0.04);
        }
        if (event.type == GameEventType::PlayerDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
            playerDamageFlashRemaining_ = 0.18;
        } else if (event.type == GameEventType::CoreDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
        } else if (event.type == GameEventType::BossRamImpact) {
            addDamageIndicator(event.position, eventSnapshot, true);
        }

        std::string message;
        if (event.type == GameEventType::BuildingUpgraded && event.buildingType) {
            message = std::string(buildingName(*event.buildingType)) + " upgraded";
        } else if (event.type == GameEventType::BuildingUpgradeRejected &&
                   event.upgradeError) {
            message = upgradeErrorMessage(*event.upgradeError);
        } else if (event.type == GameEventType::BuildingRepaired && event.buildingType) {
            message = std::string(buildingName(*event.buildingType)) + " repaired";
        } else if ((event.type == GameEventType::BuildingRepairRejected ||
                    event.type == GameEventType::BuildingSellRejected) &&
                   event.buildingActionError) {
            message = buildingActionErrorMessage(*event.buildingActionError);
        } else if (event.type == GameEventType::BuildingSold && event.buildingType) {
            message = std::string(buildingName(*event.buildingType)) + " sold";
        } else if (event.type == GameEventType::WeaponUpgraded) {
            message = "Rifle upgraded to level " + std::to_string(event.amount);
        } else if (event.type == GameEventType::WeaponUpgradeRejected &&
                   event.weaponUpgradeError) {
            message = weaponUpgradeErrorMessage(*event.weaponUpgradeError);
        } else if (event.type == GameEventType::GateToggleRejected) {
            message = "Gate blocked or not under crosshair";
        } else if (event.type == GameEventType::PlayerRespawned) {
            message = "You died - respawned at Core";
        } else if (event.type == GameEventType::WaveRewardGranted) {
            message = "Night cleared: +" + std::to_string(event.amount) + " Gold";
        }
        if (!message.empty()) {
            statusMessage_ = std::move(message);
            statusMessageRemaining_ = 2.5;
        }
    }
}

void App::addEffect(PresentationEffectType type, Vec3 position, double duration) {
    constexpr std::size_t MaxEffects = 128;
    if (effects_.size() >= MaxEffects) {
        effects_.erase(effects_.begin());
    }
    effects_.push_back({
        .type = type,
        .position = position,
        .remaining = duration,
        .duration = duration,
    });
}

void App::addCameraShake(double duration, double strength) {
    cameraShakeRemaining_ = std::max(cameraShakeRemaining_, duration);
    cameraShakeStrength_ = std::max(cameraShakeStrength_, strength);
}

void App::addDamageIndicator(Vec3 sourcePosition,
                             const SimulationSnapshot& snapshot, bool severe) {
    const double offsetX = sourcePosition.x - snapshot.playerPosition.x;
    const double offsetZ = sourcePosition.z - snapshot.playerPosition.z;
    const double worldAngle = std::atan2(offsetX, -offsetZ);
    const double relativeAngle =
        std::atan2(std::sin(worldAngle - snapshot.playerYaw),
                   std::cos(worldAngle - snapshot.playerYaw));
    for (auto& indicator : damageIndicators_) {
        const double difference =
            std::atan2(std::sin(indicator.relativeAngle - relativeAngle),
                       std::cos(indicator.relativeAngle - relativeAngle));
        if (std::abs(difference) < 0.2) {
            indicator.relativeAngle = relativeAngle;
            indicator.severe = indicator.severe || severe;
            indicator.duration = indicator.severe ? 1.4 : 1.0;
            indicator.remaining = indicator.duration;
            return;
        }
    }
    constexpr std::size_t MaxDamageIndicators = 12;
    if (damageIndicators_.size() >= MaxDamageIndicators) {
        damageIndicators_.erase(damageIndicators_.begin());
    }
    const double duration = severe ? 1.4 : 1.0;
    damageIndicators_.push_back({
        .relativeAngle = relativeAngle,
        .remaining = duration,
        .duration = duration,
        .severe = severe,
    });
}

void App::render() {
    const auto snapshot = simulation_.snapshot();

    if (snapshot.state == RunState::MainMenu) {
        renderer_->beginUiOnlyFrame({18, 22, 31, 255});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        ui_.drawPanel({centerX - 250.0F, centerY - 150.0F,
                       500.0F, 300.0F});
        ui_.drawInsetPanel({centerX - 210.0F, centerY - 112.0F,
                            420.0F, 92.0F});
        drawCentered("IT'S ALMOST NIGHT",
                     static_cast<int>(centerY) - 92, 42,
                     {245, 220, 174, 255});
        pendingStartFromUi_ =
            ui_.drawButton({centerX - 140.0F, centerY + 34.0F,
                            280.0F, 64.0F},
                           "START RUN") ||
            pendingStartFromUi_;
        drawCentered("ENTER", static_cast<int>(centerY) + 112, 16,
                     {199, 174, 142, 255});
    } else {
        const double cosPitch = std::cos(snapshot.playerPitch);
        Vector3 position = {
            static_cast<float>(snapshot.playerPosition.x),
            static_cast<float>(snapshot.playerPosition.y),
            static_cast<float>(snapshot.playerPosition.z),
        };
        const Vector3 forward = {
            static_cast<float>(std::sin(snapshot.playerYaw) * cosPitch),
            static_cast<float>(std::sin(snapshot.playerPitch)),
            static_cast<float>(-std::cos(snapshot.playerYaw) * cosPitch),
        };
        if (cameraShakeRemaining_ > 0.0) {
            const double visualTime = GetTime();
            const float shake =
                static_cast<float>(cameraShakeStrength_ * cameraShakeRemaining_ / 0.35);
            position.x += static_cast<float>(std::sin(visualTime * 83.0)) * shake;
            position.y += static_cast<float>(std::cos(visualTime * 97.0)) * shake * 0.7F;
            position.z += static_cast<float>(std::sin(visualTime * 71.0)) * shake * 0.5F;
        }
        const Camera3D camera = {
            .position = position,
            .target = Vector3Add(position, forward),
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 75.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        const auto cannonYaw = [&snapshot](const BuildingInstance& building) {
            const auto runtime = std::find_if(
                snapshot.cannons.begin(), snapshot.cannons.end(),
                [&building](const CannonRuntime& cannon) {
                    return cannon.buildingId == building.id;
                });
            if (runtime != snapshot.cannons.end()) {
                return static_cast<float>(runtime->yaw);
            }
            constexpr float QuarterTurn = PI * 0.5F;
            return static_cast<float>(building.rotation) * QuarterTurn;
        };
        const auto towerYaw = [&snapshot](const BuildingInstance& building) {
            const auto runtime = std::find_if(
                snapshot.towers.begin(), snapshot.towers.end(),
                [&building](const TowerRuntime& tower) {
                    return tower.buildingId == building.id;
                });
            if (runtime != snapshot.towers.end()) {
                return static_cast<float>(runtime->yaw);
            }
            constexpr float QuarterTurn = PI * 0.5F;
            return static_cast<float>(building.rotation) * QuarterTurn;
        };
        const auto cannonPitch = [&snapshot](const BuildingInstance& building) {
            const auto runtime = std::find_if(
                snapshot.cannons.begin(), snapshot.cannons.end(),
                [&building](const CannonRuntime& cannon) {
                    return cannon.buildingId == building.id;
                });
            return runtime != snapshot.cannons.end()
                       ? static_cast<float>(runtime->pitch)
                       : 0.0F;
        };

        constexpr Color DayGround{48, 78, 52, 255};
        constexpr Color NightGround{21, 38, 34, 255};

        float automaticTime = environment_.timeOfDay();
        if (snapshot.state == RunState::Gathering ||
            snapshot.state == RunState::BuildPhase) {
            automaticTime = 0.25F;
        } else if (snapshot.state == RunState::Sunset) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.25F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        } else if (snapshot.state == RunState::Wave) {
            automaticTime = 0.75F;
        } else if (snapshot.state == RunState::WaveComplete) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.75F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        }
        environment_.setAutomaticTime(automaticTime);
        const EnvironmentState environment = environment_.state();
        const float nightAmount = environment.nightFactor;
        const Color ground = {
            static_cast<unsigned char>(
                static_cast<float>(DayGround.r) +
                (static_cast<float>(NightGround.r) -
                 static_cast<float>(DayGround.r)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.g) +
                (static_cast<float>(NightGround.g) -
                 static_cast<float>(DayGround.g)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.b) +
                (static_cast<float>(NightGround.b) -
                 static_cast<float>(DayGround.b)) *
                    nightAmount),
            255,
        };
        const Vector3 lightDirection =
            Vector3Scale(environment.celestialDirection, -1.0F);
        const WorldLighting lighting{
            .cameraPosition = camera.position,
            .sunDirection = lightDirection,
            .sunColor = environment.sunColor,
            .sunIntensity = environment.sunIntensity,
            .skyAmbientColor = environment.skyAmbientColor,
            .groundAmbientColor = environment.groundAmbientColor,
            .ambientIntensity = environment.ambientIntensity,
            .fogColor = colorToVector(environment.fogColor),
            .fogStart = environment.fogStart,
            .fogEnd = environment.fogEnd,
            .dayNightTint = environment.dayNightTint,
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };
        const Vector3 cameraRight =
            Vector3Normalize(Vector3CrossProduct(forward, {0.0F, 1.0F, 0.0F}));
        const Vector3 cameraUp =
            Vector3Normalize(Vector3CrossProduct(cameraRight, forward));
        const SkyState skyState{
            .cameraForward = forward,
            .cameraRight = cameraRight,
            .cameraUp = cameraUp,
            .verticalFovDegrees = camera.fovy,
            .zenithColor = colorToVector(environment.skyTop),
            .horizonColor = colorToVector(environment.skyHorizon),
            .lowerSkyColor = colorToVector(environment.lowerSky),
            .celestialDirection = environment.celestialDirection,
            .celestialColor = environment.celestialColor,
            .celestialIntensity = environment.sunIntensity,
            .timeSeconds = static_cast<float>(GetTime()),
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };
        const auto hitFlashAt = [this](Vec3 position, double radius) {
            float amount = 0.0F;
            const double radiusSquared = radius * radius;
            for (const auto& effect : effects_) {
                if (effect.type != PresentationEffectType::Hit) {
                    continue;
                }
                const double offsetX = effect.position.x - position.x;
                const double offsetY = effect.position.y - position.y;
                const double offsetZ = effect.position.z - position.z;
                const double distanceSquared =
                    offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ;
                if (distanceSquared <= radiusSquared) {
                    amount =
                        std::max(amount, static_cast<float>(
                                             effect.remaining / effect.duration));
                }
            }
            return amount;
        };

        const Vector3 shadowFocus{
            static_cast<float>(snapshot.playerPosition.x),
            0.0F,
            static_cast<float>(snapshot.playerPosition.z),
        };
        const auto withinLocalShadowDistance =
            [shadowFocus](Vector3 position, float maximumDistance) {
                const float offsetX = position.x - shadowFocus.x;
                const float offsetZ = position.z - shadowFocus.z;
                return offsetX * offsetX + offsetZ * offsetZ <=
                       maximumDistance * maximumDistance;
            };
        if (renderer_->beginShadowPass(lighting, shadowFocus)) {
            const float mapSize =
                static_cast<float>(snapshot.worldLimit * 2.0);
            DrawPlane({0.0F, 0.0F, 0.0F}, {mapSize, mapSize}, WHITE);
            for (const auto& obstacle : snapshot.mapObstacles) {
                const float width = static_cast<float>(
                    obstacle.collision.maxX - obstacle.collision.minX);
                const float depth = static_cast<float>(
                    obstacle.collision.maxZ - obstacle.collision.minZ);
                const Vector3 center{
                    static_cast<float>(
                        (obstacle.collision.minX + obstacle.collision.maxX) *
                        0.5),
                    static_cast<float>(obstacle.height * 0.5),
                    static_cast<float>(
                        (obstacle.collision.minZ + obstacle.collision.maxZ) *
                        0.5),
                };
                if (renderer_->shadowCasterVisible(
                        center, std::max(width, depth) * 0.5F)) {
                    DrawCube(center, width,
                             static_cast<float>(obstacle.height), depth, WHITE);
                }
            }
            for (const auto& node : snapshot.resourceNodes) {
                if (!node.active) {
                    continue;
                }
                const Vector3 nodePosition{
                    static_cast<float>(node.position.x),
                    static_cast<float>(node.position.y),
                    static_cast<float>(node.position.z),
                };
                if (!renderer_->shadowCasterVisible(
                        nodePosition, static_cast<float>(node.radius)) ||
                    !withinLocalShadowDistance(nodePosition, 45.0F)) {
                    continue;
                }
                if (node.type == ResourceType::Wood) {
                    if (!renderer_->drawTree(
                            {nodePosition.x, 0.0F, nodePosition.z})) {
                        DrawCylinder(
                            {nodePosition.x, 0.9F, nodePosition.z},
                            0.32F, 0.42F, 1.8F, 8, WHITE);
                        DrawSphere(
                            {nodePosition.x, 2.2F, nodePosition.z},
                            1.15F, WHITE);
                    }
                } else {
                    if (!renderer_->drawRock(
                            {nodePosition.x, 0.0F, nodePosition.z})) {
                        DrawSphere(nodePosition, 0.9F, WHITE);
                    }
                }
            }
            for (const auto& building : snapshot.buildings) {
                const float x =
                    static_cast<float>(building.gridPosition.x);
                const float z =
                    static_cast<float>(building.gridPosition.z);
                if (!renderer_->shadowCasterVisible({x, 1.0F, z}, 2.2F)) {
                    continue;
                }
                if (building.type == BuildingType::Core) {
                    constexpr float QuarterTurn = PI * 0.5F;
                    if (!renderer_->drawCore(
                            {x, 0.0F, z},
                            static_cast<float>(building.rotation) *
                                QuarterTurn)) {
                        DrawCube({x, 1.25F, z}, 2.0F, 2.5F,
                                 2.0F, WHITE);
                    }
                } else if (building.type == BuildingType::Wall) {
                    const std::uint8_t connections = wallConnectionMask(
                        snapshot.buildings, building.gridPosition);
                    const auto drawSection =
                        [x, z](float offsetX, float offsetZ, float width,
                               float depth) {
                            DrawCube({x + offsetX, 1.0F, z + offsetZ},
                                     width, 2.0F, depth, WHITE);
                        };
                    if (connections == 0U) {
                        drawSection(0.0F, 0.0F, 1.0F, 1.0F);
                    } else {
                        drawSection(0.0F, 0.0F, 0.5F, 0.5F);
                        if ((connections & WallConnectionNorth) != 0U) {
                            drawSection(0.0F, -0.35F, 0.5F, 0.7F);
                        }
                        if ((connections & WallConnectionEast) != 0U) {
                            drawSection(0.35F, 0.0F, 0.7F, 0.5F);
                        }
                        if ((connections & WallConnectionSouth) != 0U) {
                            drawSection(0.0F, 0.35F, 0.5F, 0.7F);
                        }
                        if ((connections & WallConnectionWest) != 0U) {
                            drawSection(-0.35F, 0.0F, 0.7F, 0.5F);
                        }
                    }
                } else if (building.type == BuildingType::Turret) {
                    if (!renderer_->drawCrossbow(
                            {x, 0.0F, z}, towerYaw(building))) {
                        DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                                 WHITE);
                        DrawCylinder({x, 1.45F, z}, 0.42F, 0.32F,
                                     0.7F, 8, WHITE);
                        DrawCube({x, 1.55F, z - 0.55F}, 0.18F,
                                 0.18F, 1.0F, WHITE);
                    }
                } else if (building.type == BuildingType::GoldMine) {
                    DrawCube({x, 0.55F, z}, 1.0F, 1.1F, 1.0F, WHITE);
                    DrawCylinder({x, 1.25F, z}, 0.32F, 0.48F, 0.7F,
                                 8, WHITE);
                    DrawSphere({x, 1.72F, z}, 0.22F, WHITE);
                } else if (building.type == BuildingType::Cannon) {
                    if (!renderer_->drawCannon({x, 0.0F, z},
                                               cannonYaw(building),
                                               cannonPitch(building))) {
                        DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                                 WHITE);
                        DrawSphere({x, 1.35F, z}, 0.48F, WHITE);
                        DrawCube({x, 1.45F, z - 0.75F}, 0.28F,
                                 0.28F, 1.4F, WHITE);
                    }
                } else if (building.type == BuildingType::SlowTrap) {
                    DrawCube({x, 0.08F, z}, 1.0F, 0.16F, 1.0F,
                             WHITE);
                } else if ((building.rotation % 2U) == 0U) {
                    DrawCube({x - 0.38F, 1.0F, z}, 0.22F, 2.0F,
                             1.0F, WHITE);
                    DrawCube({x + 0.38F, 1.0F, z}, 0.22F, 2.0F,
                             1.0F, WHITE);
                    if (!building.open) {
                        DrawCube({x, 1.0F, z}, 0.55F, 1.7F, 0.18F,
                                 WHITE);
                    }
                } else {
                    DrawCube({x, 1.0F, z - 0.38F}, 1.0F, 2.0F,
                             0.22F, WHITE);
                    DrawCube({x, 1.0F, z + 0.38F}, 1.0F, 2.0F,
                             0.22F, WHITE);
                    if (!building.open) {
                        DrawCube({x, 1.0F, z}, 0.18F, 1.7F, 0.55F,
                                 WHITE);
                    }
                }
            }
            for (const auto& enemy : snapshot.enemies) {
                if (!enemy.active) {
                    continue;
                }
                const Vector3 enemyPosition{
                    static_cast<float>(enemy.position.x),
                    static_cast<float>(enemy.position.y),
                    static_cast<float>(enemy.position.z),
                };
                float width = 0.8F;
                float height = 1.6F;
                if (enemy.type == EnemyType::Fast) {
                    width = 0.65F;
                    height = 1.35F;
                } else if (enemy.type == EnemyType::Heavy) {
                    width = 1.15F;
                    height = 2.0F;
                } else if (enemy.type == EnemyType::Boss) {
                    width = 2.0F;
                    height = 3.2F;
                }
                float casterDistance = 36.0F;
                if (enemy.type == EnemyType::Heavy) {
                    casterDistance = 45.0F;
                } else if (enemy.type == EnemyType::Boss) {
                    casterDistance = 60.0F;
                }
                if (!renderer_->shadowCasterVisible(enemyPosition, width) ||
                    !withinLocalShadowDistance(enemyPosition,
                                               casterDistance)) {
                    continue;
                }
                DrawCube(enemyPosition, width, height, width, WHITE);
                DrawSphere(
                    {enemyPosition.x,
                     enemyPosition.y + height * 0.62F,
                     enemyPosition.z},
                    width * 0.52F, WHITE);
            }
            renderer_->endShadowPass();
        }

        renderer_->beginWorldPass(environment.skyHorizon);
        renderer_->drawSky(skyState);
        BeginMode3D(camera);
        renderer_->beginWorldShader(lighting);
        const float mapSize = static_cast<float>(snapshot.worldLimit * 2.0);
        WorldMaterialState terrainMaterial{};
        terrainMaterial.terrainAmount = 1.0F;
        terrainMaterial.bakedAo = 0.9F;
        renderer_->setWorldMaterial(terrainMaterial);
        DrawPlane({0.0F, 0.0F, 0.0F}, {mapSize, mapSize}, ground);
        WorldMaterialState obstacleMaterial{};
        obstacleMaterial.bakedAo = 0.74F;
        renderer_->setWorldMaterial(obstacleMaterial);
        for (const auto& obstacle : snapshot.mapObstacles) {
            const float width =
                static_cast<float>(obstacle.collision.maxX - obstacle.collision.minX);
            const float depth =
                static_cast<float>(obstacle.collision.maxZ - obstacle.collision.minZ);
            const Vector3 center{
                static_cast<float>((obstacle.collision.minX + obstacle.collision.maxX) * 0.5),
                static_cast<float>(obstacle.height * 0.5),
                static_cast<float>((obstacle.collision.minZ + obstacle.collision.maxZ) * 0.5),
            };
            DrawCube(center, width, static_cast<float>(obstacle.height), depth,
                     {99, 111, 122, 255});
        }
        for (const auto& node : snapshot.resourceNodes) {
            if (!node.active) {
                continue;
            }

            const Vector3 nodePosition = {
                static_cast<float>(node.position.x),
                static_cast<float>(node.position.y),
                static_cast<float>(node.position.z),
            };
            const bool aimed = snapshot.aimedResource && *snapshot.aimedResource == node.id;
            WorldMaterialState material{};
            material.bakedAo = 0.78F;
            material.hitFlashAmount = hitFlashAt(node.position, 1.5);
            material.selectionAmount = aimed ? 0.28F : 0.0F;
            material.selectionTint = {1.0F, 0.78F, 0.2F};
            renderer_->setWorldMaterial(material);
            if (node.type == ResourceType::Wood) {
                if (!renderer_->drawTree(
                        {nodePosition.x, 0.0F, nodePosition.z})) {
                    DrawCylinder(
                        {nodePosition.x, 0.9F, nodePosition.z}, 0.32F,
                        0.42F, 1.8F, 8, {112, 74, 42, 255});
                    DrawSphere(
                        {nodePosition.x, 2.2F, nodePosition.z}, 1.15F,
                        aimed ? Color{132, 205, 92, 255}
                              : Color{58, 124, 67, 255});
                }
            } else {
                if (!renderer_->drawRock(
                        {nodePosition.x, 0.0F, nodePosition.z})) {
                    DrawSphere(
                        nodePosition, 0.9F,
                        aimed ? Color{191, 205, 216, 255}
                              : Color{104, 116, 128, 255});
                }
            }
        }
        for (const auto& building : snapshot.buildings) {
            const float x = static_cast<float>(building.gridPosition.x);
            const float z = static_cast<float>(building.gridPosition.z);
            WorldMaterialState material{};
            material.bakedAo = 0.72F;
            material.selectionAmount =
                snapshot.aimedBuilding && *snapshot.aimedBuilding == building.id
                    ? 0.24F
                    : 0.0F;
            renderer_->setWorldMaterial(material);
            if (building.type == BuildingType::Core) {
                constexpr float QuarterTurn = PI * 0.5F;
                if (!renderer_->drawCore(
                        {x, 0.0F, z},
                        static_cast<float>(building.rotation) *
                            QuarterTurn)) {
                    DrawCube({x, 1.25F, z}, 2.0F, 2.5F, 2.0F,
                             {219, 151, 60, 255});
                }
                if (nightAmount > 0.0F) {
                    const unsigned char alpha =
                        static_cast<unsigned char>(80.0F + 120.0F * nightAmount);
                    DrawSphere({x, 2.35F, z}, 0.22F, {255, 204, 91, alpha});
                }
            } else if (building.type == BuildingType::Wall) {
                const std::uint8_t connections =
                    wallConnectionMask(snapshot.buildings, building.gridPosition);
                const auto drawSection = [x, z](float offsetX, float offsetZ, float width,
                                                float depth) {
                    DrawCube({x + offsetX, 1.0F, z + offsetZ}, width, 2.0F, depth,
                             {126, 86, 54, 255});
                };
                if (connections == 0U) {
                    drawSection(0.0F, 0.0F, 1.0F, 1.0F);
                } else {
                    drawSection(0.0F, 0.0F, 0.5F, 0.5F);
                    if ((connections & WallConnectionNorth) != 0U) {
                        drawSection(0.0F, -0.35F, 0.5F, 0.7F);
                    }
                    if ((connections & WallConnectionEast) != 0U) {
                        drawSection(0.35F, 0.0F, 0.7F, 0.5F);
                    }
                    if ((connections & WallConnectionSouth) != 0U) {
                        drawSection(0.0F, 0.35F, 0.5F, 0.7F);
                    }
                    if ((connections & WallConnectionWest) != 0U) {
                        drawSection(-0.35F, 0.0F, 0.7F, 0.5F);
                    }
                }
            } else if (building.type == BuildingType::Turret) {
                if (!renderer_->drawCrossbow(
                        {x, 0.0F, z}, towerYaw(building))) {
                    DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                             {68, 83, 96, 255});
                    DrawCylinder({x, 1.45F, z}, 0.42F, 0.32F,
                                 0.7F, 8, {176, 128, 60, 255});
                    DrawCube({x, 1.55F, z - 0.55F}, 0.18F,
                             0.18F, 1.0F, {50, 58, 67, 255});
                }
            } else if (building.type == BuildingType::GoldMine) {
                DrawCube({x, 0.55F, z}, 1.0F, 1.1F, 1.0F, {71, 75, 82, 255});
                DrawCylinder({x, 1.25F, z}, 0.32F, 0.48F, 0.7F, 8,
                             {189, 142, 45, 255});
                DrawSphere({x, 1.72F, z}, 0.22F, GOLD);
            } else if (building.type == BuildingType::Cannon) {
                if (!renderer_->drawCannon({x, 0.0F, z},
                                           cannonYaw(building),
                                           cannonPitch(building))) {
                    DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                             {62, 70, 78, 255});
                    DrawSphere({x, 1.35F, z}, 0.48F,
                               {83, 91, 99, 255});
                    DrawCube({x, 1.45F, z - 0.75F}, 0.28F, 0.28F,
                             1.4F, {42, 48, 54, 255});
                }
            } else if (building.type == BuildingType::SlowTrap) {
                DrawCube({x, 0.08F, z}, 1.0F, 0.16F, 1.0F, {76, 110, 132, 255});
            } else {
                if ((building.rotation % 2U) == 0U) {
                    DrawCube({x - 0.38F, 1.0F, z}, 0.22F, 2.0F, 1.0F,
                             {112, 76, 48, 255});
                    DrawCube({x + 0.38F, 1.0F, z}, 0.22F, 2.0F, 1.0F,
                             {112, 76, 48, 255});
                    if (!building.open) {
                        DrawCube({x, 1.0F, z}, 0.55F, 1.7F, 0.18F,
                                 {151, 105, 62, 255});
                    }
                } else {
                    DrawCube({x, 1.0F, z - 0.38F}, 1.0F, 2.0F, 0.22F,
                             {112, 76, 48, 255});
                    DrawCube({x, 1.0F, z + 0.38F}, 1.0F, 2.0F, 0.22F,
                             {112, 76, 48, 255});
                    if (!building.open) {
                        DrawCube({x, 1.0F, z}, 0.18F, 1.7F, 0.55F,
                                 {151, 105, 62, 255});
                    }
                }
            }
        }
        renderer_->setWorldMaterial({});
        for (const auto& projectile : snapshot.cannonProjectiles) {
            if (projectile.active) {
                const Vector3 projectilePosition{
                    static_cast<float>(projectile.position.x),
                    static_cast<float>(projectile.position.y),
                    static_cast<float>(projectile.position.z),
                };
                if (!renderer_->drawCannonball(projectilePosition)) {
                    DrawSphere(projectilePosition, 0.2F,
                               {36, 39, 43, 255});
                }
            }
        }
        for (const auto& arrow : arrowVisuals_) {
            const double progress =
                1.0 - arrow.remaining / arrow.duration;
            const Vec3 arrowPosition{
                arrow.origin.x +
                    (arrow.target.x - arrow.origin.x) * progress,
                arrow.origin.y +
                    (arrow.target.y - arrow.origin.y) * progress,
                arrow.origin.z +
                    (arrow.target.z - arrow.origin.z) * progress,
            };
            (void)renderer_->drawArrow(
                {static_cast<float>(arrowPosition.x),
                 static_cast<float>(arrowPosition.y),
                 static_cast<float>(arrowPosition.z)},
                {static_cast<float>(arrow.target.x - arrow.origin.x),
                 static_cast<float>(arrow.target.y - arrow.origin.y),
                 static_cast<float>(arrow.target.z - arrow.origin.z)});
        }
        for (const auto& projectile : snapshot.bombProjectiles) {
            if (projectile.active) {
                DrawSphere({static_cast<float>(projectile.position.x),
                            static_cast<float>(projectile.position.y),
                            static_cast<float>(projectile.position.z)},
                           0.16F, {52, 57, 62, 255});
            }
        }
        for (const auto& enemy : snapshot.enemies) {
            if (!enemy.active) {
                continue;
            }
            const Vector3 enemyPosition = {
                static_cast<float>(enemy.position.x),
                static_cast<float>(enemy.position.y),
                static_cast<float>(enemy.position.z),
            };
            const bool aimed = snapshot.aimedEnemy && *snapshot.aimedEnemy == enemy.id;
            WorldMaterialState material{};
            material.bakedAo = 0.82F;
            material.hitFlashAmount = hitFlashAt(enemy.position, 1.6);
            material.selectionAmount = aimed ? 0.32F : 0.0F;
            material.selectionTint = {1.0F, 0.38F, 0.12F};
            renderer_->setWorldMaterial(material);
            float width = 0.8F;
            float height = 1.6F;
            Color body = {150, 55, 52, 255};
            if (enemy.type == EnemyType::Fast) {
                width = 0.65F;
                height = 1.35F;
                body = {191, 104, 52, 255};
            } else if (enemy.type == EnemyType::Heavy) {
                width = 1.15F;
                height = 2.0F;
                body = {93, 60, 105, 255};
            } else if (enemy.type == EnemyType::Boss) {
                width = 2.0F;
                height = 3.2F;
                body = {74, 35, 45, 255};
            }
            if (aimed) {
                body = {242, 118, 76, 255};
            } else if (enemy.slowRemaining > 0.0) {
                body = {70, 128, 170, 255};
            } else if (enemy.state == EnemyState::BossRamWindup) {
                body = {235, 64, 45, 255};
            }
            DrawCube(enemyPosition, width, height, width, body);
            DrawSphere({enemyPosition.x, enemyPosition.y + height * 0.62F, enemyPosition.z},
                       width * 0.52F, aimed ? ORANGE : MAROON);
        }
        renderer_->endWorldShader();

        if (renderer_->beginBlobShadowBatch(camera.position)) {
            for (const auto& node : snapshot.resourceNodes) {
                if (!node.active) {
                    continue;
                }
                const float radius =
                    std::max(static_cast<float>(node.radius), 0.45F);
                renderer_->drawBlobShadow(
                    {static_cast<float>(node.position.x), 0.018F,
                     static_cast<float>(node.position.z)},
                    radius, radius * 0.82F,
                    node.type == ResourceType::Wood ? 0.24F : 0.2F);
            }
            for (const auto& building : snapshot.buildings) {
                const float x =
                    static_cast<float>(building.gridPosition.x);
                const float z =
                    static_cast<float>(building.gridPosition.z);
                float radius = 0.62F;
                float opacity = 0.2F;
                if (building.type == BuildingType::Core) {
                    radius = 1.1F;
                    opacity = 0.24F;
                } else if (building.type == BuildingType::SlowTrap) {
                    radius = 0.54F;
                    opacity = 0.1F;
                }
                renderer_->drawBlobShadow({x, 0.018F, z}, radius,
                                          radius * 0.82F, opacity);
            }
            for (const auto& enemy : snapshot.enemies) {
                if (!enemy.active) {
                    continue;
                }
                float width = 0.8F;
                if (enemy.type == EnemyType::Fast) {
                    width = 0.65F;
                } else if (enemy.type == EnemyType::Heavy) {
                    width = 1.15F;
                } else if (enemy.type == EnemyType::Boss) {
                    width = 2.0F;
                }
                renderer_->drawBlobShadow(
                    {static_cast<float>(enemy.position.x), 0.02F,
                     static_cast<float>(enemy.position.z)},
                    width * 0.72F, width * 0.6F,
                    enemy.type == EnemyType::Boss ? 0.3F : 0.24F);
            }
            renderer_->endBlobShadowBatch();
        }

        if (snapshot.buildingPreview) {
            drawBuildGrid(
                {static_cast<float>(snapshot.playerPosition.x), 0.0F,
                 static_cast<float>(snapshot.playerPosition.z)},
                snapshot.worldLimit);
        }
        for (const auto& node : snapshot.resourceNodes) {
            if (!node.active || !snapshot.aimedResource ||
                *snapshot.aimedResource != node.id) {
                continue;
            }
            const Vector3 nodePosition{
                static_cast<float>(node.position.x),
                static_cast<float>(node.position.y),
                static_cast<float>(node.position.z),
            };
            DrawSphereWires(nodePosition, static_cast<float>(node.radius), 8, 8,
                            YELLOW);
        }
        for (const auto& enemy : snapshot.enemies) {
            if (!enemy.active) {
                continue;
            }
            const Vector3 enemyPosition{
                static_cast<float>(enemy.position.x),
                static_cast<float>(enemy.position.y),
                static_cast<float>(enemy.position.z),
            };
            float width = 0.8F;
            float height = 1.6F;
            if (enemy.type == EnemyType::Fast) {
                width = 0.65F;
                height = 1.35F;
            } else if (enemy.type == EnemyType::Heavy) {
                width = 1.15F;
                height = 2.0F;
            } else if (enemy.type == EnemyType::Boss) {
                width = 2.0F;
                height = 3.2F;
            }
            const bool aimed =
                snapshot.aimedEnemy && *snapshot.aimedEnemy == enemy.id;
            if (aimed) {
                DrawCubeWires(enemyPosition, width + 0.1F, height + 0.1F, width + 0.1F, YELLOW);
            } else if (enemy.state == EnemyState::BossRamWindup) {
                const float pulse =
                    0.12F + static_cast<float>(std::sin(snapshot.elapsedSeconds * 18.0)) * 0.06F;
                DrawCubeWires(enemyPosition, width + pulse, height + pulse,
                              width + pulse, ORANGE);
            }
        }
        if (snapshot.buildingPreview) {
            const auto& preview = *snapshot.buildingPreview;
            const float x = static_cast<float>(preview.gridPosition.x);
            const float z = static_cast<float>(preview.gridPosition.z);
            const Color color =
                preview.placement.valid() ? Color{67, 214, 112, 110} : Color{224, 67, 67, 110};
            constexpr float QuarterTurn = PI * 0.5F;
            const float yaw =
                static_cast<float>(preview.rotation) * QuarterTurn;
            WorldMaterialState previewMaterial{};
            previewMaterial.baseColor = {
                static_cast<float>(color.r) / 255.0F,
                static_cast<float>(color.g) / 255.0F,
                static_cast<float>(color.b) / 255.0F,
                static_cast<float>(color.a) / 255.0F,
            };
            previewMaterial.bakedAo = 0.85F;
            renderer_->beginWorldShader(lighting);
            renderer_->setWorldMaterial(previewMaterial);

            if (preview.type == BuildingType::Core) {
                if (!renderer_->drawCore({x, 0.0F, z}, yaw)) {
                    DrawCube({x, 1.25F, z}, 2.0F, 2.5F, 2.0F,
                             WHITE);
                }
            } else if (preview.type == BuildingType::Turret) {
                if (!renderer_->drawCrossbow({x, 0.0F, z}, yaw)) {
                    DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                             WHITE);
                    DrawCylinder({x, 1.45F, z}, 0.42F, 0.32F,
                                 0.7F, 8, WHITE);
                    DrawCube({x, 1.55F, z - 0.55F}, 0.18F,
                             0.18F, 1.0F, WHITE);
                }
            } else if (preview.type == BuildingType::Cannon) {
                if (!renderer_->drawCannon({x, 0.0F, z}, yaw, 0.0F)) {
                    DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                             WHITE);
                    DrawSphere({x, 1.35F, z}, 0.48F, WHITE);
                    DrawCube({x, 1.45F, z - 0.75F}, 0.28F,
                             0.28F, 1.4F, WHITE);
                }
            } else if (preview.type == BuildingType::GoldMine) {
                DrawCube({x, 0.55F, z}, 1.0F, 1.1F, 1.0F,
                         WHITE);
                DrawCylinder({x, 1.25F, z}, 0.32F, 0.48F, 0.7F,
                             8, WHITE);
                DrawSphere({x, 1.72F, z}, 0.22F, WHITE);
            } else if (preview.type == BuildingType::SlowTrap) {
                DrawCube({x, 0.08F, z}, 1.0F, 0.16F, 1.0F,
                         WHITE);
            } else if (preview.type == BuildingType::Wall) {
                const std::uint8_t connections = wallConnectionMask(
                    snapshot.buildings, preview.gridPosition);
                const auto drawSection =
                    [x, z](float offsetX, float offsetZ, float width,
                           float depth) {
                        DrawCube({x + offsetX, 1.0F, z + offsetZ},
                                 width, 2.0F, depth, WHITE);
                    };
                if (connections == 0U) {
                    drawSection(0.0F, 0.0F, 1.0F, 1.0F);
                } else {
                    drawSection(0.0F, 0.0F, 0.5F, 0.5F);
                    if ((connections & WallConnectionNorth) != 0U) {
                        drawSection(0.0F, -0.35F, 0.5F, 0.7F);
                    }
                    if ((connections & WallConnectionEast) != 0U) {
                        drawSection(0.35F, 0.0F, 0.7F, 0.5F);
                    }
                    if ((connections & WallConnectionSouth) != 0U) {
                        drawSection(0.0F, 0.35F, 0.5F, 0.7F);
                    }
                    if ((connections & WallConnectionWest) != 0U) {
                        drawSection(-0.35F, 0.0F, 0.7F, 0.5F);
                    }
                }
            } else if ((preview.rotation % 2U) == 0U) {
                DrawCube({x - 0.38F, 1.0F, z}, 0.22F, 2.0F,
                         1.0F, WHITE);
                DrawCube({x + 0.38F, 1.0F, z}, 0.22F, 2.0F,
                         1.0F, WHITE);
                DrawCube({x, 1.0F, z}, 0.55F, 1.7F, 0.18F,
                         WHITE);
            } else {
                DrawCube({x, 1.0F, z - 0.38F}, 1.0F, 2.0F,
                         0.22F, WHITE);
                DrawCube({x, 1.0F, z + 0.38F}, 1.0F, 2.0F,
                         0.22F, WHITE);
                DrawCube({x, 1.0F, z}, 0.18F, 1.7F, 0.55F,
                         WHITE);
            }
            renderer_->endWorldShader();

        }
        if (showColliders_) {
            for (const auto& collider : snapshot.collisionBoxes) {
                const float width = static_cast<float>(collider.maxX - collider.minX);
                const float depth = static_cast<float>(collider.maxZ - collider.minZ);
                const Vector3 center{
                    static_cast<float>((collider.minX + collider.maxX) * 0.5),
                    1.0F,
                    static_cast<float>((collider.minZ + collider.maxZ) * 0.5),
                };
                DrawCubeWires(center, width, 2.0F, depth, MAGENTA);
            }
        }
        if (showFlowField_) {
            for (const auto& sample : snapshot.flowDebugVectors) {
                const Vector3 start{
                    static_cast<float>(sample.position.x),
                    static_cast<float>(sample.position.y),
                    static_cast<float>(sample.position.z),
                };
                if (sample.blocked) {
                    DrawCubeWires(start, 0.45F, 0.08F, 0.45F, RED);
                    continue;
                }
                const Vector3 end{
                    start.x + static_cast<float>(sample.direction.x) * 0.75F,
                    start.y,
                    start.z + static_cast<float>(sample.direction.z) * 0.75F,
                };
                const Color color =
                    sample.terrainCost >= FlowField::WallTraversalCost
                        ? ORANGE
                        : (sample.terrainCost > 1.0 ? YELLOW : LIME);
                DrawLine3D(start, end, color);
            }
        }
        if (showSpatialHash_) {
            std::array<GridPosition, EnemySystem::MaxEnemies> occupiedCells{};
            std::size_t occupiedCount = 0;
            for (const auto& enemy : snapshot.enemies) {
                if (!enemy.active) {
                    continue;
                }
                const GridPosition cell{
                    static_cast<int>(std::floor(
                        (enemy.position.x - SpatialHash::MinimumCoordinate) /
                        SpatialHash::CellSize)),
                    static_cast<int>(std::floor(
                        (enemy.position.z - SpatialHash::MinimumCoordinate) /
                        SpatialHash::CellSize)),
                };
                const bool exists =
                    std::find(occupiedCells.begin(),
                              occupiedCells.begin() +
                                  static_cast<std::ptrdiff_t>(occupiedCount),
                              cell) !=
                    occupiedCells.begin() +
                        static_cast<std::ptrdiff_t>(occupiedCount);
                if (!exists && occupiedCount < occupiedCells.size()) {
                    occupiedCells[occupiedCount++] = cell;
                }
            }
            for (std::size_t index = 0; index < occupiedCount; ++index) {
                const float x = static_cast<float>(
                    SpatialHash::MinimumCoordinate +
                    (static_cast<double>(occupiedCells[index].x) + 0.5) *
                        SpatialHash::CellSize);
                const float z = static_cast<float>(
                    SpatialHash::MinimumCoordinate +
                    (static_cast<double>(occupiedCells[index].z) + 0.5) *
                        SpatialHash::CellSize);
                DrawCubeWires({x, 0.05F, z}, static_cast<float>(SpatialHash::CellSize),
                              0.1F, static_cast<float>(SpatialHash::CellSize),
                              PURPLE);
            }
        }
        if (renderer_->settings().particles) {
            for (const auto& effect : effects_) {
                const float progress =
                    static_cast<float>(1.0 - effect.remaining / effect.duration);
                const Vector3 origin{
                    static_cast<float>(effect.position.x),
                    static_cast<float>(effect.position.y),
                    static_cast<float>(effect.position.z),
                };
                if (effect.type == PresentationEffectType::Hit) {
                    DrawSphere(origin, 0.18F * (1.0F - progress),
                               {255, 220, 120, 255});
                } else if (effect.type == PresentationEffectType::Explosion) {
                    DrawSphereWires(origin, 0.3F + progress * 4.5F, 10, 10,
                                    {255, 132, 48, 255});
                } else if (effect.type == PresentationEffectType::RamImpact) {
                    DrawSphereWires(origin, 0.4F + progress * 2.8F, 8, 8,
                                    {255, 72, 45, 255});
                    DrawSphereWires(origin, 0.2F + progress * 1.7F, 8, 8,
                                    ORANGE);
                } else {
                    const Color color =
                        effect.type == PresentationEffectType::ResourceBurst
                            ? Color{184, 145, 82, 255}
                            : Color{125, 112, 101, 255};
                    for (int particle = 0; particle < 6; ++particle) {
                        const float angle =
                            static_cast<float>(particle) * 1.04719755F;
                        const float distance = progress * 1.4F;
                        const Vector3 particlePosition{
                            origin.x + std::cos(angle) * distance,
                            origin.y + 0.25F +
                                progress * (1.0F - progress) * 2.0F,
                            origin.z + std::sin(angle) * distance,
                        };
                        DrawCube(particlePosition, 0.12F, 0.12F, 0.12F, color);
                    }
                }
            }
        }
        EndMode3D();
        renderer_->endWorldPass();

        if (playerDamageFlashRemaining_ > 0.0) {
            const auto alpha = static_cast<unsigned char>(
                90.0 * playerDamageFlashRemaining_ / 0.18);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          {190, 24, 24, alpha});
        }

        if (!hideHud_) {
        ui_.drawPanel({12.0F, 12.0F, 360.0F, 238.0F}, 232);
        ui_.drawInsetPanel(
            {20.0F, 72.0F, 344.0F, 54.0F}, 220);
        ui_.drawPanel(
            {12.0F, static_cast<float>(GetScreenHeight()) - 166.0F,
             static_cast<float>(GetScreenWidth()) - 24.0F, 154.0F},
            220);
        if (snapshot.state == RunState::BuildPhase ||
            snapshot.state == RunState::Sunset ||
            snapshot.state == RunState::Wave ||
            snapshot.state == RunState::WaveComplete) {
            ui_.drawPanel(
                {static_cast<float>(GetScreenWidth()) * 0.5F - 300.0F,
                 10.0F, 600.0F, 76.0F},
                226);
        }
        const std::string tickText = "Simulation tick: " + std::to_string(snapshot.tick);
        DrawText(tickText.c_str(), 24, 24, 20, RAYWHITE);
        DrawText("Milestone: One Night", 24, 52, 20, {245, 184, 76, 255});
        const std::string resourcesText = "Wood: " + std::to_string(snapshot.wood) +
                                          "   Stone: " + std::to_string(snapshot.stone) +
                                          "   Gold: " + std::to_string(snapshot.gold);
        DrawText(resourcesText.c_str(), 24, 80, 22, RAYWHITE);
        const std::string playerHealthText =
            "Health: " + std::to_string(static_cast<int>(snapshot.playerHealth)) + " / " +
            std::to_string(static_cast<int>(snapshot.playerMaxHealth));
        const double healthFraction = snapshot.playerHealth / snapshot.playerMaxHealth;
        DrawText(playerHealthText.c_str(), 24, 108, 22,
                 healthFraction > 0.3 ? Color{105, 220, 125, 255}
                                      : Color{235, 92, 72, 255});
        ui_.drawProgressBar(
            {24.0F, 134.0F, 324.0F, 18.0F},
            static_cast<float>(healthFraction),
            healthFraction > 0.3 ? UiBarColor::Green
                                 : UiBarColor::Red);
        if (snapshot.unlimitedResources) {
            DrawText("UNLIMITED RESOURCES", 24, 158, 18,
                     {88, 220, 130, 255});
        }
        if (snapshot.coreMaxHealth > 0.0) {
            const std::string coreText =
                "Core L" + std::to_string(snapshot.coreLevel) + ": " +
                std::to_string(static_cast<int>(snapshot.coreHealth)) + " / " +
                std::to_string(static_cast<int>(snapshot.coreMaxHealth));
            const int coreTextY =
                snapshot.unlimitedResources ? 180 : 158;
            DrawText(coreText.c_str(), 24, coreTextY, 20,
                     {245, 184, 76, 255});
            ui_.drawProgressBar(
                {24.0F, static_cast<float>(coreTextY + 26), 324.0F,
                 18.0F},
                static_cast<float>(snapshot.coreHealth /
                                   snapshot.coreMaxHealth),
                UiBarColor::Yellow);
        }
        if (snapshot.state == RunState::BuildPhase) {
            const std::string phaseText = "Wave " + std::to_string(snapshot.wave + 1) +
                                          " in: " +
                                          std::to_string(
                                              static_cast<int>(snapshot.phaseTimeRemaining) + 1) +
                                          "   N: start early";
            drawCentered(phaseText.c_str(), 24, 24, {245, 184, 76, 255});
        } else if (snapshot.state == RunState::Sunset) {
            std::string sunsetText =
                "SUNSET   Wave " + std::to_string(snapshot.wave + 1) + " in: " +
                std::to_string(static_cast<int>(snapshot.phaseTimeRemaining) + 1);
            if (snapshot.upcomingAttackDirection) {
                sunsetText += "   ATTACK: ";
                sunsetText += attackDirectionName(*snapshot.upcomingAttackDirection);
            }
            drawCentered(sunsetText.c_str(), 24, 24, {255, 146, 79, 255});
        } else if (snapshot.state == RunState::Wave) {
            std::string waveText = "WAVE " + std::to_string(snapshot.wave) +
                                   " / 6   Enemies: " +
                                   std::to_string(snapshot.activeEnemyCount) +
                                   "   Incoming: " +
                                   std::to_string(snapshot.pendingEnemyCount);
            const bool bossCharging =
                std::any_of(snapshot.enemies.begin(), snapshot.enemies.end(),
                            [](const EnemyInstance& enemy) {
                                return enemy.active &&
                                       enemy.state == EnemyState::BossRamWindup;
                            });
            if (bossCharging) {
                waveText += "   BOSS RAM INCOMING";
            }
            drawCentered(waveText.c_str(), 24, 24, {235, 92, 72, 255});
        } else if (snapshot.state == RunState::WaveComplete) {
            const std::string completeText =
                "DAWN   +" + std::to_string(snapshot.waveCompletionReward) +
                " Gold   New day in: " +
                std::to_string(static_cast<int>(snapshot.phaseTimeRemaining) + 1);
            drawCentered(completeText.c_str(), 24, 24, {255, 194, 92, 255});
        }
        const std::string objectiveText = tutorialText(snapshot);
        if (!objectiveText.empty()) {
            drawCentered(objectiveText.c_str(), 56, 20, {255, 224, 146, 255});
        }
        std::string weaponText =
            snapshot.selectedWeapon == PlayerWeapon::Pickaxe
                ? "Weapon: Pickaxe   Rifle L" + std::to_string(snapshot.rifleLevel)
                : "Weapon: Rifle L" + std::to_string(snapshot.rifleLevel) + "   Ammo: " +
                      std::to_string(snapshot.rifleAmmunition) + " / " +
                      std::to_string(snapshot.rifleMagazineSize);
        if (snapshot.rifleReloading) {
            weaponText += "   RELOADING";
        }
        if (snapshot.rifleLevel < 3) {
            weaponText +=
                "   V Upgrade G:" + std::to_string(snapshot.rifleUpgradeGoldCost);
        }
        weaponText += "   Bombs: " + std::to_string(snapshot.bombsRemaining);
        DrawText(weaponText.c_str(), 24, GetScreenHeight() - 88, 18, {245, 184, 76, 255});
        DrawText("1 Core 2 Wall 3 Turret 4 Mine 5 Cannon 6 Trap 7 Gate U Upgrade O Unlimited", 24,
                 GetScreenHeight() - 64, 18, RAYWHITE);
        DrawText("LMB Use/Place  F Repair  X Sell  E Gate  Q Weapon  V Upgrade Rifle  G Bomb", 24,
                 GetScreenHeight() - 40, 18, LIGHTGRAY);
        DrawLine(GetScreenWidth() / 2 - 7, GetScreenHeight() / 2, GetScreenWidth() / 2 + 7,
                 GetScreenHeight() / 2, RAYWHITE);
        DrawLine(GetScreenWidth() / 2, GetScreenHeight() / 2 - 7, GetScreenWidth() / 2,
                 GetScreenHeight() / 2 + 7, RAYWHITE);
        const float indicatorRadius =
            static_cast<float>(std::min(GetScreenWidth(), GetScreenHeight())) * 0.31F;
        const Vector2 screenCenter{
            static_cast<float>(GetScreenWidth()) * 0.5F,
            static_cast<float>(GetScreenHeight()) * 0.5F,
        };
        for (const auto& indicator : damageIndicators_) {
            const float outwardX =
                static_cast<float>(std::sin(indicator.relativeAngle));
            const float outwardY =
                static_cast<float>(-std::cos(indicator.relativeAngle));
            const Vector2 center{
                screenCenter.x + outwardX * indicatorRadius,
                screenCenter.y + outwardY * indicatorRadius,
            };
            const Vector2 perpendicular{-outwardY, outwardX};
            const Vector2 tip{center.x + outwardX * 13.0F,
                              center.y + outwardY * 13.0F};
            const Vector2 baseLeft{
                center.x - outwardX * 9.0F + perpendicular.x * 9.0F,
                center.y - outwardY * 9.0F + perpendicular.y * 9.0F,
            };
            const Vector2 baseRight{
                center.x - outwardX * 9.0F - perpendicular.x * 9.0F,
                center.y - outwardY * 9.0F - perpendicular.y * 9.0F,
            };
            const double fade = indicator.remaining / indicator.duration;
            const auto alpha =
                static_cast<unsigned char>(220.0 * std::clamp(fade, 0.0, 1.0));
            const Color color = indicator.severe ? Color{255, 92, 42, alpha}
                                                 : Color{235, 62, 62, alpha};
            DrawTriangle(tip, baseLeft, baseRight, color);
        }
        if (statusMessageRemaining_ > 0.0 && !statusMessage_.empty()) {
            drawCentered(statusMessage_.c_str(), GetScreenHeight() / 2 + 78, 20,
                         {255, 194, 92, 255});
        }
        if (snapshot.buildingPreview) {
            const auto& preview = *snapshot.buildingPreview;
            const ResourceCost cost = preview.placement.cost;
            const std::string buildText =
                std::string(buildingName(preview.type)) + "  W:" + std::to_string(cost.wood) +
                " S:" + std::to_string(cost.stone) + " G:" + std::to_string(cost.gold);
            drawCentered(buildText.c_str(), GetScreenHeight() / 2 + 24, 20, RAYWHITE);
            drawCentered(placementMessage(preview.placement.error), GetScreenHeight() / 2 + 50, 18,
                         preview.placement.valid() ? GREEN : RED);
        } else if (snapshot.aimedBuilding) {
            const auto aimed = std::find_if(
                snapshot.buildings.begin(), snapshot.buildings.end(),
                [&snapshot](const BuildingInstance& building) {
                    return building.id == *snapshot.aimedBuilding;
                });
            if (aimed != snapshot.buildings.end()) {
                std::string actionText =
                    std::string(buildingName(aimed->type)) + " L" +
                    std::to_string(aimed->level) + "  HP " +
                    std::to_string(static_cast<int>(aimed->health)) + "/" +
                    std::to_string(static_cast<int>(aimed->maxHealth));
                if (snapshot.aimedBuildingUpgradeCost) {
                    const ResourceCost upgradeCost = *snapshot.aimedBuildingUpgradeCost;
                    actionText += "  U Upgrade W:" + std::to_string(upgradeCost.wood) +
                                  " S:" + std::to_string(upgradeCost.stone) +
                                  " G:" + std::to_string(upgradeCost.gold);
                }
                if (aimed->type == BuildingType::Core) {
                    actionText += "  F Repair";
                } else {
                    actionText += "  F Repair  X Sell";
                    if (aimed->type == BuildingType::Gate) {
                        actionText += "  E Open/Close";
                    }
                }
                drawCentered(actionText.c_str(), GetScreenHeight() / 2 + 24, 18,
                             {245, 184, 76, 255});
            }
        } else if (snapshot.aimedEnemy) {
            drawCentered("Attack", GetScreenHeight() / 2 + 24, 18, ORANGE);
        } else if (snapshot.aimedResource) {
            drawCentered("Mine", GetScreenHeight() / 2 + 24, 18, YELLOW);
        }
        const std::string debugText =
            "DEBUG  I Invulnerability:" +
            std::string(snapshot.playerInvulnerable ? "ON" : "OFF") +
            "  T Slow:" + (slowMotion_ ? "ON" : "OFF") +
            "  C Colliders:" + (showColliders_ ? "ON" : "OFF") +
            "  H Flow:" + (showFlowField_ ? "ON" : "OFF") +
            "  L Hash:" + (showSpatialHash_ ? "ON" : "OFF") +
            "  Z/B Spawn:" + enemyName(debugSpawnType_) +
            "  M Damage Core  J Hide HUD  F2 Graphics";
        DrawText(debugText.c_str(), 24, GetScreenHeight() - 112, 16,
                 {199, 154, 235, 255});
        const std::string timingText =
            "Simulation: " + std::to_string(simulationTickMilliseconds_) +
            " ms   Peak: " + std::to_string(peakSimulationTickMilliseconds_) +
            " ms   Budget: 5.0 ms";
        DrawText(timingText.c_str(), 24, GetScreenHeight() - 132, 16,
                 {199, 154, 235, 255});
        const std::string environmentText =
            "Environment: " + std::string(environment_.nearestProfileName()) +
            "  Time:" + std::to_string(environment_.timeOfDay()) +
            "  Y Freeze:" + (environment_.frozen() ? "ON" : "OFF") +
            "  Mode:" +
            (environment_.manualOverride() ? "MANUAL" : "AUTO") +
            "  [/] Time  ' Profile  \\ Auto";
        DrawText(environmentText.c_str(), 24, GetScreenHeight() - 152, 16,
                 {199, 154, 235, 255});
        } else {
            DrawText("HUD HIDDEN  [J]", 24, 24, 18, {199, 154, 235, 255});
        }

        if (snapshot.state == RunState::Paused) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 150});
            drawCentered("PAUSED", GetScreenHeight() / 2 - 24, 48, RAYWHITE);
        } else if (snapshot.state == RunState::Victory) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 170});
            drawCentered("VICTORY", GetScreenHeight() / 2 - 48, 56, {88, 220, 130, 255});
            drawCentered("R: restart", GetScreenHeight() / 2 + 24, 24, RAYWHITE);
        } else if (snapshot.state == RunState::Defeat) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 170});
            drawCentered("CORE DESTROYED", GetScreenHeight() / 2 - 48, 48,
                         {235, 92, 72, 255});
            drawCentered("R: restart", GetScreenHeight() / 2 + 24, 24, RAYWHITE);
        }
    }

    if (!hideHud_) {
        DrawFPS(GetScreenWidth() - 100, 20);
    }
    renderer_->endFrame();
}

} // namespace ian
