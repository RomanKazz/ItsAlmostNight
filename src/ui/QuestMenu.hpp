#pragma once

#include "progression/QuestSystem.hpp"
#include "ui/GameUi.hpp"

#include <optional>
#include <string>

namespace ian {

struct QuestMenuState {
    std::optional<QuestCategory> category;
    bool completedLast{true};
    std::string selectedId;
    float scroll{};
    bool draggingScroll{};
};

struct QuestMenuInput {
    Vector2 pointer{};
    float wheel{};
    bool click{};
    bool pointerDown{};
    bool close{};
    int selectionStep{};
    int categoryStep{};
};

struct QuestMenuResult { bool close{}; bool purchase{}; bool interaction{}; };

[[nodiscard]] Rectangle questMenuPanelBounds(Vector2 viewport);
[[nodiscard]] bool drawQuestSummaryCard(
    const GameUi& ui, const QuestStatus& status, Rectangle bounds, float scale,
    Vector2 pointer, bool click);
[[nodiscard]] QuestMenuResult drawQuestMenu(
    const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
    QuestMenuState& state, Vector2 viewport, const QuestMenuInput& input = {});

} // namespace ian
