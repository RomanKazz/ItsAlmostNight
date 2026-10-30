#pragma once

#include "progression/QuestSystem.hpp"
#include "ui/GameUi.hpp"

#include <string>

namespace ian {

enum class UnlockCategory { Classes, Buildings, Weapons, Upgrades, Items, Meta };

struct UnlockMenuState {
    UnlockCategory category{UnlockCategory::Classes};
    std::string selectedId;
    float scroll{};
    bool draggingScroll{};
};

struct UnlockMenuInput {
    Vector2 pointer{};
    float wheel{};
    bool click{};
    bool pointerDown{};
    bool close{};
    int selectionStep{};
    int categoryStep{};
};

struct UnlockMenuResult {
    bool close{};
    bool purchase{};
    bool interaction{};
    bool toggled{};
    std::string questId;
    std::string shopUnlockId;
};

[[nodiscard]] Rectangle unlockMenuPanelBounds(Vector2 viewport);
[[nodiscard]] UnlockMenuResult drawUnlockMenu(
    const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
    UnlockMenuState& state, Vector2 viewport, const UnlockMenuInput& input = {});

} // namespace ian
