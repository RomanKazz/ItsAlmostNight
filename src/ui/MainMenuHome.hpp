#pragma once

#include "progression/MetaProgression.hpp"
#include "progression/QuestSystem.hpp"
#include "ui/GameUi.hpp"

#include <span>
#include <string>
#include <array>

namespace ian {

enum class MainMenuHomeAction {
    None, Play, Continue, Unlocks, Quests, Shop, Settings, Credits, Exit,
};

struct MainMenuHomeInput {
    Vector2 pointer{};
    bool click{};
    bool pointerDown{};
    float deltaTime{};
    int tabStep{};
    int horizontalStep{};
    int verticalStep{};
    bool activate{};
};

inline constexpr float MainMenuSelectionSeconds = 0.09F;
inline constexpr std::size_t MainMenuFeaturedQuestCount = 2;

struct MainMenuHomeState {
    bool friendsLeaderboardVisible{};
    bool allQuestsCompleted{};
    std::size_t completedQuestCount{};
    std::size_t totalQuestCount{};
    std::size_t unclaimedQuestRewards{};
    std::size_t selectedButton{};
    Rectangle focusBounds{830, 506, 260, 120};
    Rectangle focusStart{focusBounds};
    float selectionElapsed{MainMenuSelectionSeconds};
    std::array<float, 7> selectionAmounts{1.0F};
    std::array<float, 7> selectionStart{selectionAmounts};
    bool pointerWasDown{};
    MainMenuHomeAction pendingAction{MainMenuHomeAction::None};
    float activationRemaining{};
};
struct MainMenuHomeResult {
    MainMenuHomeAction action{MainMenuHomeAction::None};
    std::string questId;
};

// Figma Frame 19 (1920x1080), with live quest/profile data and a transparent
// background so the player's saved world composition remains visible.
[[nodiscard]] MainMenuHomeResult drawMainMenuHome(
    const GameUi& ui, const MetaProgression& profile,
    std::span<const QuestStatus> featured, bool canContinue,
    MainMenuHomeState& state, Vector2 viewport,
    const MainMenuHomeInput& input = {});

} // namespace ian
