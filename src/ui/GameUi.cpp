#include "ui/GameUi.hpp"
#include "ui/UiCString.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#include <nlohmann/json.hpp>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace ian {
namespace {

constexpr float UiTextScale = 1.68F;
constexpr int MinimapTargetSize = 256;
constexpr int MaximumMinimapTargetSize = 768;

std::string localizedCopy(std::string_view text) {
    return currentLanguage() == Language::English
        ? std::string{text}
        : localizeText(text);
}

int fittedRayguiTextSize(
    std::string_view text, Rectangle bounds,
    float horizontalPadding, float verticalPadding) {
    const int preferred = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const float fitted = fitUiTextSize(
        text,
        static_cast<float>(preferred) / UiTextScale,
        7.0F,
        std::max(1.0F, bounds.width - horizontalPadding),
        std::max(1.0F, bounds.height - verticalPadding));
    return std::max(
        10, static_cast<int>(std::floor(fitted * UiTextScale)));
}

Color alphaTint(unsigned char alpha) {
    return {255, 255, 255, alpha};
}

NPatchInfo texturePatch(Texture2D texture, int left, int top,
                        int right, int bottom) {
    return {
        .source = {0.0F, 0.0F, static_cast<float>(texture.width),
                   static_cast<float>(texture.height)},
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .layout = NPATCH_NINE_PATCH,
    };
}

void drawPatched(Texture2D texture, Rectangle bounds,
                 int left, int top, int right, int bottom,
                 Color tint = WHITE) {
    if (!IsTextureValid(texture) || bounds.width <= 0.0F ||
        bounds.height <= 0.0F) {
        return;
    }
    DrawTextureNPatch(texture,
                      texturePatch(texture, left, top, right, bottom),
                      bounds, {0.0F, 0.0F}, 0.0F, tint);
}

} // namespace

GameUi::~GameUi() {
    shutdown();
}

void GameUi::initialize() {
    shutdown();
    panel_ = loadTexture("assets/ui/cartoony/panel.png");
    insetPanel_ = loadTexture("assets/ui/cartoony/inset.png");
    button_ = loadTexture("assets/ui/cartoony/button.png");
    buttonHover_ = loadTexture("assets/ui/cartoony/button-hover.png");
    buttonPressed_ = loadTexture("assets/ui/cartoony/button-pressed.png");
    buttonActive_ = loadTexture("assets/ui/cartoony/button-active.png");
    buttonActiveHover_ =
        loadTexture("assets/ui/cartoony/button-active-hover.png");
    buttonActivePressed_ =
        loadTexture("assets/ui/cartoony/button-active-pressed.png");
    keyCap_ = loadTexture("assets/ui/cartoony/button.png");
    keyCapPressed_ =
        loadTexture("assets/ui/cartoony/button-active-pressed.png");
    barBack_ = loadTexture("assets/ui/cartoony/bar-back.png");
    barBlue_ = loadTexture("assets/ui/cartoony/bar-blue.png");
    barRed_ = loadTexture("assets/ui/cartoony/bar-red.png");
    barGreen_ = loadTexture("assets/ui/cartoony/bar-green.png");
    barYellow_ = loadTexture("assets/ui/cartoony/bar-yellow.png");
    barPurple_ = loadTexture("assets/ui/cartoony/bar-purple.png");
    sliderKnob_ = loadTexture("assets/ui/cartoony/slider-knob.png");
    sliderKnobHover_ = loadTexture("assets/ui/cartoony/slider-knob.png");
    cursorHand_ = loadTexture("assets/ui/cursorHand_beige.png");
    checkIcon_ = loadTexture("assets/ui/cartoony/check.png");
    arrowLeft_ = loadTexture("assets/ui/arrowBrown_left.png");
    arrowRight_ = loadTexture("assets/ui/arrowBrown_right.png");
    resourceWood_ = loadTexture("assets/ui/resource_wood.png");
    resourceStone_ = loadTexture("assets/ui/resource_stone.png");
    resourceCrystal_ =
        loadTexture("assets/ui/resource_crystal.png");
    minimapTarget_ = LoadRenderTexture(
        MinimapTargetSize, MinimapTargetSize);
    if (IsTextureValid(minimapTarget_.texture)) {
        SetTextureFilter(
            minimapTarget_.texture, TEXTURE_FILTER_BILINEAR);
    }
    minimapCircleShader_ = LoadShader(
        nullptr, "assets/shaders/minimap_circle.fs");
    resourceCoin_ = loadTexture("assets/ui/resource_coin.png");
    reloadThemeSlices();
    initializeUiText();
    initialized_ = true;

    GuiSetFont(uiFont());
    GuiSetStyle(DEFAULT, TEXT_SIZE, 31);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,
                static_cast<int>(0x354c5effU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,
                static_cast<int>(0x213747ffU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,
                static_cast<int>(0x182b39ffU));
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x00000000);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,
                static_cast<int>(0xe8f7ffffU));
}

