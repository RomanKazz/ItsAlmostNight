#pragma once
#include "progression/MetaProgression.hpp"
#include "ui/GameUi.hpp"

namespace ian {
inline constexpr int MetaShopGridColumns = 6;
struct ShopMenuState { MetaShopUpgrade selected{MetaShopUpgrade::SilverGain}; };
struct ShopMenuInput {
    Vector2 pointer{};
    bool click{};
    bool close{};
    bool confirm{};
    int selectionStep{};
};
struct ShopMenuResult { bool close{}; bool purchase{}; bool interaction{}; bool openUnlocks{}; };
[[nodiscard]] Rectangle shopMenuPanelBounds(Vector2 viewport);
[[nodiscard]] ShopMenuResult drawShopMenu(const GameUi&, MetaProgression&, ShopMenuState&,
    Vector2 viewport, const ShopMenuInput& input = {});
} // namespace ian
