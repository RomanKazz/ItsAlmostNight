#pragma once
#include "ui/UiMenuMotion.hpp"

#include "progression/QuestSystem.hpp"
#include "ui/GameUi.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ian {

struct QuestRewardFlight {
    Vector2 start{};
    Vector2 burst{};
    float age{};
    float delay{};
    int silver{};
};

struct QuestMenuState {
    UiMenuMotion motion;
    std::optional<QuestCategory> category;
    bool completedLast{true};
    std::string selectedId;
    float scroll{};
    bool draggingScroll{};
    std::vector<QuestRewardFlight> rewardFlights;
    float counterPulse{};
};

struct QuestMenuInput {
    Vector2 pointer{};
    float wheel{};
    bool click{};
    bool pointerDown{};
    bool close{};
    int selectionStep{};
    int categoryStep{};
    float deltaTime{};
    bool reduceEffects{};
};

struct QuestMenuResult { bool close{}; bool purchase{}; bool rewardClaimed{}; bool interaction{}; };

[[nodiscard]] Rectangle questMenuPanelBounds(Vector2 viewport);
[[nodiscard]] bool drawQuestSummaryCard(
    const GameUi& ui, const QuestStatus& status, Rectangle bounds, float scale,
    Vector2 pointer, bool click);
[[nodiscard]] QuestMenuResult drawQuestMenu(
    const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
    QuestMenuState& state, Vector2 viewport, const QuestMenuInput& input = {});

} // namespace ian