void GameUi::shutdown() {
    if (IsShaderValid(minimapCircleShader_)) {
        UnloadShader(minimapCircleShader_);
    }
    minimapCircleShader_ = {};
    if (IsRenderTextureValid(minimapTarget_)) {
        UnloadRenderTexture(minimapTarget_);
    }
    minimapTarget_ = {};
    unloadTexture(resourceCrystal_);
    unloadTexture(resourceCoin_);
    unloadTexture(resourceStone_);
    unloadTexture(resourceWood_);
    unloadTexture(arrowRight_);
    unloadTexture(arrowLeft_);
    unloadTexture(checkIcon_);
    unloadTexture(cursorHand_);
    unloadTexture(sliderKnobHover_);
    unloadTexture(sliderKnob_);
    unloadTexture(barPurple_);
    unloadTexture(barYellow_);
    unloadTexture(barGreen_);
    unloadTexture(barRed_);
    unloadTexture(barBlue_);
    unloadTexture(barBack_);
    unloadTexture(keyCapPressed_);
    unloadTexture(keyCap_);
    unloadTexture(buttonActivePressed_);
    unloadTexture(buttonActiveHover_);
    unloadTexture(buttonActive_);
    unloadTexture(buttonPressed_);
    unloadTexture(buttonHover_);
    unloadTexture(button_);
    unloadTexture(insetPanel_);
    unloadTexture(panel_);
    shutdownUiText();
    initialized_ = false;
}

void GameUi::reloadThemeSlices() {
    panelInsets_ = {16, 16, 16, 18};
    insetInsets_ = {9, 9, 9, 9};
    buttonInsets_ = {12, 10, 12, 14};
    barInsets_ = {12, 4, 12, 4};
    std::ifstream input{"assets/ui/ui_editor.json"};
    if (!input) return;
    try {
        const nlohmann::json root = nlohmann::json::parse(input);
        const auto found = root.find("assets");
        if (found == root.end() || !found->is_array()) return;
        for (const auto& item : *found) {
            const std::string path = item.value("path", "");
            SliceInsets* insets = nullptr;
            Texture2D texture{};
            if (path == "assets/ui/cartoony/panel.png") {
                insets = &panelInsets_;
                texture = panel_;
            } else if (path == "assets/ui/cartoony/inset.png") {
                insets = &insetInsets_;
                texture = insetPanel_;
            } else if (path == "assets/ui/cartoony/button.png") {
                insets = &buttonInsets_;
                texture = button_;
            } else if (path == "assets/ui/cartoony/bar-back.png") {
                insets = &barInsets_;
                texture = barBack_;
            }
            if (insets == nullptr || !IsTextureValid(texture)) continue;
            insets->left = std::clamp(
                item.value("left", insets->left), 0, texture.width - 1);
            insets->right = std::clamp(
                item.value("right", insets->right), 0,
                texture.width - insets->left - 1);
            insets->top = std::clamp(
                item.value("top", insets->top), 0, texture.height - 1);
            insets->bottom = std::clamp(
                item.value("bottom", insets->bottom), 0,
                texture.height - insets->top - 1);
        }
    } catch (const nlohmann::json::exception&) {
        // Keep safe defaults when the editor file is incomplete.
    }
}

bool GameUi::beginMinimapTarget() const {
    if (!IsRenderTextureValid(minimapTarget_)) {
        return false;
    }
    BeginTextureMode(minimapTarget_);
    ClearBackground(BLANK);
    return true;
}

void GameUi::endMinimapTarget() const {
    EndTextureMode();
}

void GameUi::drawMinimapTarget(Rectangle bounds) const {
    if (!IsRenderTextureValid(minimapTarget_)) {
        return;
    }
    if (IsShaderValid(minimapCircleShader_)) {
        BeginShaderMode(minimapCircleShader_);
    }
    DrawTexturePro(
        minimapTarget_.texture,
        {0.0F, 0.0F,
         static_cast<float>(minimapTarget_.texture.width),
         -static_cast<float>(minimapTarget_.texture.height)},
        bounds, {0.0F, 0.0F}, 0.0F, WHITE);
    if (IsShaderValid(minimapCircleShader_)) {
        EndShaderMode();
    }
}

