#pragma once

#include "game/PlayerClass.hpp"
#include "progression/MetaProgression.hpp"
#include "ui/GameUi.hpp"

namespace ian {

struct MenuNavigationInput {
    Vector2 pointer{};
    bool click{};
    int tabStep{};
    int horizontalStep{};
    int verticalStep{};
    bool activate{};
};
[[nodiscard]] MenuNavigationInput readMenuNavigationInput();

struct ClassSelectionState { int focused{}; };
struct ClassSelectionResult { bool start{}; bool back{}; bool interaction{}; };
struct ClassSelectionLayout {
    Rectangle panel{};
    std::array<Rectangle, 8> cards{};
    Rectangle details{};
    std::array<Rectangle, 3> buttons{};
    int columns{2};
    float scale{1};
};
[[nodiscard]] ClassSelectionLayout classSelectionLayout(Vector2 screen);
[[nodiscard]] ClassSelectionResult drawClassSelectionMenu(
    const GameUi& ui, const MetaProgression& profile, PlayerClass& selected,
    bool& sandbox, bool collectionOnly, ClassSelectionState& state,
    Vector2 screen, const MenuNavigationInput& input);

enum class PauseMenuAction { None, Resume, Settings, Restart, MainMenu, Exit };
struct PauseMenuState { int focused{}; };
struct PauseMenuLayout { Rectangle panel{}; std::array<Rectangle, 5> buttons{}; float scale{1}; };
[[nodiscard]] PauseMenuLayout pauseMenuLayout(Vector2 screen);
[[nodiscard]] PauseMenuAction drawPauseMenu(PauseMenuState& state, bool sandbox,
    Vector2 screen, const MenuNavigationInput& input);

struct SimulationSnapshot;
enum class RunSummaryAction { None, Restart, MainMenu };
struct RunSummaryState { int focused{}; };
struct RunSummaryLayout {
    Rectangle panel{};
    Rectangle header{};
    std::array<Rectangle, 4> highlights{};
    std::array<Rectangle, 2> details{};
    std::array<Rectangle, 2> buttons{};
    float scale{1};
};
[[nodiscard]] RunSummaryLayout runSummaryLayout(Vector2 screen);
[[nodiscard]] RunSummaryAction drawRunSummaryMenu(const SimulationSnapshot& snapshot,
    RunSummaryState& state, Vector2 screen, const MenuNavigationInput& input);

} // namespace ian
