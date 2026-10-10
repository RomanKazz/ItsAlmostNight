#include "app/App.hpp"
#include "app/ActionModeEquipment.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "buildings/BuildingHotbarLayout.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "buildings/RampPlacementDirection.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ian {
using namespace app_detail;

namespace {
bool keyPressed(const ControlSettings& settings,
                ControlAction action) {
    const int key = controlKey(settings, action);
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    }
    return key != KEY_NULL &&
           IsKeyPressed(static_cast<KeyboardKey>(key));
}

}

void App::resetMainMenuInput() {
    pendingStartFromUi_ = false;
    pendingContinueFromUi_ = false;
    classSelectionVisible_ = false;
    classCollectionOnly_ = false;
    sandboxClassSelection_ = false;
    mainMenuPage_ = MainMenuPage::None;
    mainMenuInputBarrier_.blockUntilRelease();
}

void App::processInput() {
    input_.jumpHeld = false;
    renderer_->processInput();
    // Keep only the input flags stable across commands that invalidate the
    // simulation snapshot, without copying the building card vectors.
    const auto& initialSnapshot = simulation_.snapshot();
    const RunState runState = initialSnapshot.state;
    const bool sandboxMode = initialSnapshot.sandboxMode;
    const bool upgradeChoicePending = initialSnapshot.runUpgradeChoicePending;
    const std::size_t upgradeChoiceCount = initialSnapshot.runUpgradeChoiceCount;
    const int rerollTokens = initialSnapshot.runUpgradeRerollTokens;
    const bool graphicsPanelVisible =
        renderer_->graphicsPanelVisible();
    const int restartKey = controlKey(userSettings_.controls, ControlAction::Restart);
    const bool restartHeld = restartHold_.update(
        restartKey != KEY_NULL && IsKeyDown(restartKey),
        runState != RunState::MainMenu && IsWindowFocused() &&
            !graphicsPanelVisible && !pendingControlRebind_ &&
            !upgradeChoicePending &&
            !enemySpawnMenuVisible_ && !itemGrantMenuVisible_ &&
            !sandboxCardMenuVisible_ && !coreDefenseMenuVisible_ &&
            !objectiveDebugMenuVisible_,
        static_cast<double>(GetFrameTime()));
    if (!coreDefenseMenuVisible_ && coreMenuPointerActive_) {
        coreMenuPointerActive_ = false;
        if (acceptsGameplayInput(runState) &&
            !graphicsPanelVisible && !enemySpawnMenuVisible_ &&
            !itemGrantMenuVisible_ && !sandboxCardMenuVisible_) {
            DisableCursor();
        }
    }
    const bool primaryMouseDown =
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (primaryMouseDown &&
        acceptsGameplayInput(runState) &&
        !graphicsPanelVisible &&
        !enemySpawnMenuVisible_ &&
        !itemGrantMenuVisible_ &&
        !sandboxCardMenuVisible_ &&
        !coreDefenseMenuVisible_ &&
        !coreMenuPointerActive_ &&
        !pendingControlRebind_) {
        primaryAttackHoldSeconds_ +=
            static_cast<double>(GetFrameTime());
    } else {
        primaryAttackHoldSeconds_ = 0.0;
    }
    if (pendingControlRebind_) {
        const int pressedKey = GetKeyPressed();
        if (pressedKey == KEY_ESCAPE) {
            pendingControlRebind_.reset();
        } else if (pressedKey != KEY_NULL) {
            setControlKey(
                userSettings_.controls, *pendingControlRebind_,
                pressedKey);
            pendingControlRebind_.reset();
        }
        suspendGameplayInput();
        return;
    }
    const bool shopShortcut = IsKeyPressed(KEY_P) &&
        (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    const bool shopVisible = simulation_.snapshot().runShop.pending && !upgradeChoicePending && runState != RunState::Paused;
    if (shopVisible) {
        if (!runShopWasVisible_) {
            runShopWasVisible_ = true;
            runShopInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
            runShopKeyboardFocus_.reset();
            coreDefenseMenuVisible_ = false; coreDefenseMenuClosing_ = false;
            enemySpawnMenuVisible_ = false; itemGrantMenuVisible_ = false; sandboxCardMenuVisible_ = false;
            EnableCursor();
        }
        const bool released = !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_ENTER) &&
            !IsKeyDown(KEY_SPACE) && !IsKeyDown(KEY_R) && !IsKeyDown(KEY_ONE) &&
            !IsKeyDown(KEY_TWO) && !IsKeyDown(KEY_THREE) && !IsKeyDown(KEY_FOUR);
        if (runShopInputDelayRemaining_ > 0)
            runShopInputDelayRemaining_ = advanceRunUpgradeInputDelay(runShopInputDelayRemaining_,released,GetFrameTime());
        if (runShopInputDelayRemaining_ <= 0) {
            const auto l = runShopLayout();
            const auto pointer = GetMousePosition();
            const auto delta = GetMouseDelta();
            if (delta.x != 0 || delta.y != 0) runShopKeyboardFocus_.reset();
            const bool reverse = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            const int step = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP) || (IsKeyPressed(KEY_TAB) && reverse) ? -1 :
                IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB) ? 1 : 0;
            if (step) {
                const int old = runShopKeyboardFocus_.value_or(step > 0 ? -1 : 0);
                runShopKeyboardFocus_ = (old + step + 10) % 10;
                audio_.playUiSelect();
            }
            std::optional<int> action;
            if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && runShopKeyboardFocus_) action = runShopKeyboardFocus_;
            if (IsKeyPressed(KEY_R)) action = 8;
            if (IsKeyPressed(KEY_ESCAPE) || shopShortcut) action = 9;
            for (std::size_t i = 0; i < RunShopSlotCount; ++i) {
                if (IsKeyPressed(static_cast<int>(KEY_ONE) + static_cast<int>(i))) action = static_cast<int>(i);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(pointer,l.buy[i])) action = static_cast<int>(i);
                    if (CheckCollisionPointRec(pointer,l.locks[i])) action = static_cast<int>(i+4);
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(pointer,l.reroll)) action = 8;
                if (CheckCollisionPointRec(pointer,l.proceed)) action = 9;
            }
            if (action) {
                const bool success = *action < 4 ? simulation_.buyRunShopOffer(static_cast<std::size_t>(*action)) : *action < 8 ?
                    simulation_.toggleRunShopLock(static_cast<std::size_t>(*action - 4)) : *action == 8 ? simulation_.rerollRunShop() : simulation_.closeRunShop();
                if (success) audio_.playUiConfirm(); else audio_.playUiError();
                if (*action == 8) runShopInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
                if (*action == 9 && success) {
                    runShopWasVisible_ = false;
                    runShopKeyboardFocus_.reset();
                    if (runState != RunState::StageClear) DisableCursor();
                }
            }
        }
        suspendGameplayInput();
        return;
    }
    if (runShopWasVisible_ && !upgradeChoicePending) {
        runShopWasVisible_ = false;
        runShopKeyboardFocus_.reset();
        if (runState != RunState::MainMenu && runState != RunState::Paused && runState != RunState::StageClear && !runFinished) DisableCursor();
    }
    if (!upgradeChoicePending && (shopShortcut || keyPressed(userSettings_.controls,ControlAction::RunShop))) {
        if (shopShortcut && runState == RunState::Paused) simulation_.togglePause();
        if (simulation_.openRunShop(shopShortcut)) {
            EnableCursor();
            audio_.playUiConfirm();
            suspendGameplayInput();
            return;
        }
        // Ctrl+P belongs to the shop; never let it trigger ordinary P pause.
        if (shopShortcut) {
            suspendGameplayInput();
            return;
        }
    }
    if (runState == RunState::StageClear &&
        !upgradeChoicePending && !restartHeld) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_F)) {
            if (simulation_.enterFinalNight()) {
                audio_.playUiConfirm();
            }
        } else if (IsKeyPressed(KEY_B)) {
            if (simulation_.bankStageClear()) {
                audio_.playUiConfirm();
            }
        }
        suspendGameplayInput();
        return;
    }
    if (upgradeChoicePending) {
        if (!runUpgradeChoiceWasVisible_) {
            renderer_->setGraphicsPanelVisible(false);
            enemySpawnMenuVisible_ = false;
            itemGrantMenuVisible_ = false;
            sandboxCardMenuVisible_ = false;
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuEntranceSeconds_ = 0.0;
            cameraBobSpeed_ = 0.0;
            cameraBobAmount_ = 0.0;
            cameraLookYawLag_ = 0.0;
            cameraLookPitchLag_ = 0.0;
            cameraStrafeLean_ = 0.0;
            cameraImpulseOffset_ = {};
            cameraShakeRemaining_ = 0.0;
            explosionShakeCooldownRemaining_ = 0.0;
            landingResponseRemaining_ = 0.0;
            EnableCursor();
            runUpgradeChoiceWasVisible_ = true;
            runUpgradeChoiceInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
            runUpgradeChoiceInputArmed_ = false;
            runUpgradeOverlayEntranceSeconds_ = 0.0;
            runUpgradeOverlayExitSeconds_ = 0.0;
            runUpgradePendingChoice_.reset();
            runUpgradeCardHoverAmounts_.fill(0.0F);
        }
        if (runUpgradePendingChoice_) {
            runUpgradeChoiceInputArmed_ = false;
            runUpgradeOverlayExitSeconds_ = std::min(
                RunUpgradeOverlayExitDuration,
                runUpgradeOverlayExitSeconds_ +
                    static_cast<double>(GetFrameTime()));
            if (runUpgradeOverlayExitSeconds_ >=
                RunUpgradeOverlayExitDuration) {
                const std::size_t selected =
                    *runUpgradePendingChoice_;
                runUpgradePendingChoice_.reset();
                runUpgradeOverlayExitSeconds_ = 0.0;
                if (simulation_.selectRunUpgrade(selected)) {
                    if (!simulation_.snapshot().runUpgradeChoicePending) {
                        runUpgradeChoiceWasVisible_ = false;
                        runUpgradeChoiceInputArmed_ = false;
                        runUpgradeChoiceInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
                        if (!simulation_.snapshot().runShop.pending) DisableCursor();
                    } else {
                        runUpgradeOverlayEntranceSeconds_ = 0.0;
                        runUpgradeCardHoverAmounts_.fill(0.0F);
                        runUpgradeChoiceInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
                        runUpgradeChoiceInputArmed_ = false;
                    }
                } else {
                    runUpgradeChoiceInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
                }
            }
        }
        const bool choiceInputsReleased =
            !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            !IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
            !IsKeyDown(KEY_R) && !IsKeyDown(KEY_ONE) &&
            !IsKeyDown(KEY_TWO) && !IsKeyDown(KEY_THREE) &&
            !IsKeyDown(KEY_FOUR) && !IsKeyDown(KEY_FIVE);
        if (!runUpgradePendingChoice_ && !runUpgradeChoiceInputArmed_) {
            runUpgradeChoiceInputDelayRemaining_ = advanceRunUpgradeInputDelay(
                runUpgradeChoiceInputDelayRemaining_, choiceInputsReleased,
                static_cast<double>(GetFrameTime()));
            if (choiceInputsReleased && runUpgradeChoiceInputDelayRemaining_ <= 0.0)
                runUpgradeChoiceInputArmed_ = true;
        }
        std::optional<std::size_t> choice;
        if (runUpgradeChoiceInputArmed_ && IsKeyPressed(KEY_R) &&
            simulation_.rerollRunUpgrades()) {
            runUpgradeOverlayEntranceSeconds_ = 0.0;
            runUpgradeOverlayExitSeconds_ = 0.0;
            runUpgradePendingChoice_.reset();
            runUpgradeCardHoverAmounts_.fill(0.0F);
            runUpgradeChoiceInputDelayRemaining_ = RunUpgradeChoiceInputQuietDuration;
            runUpgradeChoiceInputArmed_ = false;
            audio_.playUiConfirm();
        }
        if (runUpgradeChoiceInputArmed_ &&
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            const auto hovered = hoveredRunUpgradeChoice(
                simulation_.snapshot(), GetMousePosition());
            if (hovered && simulation_.lockRunUpgrade(*hovered)) {
                audio_.playUiConfirm();
            }
        }
        for (std::size_t index = 0;
             index < upgradeChoiceCount; ++index) {
            if (runUpgradeChoiceInputArmed_ &&
                IsKeyPressed(static_cast<int>(KEY_ONE) +
                             static_cast<int>(index))) {
                choice = index;
                break;
            }
        }
        if (runUpgradeChoiceInputArmed_ && !choice &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            choice = hoveredRunUpgradeChoice(
                simulation_.snapshot(), GetMousePosition());
        }
        if (choice && !runUpgradePendingChoice_) {
            runUpgradePendingChoice_ = *choice;
            runUpgradeOverlayExitSeconds_ = 0.0;
            runUpgradeChoiceInputArmed_ = false;
            audio_.playUiConfirm();
        }
        suspendGameplayInput();
        return;
    }
    if (runUpgradeChoiceWasVisible_) {
        runUpgradeChoiceWasVisible_ = false;
        runUpgradeChoiceInputArmed_ = false;
        runUpgradeOverlayEntranceSeconds_ = 0.0;
        runUpgradeOverlayExitSeconds_ = 0.0;
        runUpgradePendingChoice_.reset();
        runUpgradeCardHoverAmounts_.fill(0.0F);
        if (runState != RunState::Paused &&
            runState != RunState::MainMenu) {
            DisableCursor();
        }
    }
    if (graphicsPanelVisible != graphicsPanelWasVisible_) {
        if (graphicsPanelVisible) {
            EnableCursor();
        } else if (runState != RunState::MainMenu &&
                   runState != RunState::Paused) {
            DisableCursor();
        }
        graphicsPanelWasVisible_ = graphicsPanelVisible;
    }
    if (graphicsPanelVisible) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            renderer_->setGraphicsPanelVisible(false);
            audio_.playUiConfirm();
        }
        suspendGameplayInput();
        return;
    }
    if (pendingResumeFromUi_) {
        pendingResumeFromUi_ = false;
        if (runState == RunState::Paused) {
            simulation_.togglePause();
            fixedStep_.reset();
            DisableCursor();
            audio_.playUiConfirm();
        }
    }
    if (pendingRestartFromUi_) {
        pendingRestartFromUi_ = false;
        automaticRunRestartPending_ = true;
        audio_.playUiConfirm();
    }
    if (pendingReturnToMenuFromUi_) {
        pendingReturnToMenuFromUi_ = false;
        if (!sandboxMode && !saveSuspendedRun()) {
            audio_.playUiError();
            return;
        }
        simulation_.returnToMainMenu();
        resetMainMenuInput();
        fixedStep_.reset();
        EnableCursor();
        audio_.playUiConfirm();
        return;
    }
    const bool shiftHeld =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (runState != RunState::MainMenu && shiftHeld &&
        IsKeyPressed(KEY_F5)) {
        sandboxCardMenuVisible_ = !sandboxCardMenuVisible_;
        itemGrantMenuVisible_ = false;
        if (sandboxCardMenuVisible_) {
            enemySpawnMenuVisible_ = false;
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuEntranceSeconds_ = 0.0;
            EnableCursor();
        } else if (runState != RunState::Paused) {
            DisableCursor();
        }
        audio_.playUiConfirm();
    }
    if (runState != RunState::MainMenu &&
        !IsKeyDown(KEY_LEFT_SHIFT) &&
        !IsKeyDown(KEY_RIGHT_SHIFT) &&
        IsKeyPressed(KEY_F5)) {
        if (sandboxMode) {
            sandboxCardMenuVisible_ = !sandboxCardMenuVisible_;
            itemGrantMenuVisible_ = false;
        } else {
            itemGrantMenuVisible_ = !itemGrantMenuVisible_;
            sandboxCardMenuVisible_ = false;
        }
        if (itemGrantMenuVisible_ || sandboxCardMenuVisible_) {
            enemySpawnMenuVisible_ = false;
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuEntranceSeconds_ = 0.0;
            EnableCursor();
        } else if (runState != RunState::Paused) {
            DisableCursor();
        }
        audio_.playUiConfirm();
    }
    if (sandboxCardMenuVisible_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            sandboxCardMenuVisible_ = false;
            if (runState != RunState::Paused) {
                DisableCursor();
            }
            audio_.playUiConfirm();
        }
        suspendGameplayInput();
        return;
    }
    if (coreDefenseMenuVisible_) {
        if (!coreMenuPointerActive_) {
            coreMenuPointerActive_ = true;
            EnableCursor();
        }
    }
    if (itemGrantMenuVisible_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            itemGrantMenuVisible_ = false;
            if (runState != RunState::Paused) {
                DisableCursor();
            }
            audio_.playUiConfirm();
        }
        suspendGameplayInput();
        return;
    }
    if (runState != RunState::MainMenu &&
        IsKeyPressed(KEY_B)) {
        enemySpawnMenuVisible_ = !enemySpawnMenuVisible_;
        if (enemySpawnMenuVisible_) {
            itemGrantMenuVisible_ = false;
            sandboxCardMenuVisible_ = false;
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuEntranceSeconds_ = 0.0;
            EnableCursor();
        } else if (runState != RunState::Paused) {
            DisableCursor();
        }
    }
    const bool controlDown =
        IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shiftDown =
        IsKeyDown(KEY_LEFT_SHIFT) ||
        IsKeyDown(KEY_RIGHT_SHIFT);
    if (runState != RunState::MainMenu &&
        controlDown &&
        IsKeyPressed(KEY_F10)) {
        simulation_.setStructuralCollapseEnabled(
            !simulation_.structuralCollapseEnabled());
    }
    if (runState != RunState::MainMenu &&
        controlDown &&
        IsKeyPressed(KEY_F11)) {
        static_cast<void>(
            simulation_.clearModularBuildings());
    }
    if (enemySpawnMenuVisible_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            enemySpawnMenuVisible_ = false;
            if (runState != RunState::Paused) {
                DisableCursor();
            }
            audio_.playUiConfirm();
        }
        suspendGameplayInput();
        return;
    }
    if (runState == RunState::MainMenu &&
        pendingContinueFromUi_) {
        pendingContinueFromUi_ = false;
        if (loadSuspendedRun()) {
            const auto continuedSnapshot = simulation_.snapshot();
            classSelectionVisible_ = false;
            classCollectionOnly_ = false;
            worldRevealOrigin_ = {
                static_cast<float>(continuedSnapshot.playerPosition.x),
                static_cast<float>(continuedSnapshot.playerPosition.z)};
            worldRevealElapsed_ = 1.7;
            DisableCursor();
            audio_.playUiConfirm();
        } else {
            discardSuspendedRun();
            audio_.playUiError();
        }
        return;
    }
    if (runState == RunState::MainMenu &&
        mainMenuPage_ != MainMenuPage::None) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            mainMenuPage_ = MainMenuPage::None;
            audio_.playUiConfirm();
        }
        return;
    }
    if (runState == RunState::MainMenu &&
        classSelectionVisible_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            classSelectionVisible_ = false;
            classCollectionOnly_ = false;
            sandboxClassSelection_ = false;
            pendingStartFromUi_ = false;
            audio_.playUiConfirm();
            return;
        }
        const int direction =
            IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)
                ? -1
                : IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)
                    ? 1
                    : 0;
        if (direction != 0) {
            const auto current = std::ranges::find(
                PlayerClassDefinitions,
                selectedPlayerClass_,
                &PlayerClassDefinition::type);
            const std::size_t currentIndex =
                current == PlayerClassDefinitions.end()
                    ? 0U
                    : static_cast<std::size_t>(
                          std::distance(
                              PlayerClassDefinitions.begin(),
                              current));
            std::size_t nextIndex = currentIndex;
            do {
                nextIndex = static_cast<std::size_t>(
                    (static_cast<int>(nextIndex) + direction +
                     static_cast<int>(PlayerClassDefinitions.size())) %
                    static_cast<int>(PlayerClassDefinitions.size()));
            } while (!sandboxClassSelection_ &&
                !isPlayerClassUnlocked(
                    PlayerClassDefinitions[nextIndex].type,
                    metaProgression_));
            selectedPlayerClass_ = PlayerClassDefinitions[nextIndex].type;
            audio_.playUiConfirm();
        }
    }
    if (runState == RunState::MainMenu &&
        (IsKeyPressed(KEY_ENTER) || pendingStartFromUi_)) {
        if (!classSelectionVisible_) {
            pendingStartFromUi_ = false;
            classSelectionVisible_ = true;
            classCollectionOnly_ = false;
            sandboxClassSelection_ = false;
            audio_.playUiConfirm();
            return;
        }
        if (classCollectionOnly_ ||
            (!sandboxClassSelection_ && !isPlayerClassUnlocked(
                selectedPlayerClass_, metaProgression_))) {
            pendingStartFromUi_ = false;
            audio_.playUiError();
            return;
        }
        pendingStartFromUi_ = false;
        classSelectionVisible_ = false;
        classCollectionOnly_ = false;
        static_cast<void>(simulation_.configureNextRunBonuses(metaRunBonuses(metaProgression_)));
        if (sandboxClassSelection_) {
            simulation_.startSandboxRun(selectedPlayerClass_);
        } else {
            simulation_.startRun(selectedPlayerClass_);
        }
        const auto startedSnapshot = simulation_.snapshot();
        if (!sandboxClassSelection_) {
            discardSuspendedRun();
        }
        sandboxClassSelection_ = false;
        rebuildTerrainGraphics();
        worldRevealOrigin_ = {
            static_cast<float>(
                startedSnapshot.playerPosition.x),
            static_cast<float>(
                startedSnapshot.playerPosition.z),
        };
        worldRevealElapsed_ = 0.0;
        audio_.playUiConfirm();
        resetRunClientState(startedSnapshot);
    }
    if (runState != RunState::Defeat &&
        runState != RunState::Victory &&
        (keyPressed(userSettings_.controls, ControlAction::Pause) ||
         IsKeyPressed(KEY_ESCAPE))) {
        simulation_.togglePause();
        fixedStep_.reset();
        if (simulation_.snapshot().state == RunState::Paused) {
            EnableCursor();
        } else if (simulation_.snapshot().state != RunState::MainMenu) {
            DisableCursor();
        }
    }
    if (runState != RunState::MainMenu &&
        (automaticRunRestartPending_ || restartHeld)) {
        automaticRunRestartPending_ = false;
        static_cast<void>(simulation_.configureNextRunBonuses(metaRunBonuses(metaProgression_)));
        simulation_.restartRun();
        const auto restartedSnapshot = simulation_.snapshot();
        discardSuspendedRun();
        rebuildTerrainGraphics();
        worldRevealOrigin_ = {
            static_cast<float>(
                restartedSnapshot.playerPosition.x),
            static_cast<float>(
                restartedSnapshot.playerPosition.z),
        };
        worldRevealElapsed_ = 0.0;
        resetRunClientState(restartedSnapshot);
    }
    if ((runState == RunState::Defeat ||
         runState == RunState::Victory) &&
        IsKeyPressed(KEY_ESCAPE)) {
        simulation_.returnToMainMenu();
        resetMainMenuInput();
        fixedStep_.reset();
        EnableCursor();
        audio_.playUiConfirm();
        return;
    }
    if (runState != RunState::MainMenu) {
        if (!controlDown &&
            keyPressed(userSettings_.controls,
                       ControlAction::ToggleHud)) {
            hudHidden_ = !hudHidden_;
        }
        if (shiftDown &&
            IsKeyPressed(KEY_F10)) {
            performanceOverlayVisible_ =
                !performanceOverlayVisible_;
        }
        if (shiftDown && IsKeyPressed(KEY_F9)) {
            objectiveDebugMenuVisible_ =
                !objectiveDebugMenuVisible_;
        }
        if (controlDown &&
            IsKeyPressed(KEY_F6)) {
            showTerrainWireframe_ =
                !showTerrainWireframe_;
        }
        if (controlDown &&
            IsKeyPressed(KEY_F7)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed());
            rebuildTerrainGraphics();
        }
        if (controlDown &&
            IsKeyPressed(KEY_F9)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed() +
                0x9e3779b9U);
            rebuildTerrainGraphics();
        }
        if (IsKeyPressed(KEY_T)) {
            slowMotion_ = !slowMotion_;
            fixedStep_.reset();
        }
        if (controlDown &&
            IsKeyPressed(KEY_F8)) {
            showColliders_ = !showColliders_;
        }
        if (controlDown && IsKeyPressed(KEY_F10)) {
            pendingChainLightning_ = true;
        }
        if (controlDown && IsKeyPressed(KEY_H)) {
            showFlowField_ = !showFlowField_;
        }
        if (controlDown && IsKeyPressed(KEY_L)) {
            showSpatialHash_ = !showSpatialHash_;
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

    processGameplayInput(controlDown, primaryMouseDown);
}

} // namespace ian