void GameUi::updateMinimapTarget(int desiredSize) {
    desiredSize = std::clamp(
        desiredSize, MinimapTargetSize, MaximumMinimapTargetSize);
    if (IsRenderTextureValid(minimapTarget_) &&
        minimapTarget_.texture.width == desiredSize &&
        minimapTarget_.texture.height == desiredSize) {
        return;
    }
    if (IsRenderTextureValid(minimapTarget_)) {
        UnloadRenderTexture(minimapTarget_);
    }
    minimapTarget_ = LoadRenderTexture(desiredSize, desiredSize);
    if (IsTextureValid(minimapTarget_.texture)) {
        SetTextureFilter(
            minimapTarget_.texture, TEXTURE_FILTER_BILINEAR);
    }
}

int GameUi::minimapTargetSize() const {
    return IsRenderTextureValid(minimapTarget_)
        ? minimapTarget_.texture.width
        : 0;
}

void GameUi::drawPanel(Rectangle bounds, unsigned char alpha) const {
    if (IsTextureValid(panel_)) {
        DrawTextureNPatch(
            panel_, texturePatch(panel_, panelInsets_.left,
                                 panelInsets_.top, panelInsets_.right,
                                 panelInsets_.bottom),
            bounds, {0.0F, 0.0F}, 0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {239, 244, 247, alpha});
    }
}

void GameUi::drawInsetPanel(Rectangle bounds,
                            unsigned char alpha) const {
    if (IsTextureValid(insetPanel_)) {
        DrawTextureNPatch(
                          insetPanel_,
                          texturePatch(insetPanel_, insetInsets_.left,
                                       insetInsets_.top,
                                       insetInsets_.right,
                                       insetInsets_.bottom), bounds,
                          {0.0F, 0.0F}, 0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {209, 220, 227, alpha});
    }
}

void GameUi::drawCardPanel(Rectangle bounds, Color accent,
                           unsigned char alpha) const {
    if (IsTextureValid(panel_)) {
        DrawTextureNPatch(panel_,
                          texturePatch(panel_, panelInsets_.left,
                                       panelInsets_.top,
                                       panelInsets_.right,
                                       panelInsets_.bottom), bounds,
                          {0.0F, 0.0F}, 0.0F,
                          alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {24, 31, 48, alpha});
    }
    accent.a = static_cast<unsigned char>(
        static_cast<unsigned int>(accent.a) * alpha / 255U);
    DrawRectangleRoundedLinesEx(bounds, 0.08F, 8, 2.5F, accent);
}

void GameUi::drawLabel(Rectangle bounds, std::string_view text,
                       int alignment) const {
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, alignment);
    const std::string localized = localizedCopy(text);
    const int previousSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    GuiSetStyle(
        DEFAULT, TEXT_SIZE,
        fittedRayguiTextSize(localized, bounds, 16.0F, 8.0F));
    withNullTerminatedUiText(localized, [bounds](const char* value) {
        (void)GuiLabel(bounds, value);
    });
    GuiSetStyle(DEFAULT, TEXT_SIZE, previousSize);
}

