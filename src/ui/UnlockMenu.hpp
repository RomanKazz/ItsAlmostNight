#pragma once
#include "ui/UiMenuMotion.hpp"

#include "progression/QuestSystem.hpp"
#include "ui/GameUi.hpp"

#include <string>

namespace ian {

enum class UnlockCategory { Classes, Buildings, Weapons, Upgrades, Items, Meta };

struct UnlockMenuState {
    UiMenuMotion motion;
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
    float deltaTime{1.0F / 60.0F};
    bool reduceEffects{};
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
