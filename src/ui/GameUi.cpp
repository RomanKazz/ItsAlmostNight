#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <algorithm>
#include <string>

namespace ian {
namespace {

constexpr NPatchInfo PanelPatch{
    .source = {0.0F, 0.0F, 100.0F, 100.0F},
    .left = 18,
    .top = 18,
    .right = 18,
    .bottom = 18,
    .layout = NPATCH_NINE_PATCH,
};

constexpr NPatchInfo InsetPatch{
    .source = {0.0F, 0.0F, 93.0F, 94.0F},
    .left = 16,
    .top = 16,
    .right = 16,
    .bottom = 16,
    .layout = NPATCH_NINE_PATCH,
};

Color alphaTint(unsigned char alpha) {
    return {255, 255, 255, alpha};
}

} // namespace

GameUi::~GameUi() {
    shutdown();
}

void GameUi::initialize() {
    shutdown();
    panel_ = loadTexture("assets/ui/panel_brown.png");
    insetPanel_ = loadTexture("assets/ui/panelInset_brown.png");
    button_ = loadTexture("assets/ui/buttonLong_brown.png");
    buttonPressed_ =
        loadTexture("assets/ui/buttonLong_brown_pressed.png");
    barBack_ = loadTexture("assets/ui/barBack_horizontalMid.png");
    barRed_ = loadTexture("assets/ui/barRed_horizontalMid.png");
    barGreen_ = loadTexture("assets/ui/barGreen_horizontalMid.png");
    barYellow_ = loadTexture("assets/ui/barYellow_horizontalMid.png");
    resourceWood_ = loadTexture("assets/ui/resource_wood.png");
    resourceStone_ = loadTexture("assets/ui/resource_stone.png");
    resourceCrystal_ =
        loadTexture("assets/ui/resource_crystal.png");
    initializeUiText();
    initialized_ = true;

    GuiSetFont(uiFont());
    GuiSetStyle(DEFAULT, TEXT_SIZE, 31);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,
                static_cast<int>(0xffead8ffU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,
                static_cast<int>(0xffd58affU));
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x00000000);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,
                static_cast<int>(0xffead8ffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,
                static_cast<int>(0xffd58affU));
}

void GameUi::shutdown() {
    unloadTexture(resourceCrystal_);
    unloadTexture(resourceStone_);
    unloadTexture(resourceWood_);
    unloadTexture(barYellow_);
    unloadTexture(barGreen_);
    unloadTexture(barRed_);
    unloadTexture(barBack_);
    unloadTexture(buttonPressed_);
    unloadTexture(button_);
    unloadTexture(insetPanel_);
    unloadTexture(panel_);
    shutdownUiText();
    initialized_ = false;
}

void GameUi::drawPanel(Rectangle bounds, unsigned char alpha) const {
    if (IsTextureValid(panel_)) {
        DrawTextureNPatch(panel_, PanelPatch, bounds, {0.0F, 0.0F},
                          0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {55, 38, 28, alpha});
    }
}

void GameUi::drawInsetPanel(Rectangle bounds,
                            unsigned char alpha) const {
    if (IsTextureValid(insetPanel_)) {
        DrawTextureNPatch(insetPanel_, InsetPatch, bounds,
                          {0.0F, 0.0F}, 0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {35, 25, 21, alpha});
    }
}

void GameUi::drawLabel(Rectangle bounds, std::string_view text,
                       int alignment) const {
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, alignment);
    const std::string owned{text};
    (void)GuiLabel(bounds, owned.c_str());
}

void GameUi::drawProgressBar(Rectangle bounds, float fraction,
                             UiBarColor color) const {
    fraction = std::clamp(fraction, 0.0F, 1.0F);
    if (IsTextureValid(barBack_)) {
        DrawTexturePro(
            barBack_,
            {0.0F, 0.0F, static_cast<float>(barBack_.width),
             static_cast<float>(barBack_.height)},
            bounds, {0.0F, 0.0F}, 0.0F, WHITE);
    } else {
        DrawRectangleRec(bounds, {31, 24, 22, 235});
    }

    Texture2D fill = barGreen_;
    if (color == UiBarColor::Red) {
        fill = barRed_;
    } else if (color == UiBarColor::Yellow) {
        fill = barYellow_;
    }
    Rectangle filled = bounds;
    filled.width *= fraction;
    if (filled.width <= 0.0F) {
        return;
    }
    if (IsTextureValid(fill)) {
        DrawTexturePro(fill,
                       {0.0F, 0.0F, static_cast<float>(fill.width),
                        static_cast<float>(fill.height)},
                       filled, {0.0F, 0.0F}, 0.0F, WHITE);
    } else {
        const Color fallback =
            color == UiBarColor::Red
                ? Color{190, 55, 50, 255}
                : (color == UiBarColor::Yellow
                       ? Color{218, 170, 58, 255}
                       : Color{72, 164, 82, 255});
        DrawRectangleRec(filled, fallback);
    }
}

void GameUi::drawResourceIcon(Rectangle bounds,
                              UiResourceIcon icon,
                              Color tint) const {
    const Texture2D texture = resourceTexture(icon);
    if (!IsTextureValid(texture)) {
        return;
    }
    DrawTexturePro(
        texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        bounds, {0.0F, 0.0F}, 0.0F, tint);
}

Texture2D GameUi::resourceTexture(
    UiResourceIcon icon) const {
    if (icon == UiResourceIcon::Stone) {
        return resourceStone_;
    }
    if (icon == UiResourceIcon::Crystal) {
        return resourceCrystal_;
    }
    return resourceWood_;
}

bool GameUi::drawButton(Rectangle bounds,
                        std::string_view text) const {
    return drawToggleButton(bounds, text, false);
}

bool GameUi::drawToggleButton(Rectangle bounds,
                              std::string_view text,
                              bool active) const {
    const bool pressed =
        CheckCollisionPointRec(GetMousePosition(), bounds) &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Texture2D texture =
        pressed && IsTextureValid(buttonPressed_) ? buttonPressed_
                                                  : button_;
    if (IsTextureValid(texture)) {
        const Color tint =
            active ? Color{205, 238, 190, 255} : WHITE;
        DrawTexturePro(
            texture,
            {0.0F, 0.0F, static_cast<float>(texture.width),
             static_cast<float>(texture.height)},
            bounds, {0.0F, 0.0F}, 0.0F, tint);
    }
    const std::string owned{text};
    return GuiButton(bounds, owned.c_str()) != 0;
}

float GameUi::drawSliderBar(
    Rectangle bounds, float value,
    float minimum, float maximum) const {
    (void)GuiSliderBar(
        bounds, nullptr, nullptr, &value, minimum, maximum);
    return value;
}

Texture2D GameUi::loadTexture(const char* path) {
    Texture2D texture = LoadTexture(path);
    return IsTextureValid(texture) ? texture : Texture2D{};
}

void GameUi::unloadTexture(Texture2D& texture) {
    if (IsTextureValid(texture)) {
        UnloadTexture(texture);
    }
    texture = {};
}

} // namespace ian