void GameUi::drawProgressBar(Rectangle bounds, float fraction,
                             UiBarColor color) const {
    fraction = std::clamp(fraction, 0.0F, 1.0F);
    if (IsTextureValid(barBack_)) {
        drawPatched(barBack_, bounds, barInsets_.left,
                    barInsets_.top, barInsets_.right,
                    barInsets_.bottom);
    } else {
        DrawRectangleRounded(bounds, 0.45F, 8,
                             {17, 21, 54, 245});
    }

    Texture2D fill = barGreen_;
    if (color == UiBarColor::Red) {
        fill = barRed_;
    } else if (color == UiBarColor::Yellow) {
        fill = barYellow_;
    } else if (color == UiBarColor::Purple) {
        fill = barPurple_;
    } else if (color == UiBarColor::Blue) {
        fill = barBlue_;
    }
    const float inset = std::clamp(bounds.height * 0.12F, 2.0F, 5.0F);
    Rectangle filled{
        bounds.x + inset, bounds.y + inset,
        std::max(0.0F, (bounds.width - inset * 2.0F) * fraction),
        std::max(0.0F, bounds.height - inset * 2.0F)};
    if (filled.width <= 0.0F) {
        return;
    }
    if (IsTextureValid(fill)) {
        drawPatched(fill, filled, barInsets_.left,
                    barInsets_.top, barInsets_.right,
                    barInsets_.bottom);
    } else {
        const Color fallback =
            color == UiBarColor::Red
                ? Color{190, 55, 50, 255}
                : (color == UiBarColor::Yellow
                       ? Color{218, 170, 58, 255}
                       : (color == UiBarColor::Purple
                              ? Color{139, 112, 218, 255}
                              : color == UiBarColor::Blue
                              ? Color{55, 166, 214, 255}
                              : Color{72, 164, 82, 255}));
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
    if (icon == UiResourceIcon::Coin) {
        return resourceCoin_;
    }
    return resourceWood_;
}

bool GameUi::drawButton(Rectangle bounds,
                        std::string_view text) const {
    const bool arrowLeft = text == "<" || text == "-";
    const bool arrowRight = text == ">" || text == "+";
    if (arrowLeft || arrowRight) {
        const bool hovered = CheckCollisionPointRec(
            GetMousePosition(), bounds);
        const bool pressed = hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const Texture2D buttonTexture =
            pressed && IsTextureValid(buttonPressed_)
                ? buttonPressed_
                : hovered && IsTextureValid(buttonHover_)
                    ? buttonHover_ : button_;
        if (IsTextureValid(buttonTexture)) {
            drawPatched(buttonTexture, bounds, buttonInsets_.left,
                        buttonInsets_.top, buttonInsets_.right,
                        buttonInsets_.bottom);
        }
        const Texture2D arrow =
            arrowLeft ? arrowLeft_ : arrowRight_;
        if (IsTextureValid(arrow)) {
            const float size = std::min(
                bounds.height * 0.42F, 28.0F);
            DrawTexturePro(
                arrow,
                {0.0F, 0.0F, static_cast<float>(arrow.width),
                 static_cast<float>(arrow.height)},
                {bounds.x + (bounds.width - size) * 0.5F,
                 bounds.y + (bounds.height - size) * 0.5F,
                 size, size},
                {0.0F, 0.0F}, 0.0F, WHITE);
        }
        return GuiButton(bounds, "") != 0;
    }
    return drawToggleButton(bounds, text, false);
}

bool GameUi::drawToggleButton(Rectangle bounds,
                              std::string_view text,
                              bool active) const {
    const bool hovered = CheckCollisionPointRec(
        GetMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Texture2D texture = active
        ? (pressed && IsTextureValid(buttonActivePressed_)
               ? buttonActivePressed_
               : hovered && IsTextureValid(buttonActiveHover_)
                   ? buttonActiveHover_ : buttonActive_)
        : (pressed && IsTextureValid(buttonPressed_)
               ? buttonPressed_
               : hovered && IsTextureValid(buttonHover_)
                   ? buttonHover_ : button_);
    if (IsTextureValid(texture)) {
        drawPatched(texture, bounds, buttonInsets_.left,
                    buttonInsets_.top, buttonInsets_.right,
                    buttonInsets_.bottom);
    }
    const std::string localized = localizedCopy(text);
    const int previousSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int normalText = GuiGetStyle(BUTTON, TEXT_COLOR_NORMAL);
    const int focusedText = GuiGetStyle(BUTTON, TEXT_COLOR_FOCUSED);
    const int pressedText = GuiGetStyle(BUTTON, TEXT_COLOR_PRESSED);
    const float checkSpace = active ? 34.0F : 0.0F;
    GuiSetStyle(
        DEFAULT, TEXT_SIZE,
        fittedRayguiTextSize(
            localized, bounds, 24.0F + checkSpace, 10.0F));
    if (active) {
        constexpr int DarkText = static_cast<int>(0x352000ffU);
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, DarkText);
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, DarkText);
        GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, DarkText);
    }
    const bool clicked = withNullTerminatedUiText(
        localized, [bounds](const char* value) {
            return GuiButton(bounds, value) != 0;
        });
    GuiSetStyle(DEFAULT, TEXT_SIZE, previousSize);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, normalText);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, focusedText);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, pressedText);
    if (active && IsTextureValid(checkIcon_)) {
        const float size = std::clamp(
            bounds.height * 0.32F, 14.0F, 22.0F);
        DrawTexturePro(
            checkIcon_,
            {0.0F, 0.0F, static_cast<float>(checkIcon_.width),
             static_cast<float>(checkIcon_.height)},
            {bounds.x + bounds.width - size - 12.0F,
             bounds.y + (bounds.height - size) * 0.5F,
             size, size},
            {0.0F, 0.0F}, 0.0F, WHITE);
    }
    return clicked;
}

bool GameUi::drawKeyCap(Rectangle bounds, std::string_view text,
                        bool pressed, unsigned char alpha) const {
    const bool hovered = CheckCollisionPointRec(
        GetMousePosition(), bounds);
    const bool held = pressed ||
        (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    const Texture2D texture =
        held && IsTextureValid(keyCapPressed_)
            ? keyCapPressed_
            : keyCap_;
    const float offset = held ? 2.0F : 0.0F;
    if (IsTextureValid(texture)) {
        drawPatched(
            texture,
            {bounds.x, bounds.y + offset,
             bounds.width, bounds.height},
            buttonInsets_.left, buttonInsets_.top,
            buttonInsets_.right,
            buttonInsets_.bottom,
            hovered ? Color{255, 255, 245, alpha}
                    : Color{255, 255, 255, alpha});
    } else {
        DrawRectangleRounded(
            {bounds.x, bounds.y + offset,
             bounds.width, bounds.height},
            0.16F, 4,
            held ? Color{193, 164, 113, alpha}
                 : Color{220, 195, 145, alpha});
    }
    float fontSize = std::clamp(
        bounds.height * 0.46F, 10.0F, 22.0F);
    Vector2 textSize = measureUiText(text, fontSize);
    while (textSize.x > bounds.width * 0.82F && fontSize > 7.0F) {
        fontSize -= 1.0F;
        textSize = measureUiText(text, fontSize);
    }
    drawUiText(
        text,
        {bounds.x + (bounds.width - textSize.x) * 0.5F,
         bounds.y + offset +
             (bounds.height - textSize.y) * 0.5F - 1.0F},
        fontSize,
        held ? Color{64, 37, 8, alpha}
             : Color{250, 247, 255, alpha});
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

Texture2D GameUi::keyCapTexture(bool pressed) const {
    if (pressed && IsTextureValid(keyCapPressed_)) {
        return keyCapPressed_;
    }
    return keyCap_;
}

float GameUi::drawSliderBar(
    Rectangle bounds, float value,
    float minimum, float maximum) const {
    if (maximum < minimum) {
        std::swap(minimum, maximum);
    }
    value = std::clamp(value, minimum, maximum);
    const float range = maximum - minimum;
    const Rectangle hitBounds{
        bounds.x, bounds.y - 8.0F,
        bounds.width, bounds.height + 16.0F};
    const Vector2 mouse = GetMousePosition();
    const bool hovered =
        CheckCollisionPointRec(mouse, hitBounds);
    if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        bounds.width > 0.0F && range > 0.0F) {
        const float position = std::clamp(
            (mouse.x - bounds.x) / bounds.width, 0.0F, 1.0F);
        value = minimum + range * position;
    }
    const float displayed = range > 0.0F
        ? std::clamp((value - minimum) / range, 0.0F, 1.0F)
        : 0.0F;
    drawProgressBar(bounds, displayed, UiBarColor::Blue);

    const Texture2D knob = hovered &&
        IsTextureValid(sliderKnobHover_)
        ? sliderKnobHover_ : sliderKnob_;
    if (IsTextureValid(knob)) {
        const float knobHeight = std::max(
            bounds.height * 1.45F, 26.0F);
        const float knobWidth = knobHeight *
            static_cast<float>(knob.width) /
            std::max(static_cast<float>(knob.height), 1.0F);
        const float centerX =
            bounds.x + bounds.width * displayed;
        DrawTexturePro(
            knob,
            {0.0F, 0.0F, static_cast<float>(knob.width),
             static_cast<float>(knob.height)},
            {centerX - knobWidth * 0.5F,
             bounds.y + (bounds.height - knobHeight) * 0.5F,
             knobWidth, knobHeight},
            {0.0F, 0.0F}, 0.0F, WHITE);
    }
    return value;
}

void GameUi::drawCursor() const {
    if (!IsTextureValid(cursorHand_)) {
        return;
    }
    const Vector2 mouse = GetMousePosition();
    constexpr float Scale = 1.12F;
    const float width =
        static_cast<float>(cursorHand_.width) * Scale;
    const float height =
        static_cast<float>(cursorHand_.height) * Scale;
    DrawTexturePro(
        cursorHand_,
        {0.0F, 0.0F, static_cast<float>(cursorHand_.width),
         static_cast<float>(cursorHand_.height)},
        {mouse.x - 2.0F, mouse.y - 1.0F, width, height},
        {0.0F, 0.0F}, 0.0F, WHITE);
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
