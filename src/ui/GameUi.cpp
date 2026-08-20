#include "ui/GameUi.hpp"
#include "ui/UiCString.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <algorithm>
#include <cmath>
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

constexpr float UiTextScale = 1.68F;
constexpr int MinimapTargetSize = 768;

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

void drawHorizontalBar(Texture2D left, Texture2D middle,
                       Texture2D right, Rectangle bounds,
                       float width, Color tint = WHITE) {
    width = std::clamp(width, 0.0F, bounds.width);
    if (width <= 0.0F || bounds.height <= 0.0F ||
        !IsTextureValid(middle)) {
        return;
    }

    const Rectangle destination{
        bounds.x, bounds.y, width, bounds.height};
    const Rectangle middleSource{
        0.0F, 0.0F, static_cast<float>(middle.width),
        static_cast<float>(middle.height)};
    if (!IsTextureValid(left) || !IsTextureValid(right)) {
        DrawTexturePro(middle, middleSource, destination,
                       {0.0F, 0.0F}, 0.0F, tint);
        return;
    }

    const float scale = bounds.height /
        std::max(static_cast<float>(middle.height), 1.0F);
    const float capWidth = std::min(
        static_cast<float>(left.width) * scale, width * 0.5F);
    if (capWidth <= 0.0F || width <= capWidth * 2.0F) {
        DrawTexturePro(middle, middleSource, destination,
                       {0.0F, 0.0F}, 0.0F, tint);
        return;
    }

    const Rectangle leftSource{
        0.0F, 0.0F, static_cast<float>(left.width),
        static_cast<float>(left.height)};
    const Rectangle rightSource{
        0.0F, 0.0F, static_cast<float>(right.width),
        static_cast<float>(right.height)};
    DrawTexturePro(left, leftSource,
                   {bounds.x, bounds.y, capWidth, bounds.height},
                   {0.0F, 0.0F}, 0.0F, tint);
    DrawTexturePro(middle, middleSource,
                   {bounds.x + capWidth, bounds.y,
                    width - capWidth * 2.0F, bounds.height},
                   {0.0F, 0.0F}, 0.0F, tint);
    DrawTexturePro(right, rightSource,
                   {bounds.x + width - capWidth, bounds.y,
                    capWidth, bounds.height},
                   {0.0F, 0.0F}, 0.0F, tint);
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
    keyCap_ = loadTexture("assets/ui/buttonSquare_beige.png");
    keyCapPressed_ =
        loadTexture("assets/ui/buttonSquare_beige_pressed.png");
    barBackLeft_ =
        loadTexture("assets/ui/barBack_horizontalLeft.png");
    barBack_ = loadTexture("assets/ui/barBack_horizontalMid.png");
    barBackRight_ =
        loadTexture("assets/ui/barBack_horizontalRight.png");
    barBlueLeft_ =
        loadTexture("assets/ui/barBlue_horizontalLeft.png");
    barBlue_ = loadTexture("assets/ui/barBlue_horizontalBlue.png");
    barBlueRight_ =
        loadTexture("assets/ui/barBlue_horizontalRight.png");
    barRedLeft_ =
        loadTexture("assets/ui/barRed_horizontalLeft.png");
    barRed_ = loadTexture("assets/ui/barRed_horizontalMid.png");
    barRedRight_ =
        loadTexture("assets/ui/barRed_horizontalRight.png");
    barGreenLeft_ =
        loadTexture("assets/ui/barGreen_horizontalLeft.png");
    barGreen_ = loadTexture("assets/ui/barGreen_horizontalMid.png");
    barGreenRight_ =
        loadTexture("assets/ui/barGreen_horizontalRight.png");
    barYellowLeft_ =
        loadTexture("assets/ui/barYellow_horizontalLeft.png");
    barYellow_ = loadTexture("assets/ui/barYellow_horizontalMid.png");
    barYellowRight_ =
        loadTexture("assets/ui/barYellow_horizontalRight.png");
    sliderKnob_ = loadTexture("assets/ui/buttonRound_beige.png");
    sliderKnobHover_ =
        loadTexture("assets/ui/buttonRound_blue.png");
    cursorHand_ = loadTexture("assets/ui/cursorHand_beige.png");
    checkIcon_ = loadTexture("assets/ui/iconCheck_beige.png");
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
    unloadTexture(barYellowRight_);
    unloadTexture(barYellow_);
    unloadTexture(barYellowLeft_);
    unloadTexture(barGreenRight_);
    unloadTexture(barGreen_);
    unloadTexture(barGreenLeft_);
    unloadTexture(barRedRight_);
    unloadTexture(barRed_);
    unloadTexture(barRedLeft_);
    unloadTexture(barBlueRight_);
    unloadTexture(barBlue_);
    unloadTexture(barBlueLeft_);
    unloadTexture(barBackRight_);
    unloadTexture(barBack_);
    unloadTexture(barBackLeft_);
    unloadTexture(buttonPressed_);
    unloadTexture(button_);
    unloadTexture(keyCapPressed_);
    unloadTexture(keyCap_);
    unloadTexture(insetPanel_);
    unloadTexture(panel_);
    shutdownUiText();
    initialized_ = false;
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

int GameUi::minimapTargetSize() const {
    return IsRenderTextureValid(minimapTarget_)
        ? minimapTarget_.texture.width
        : 0;
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
        drawHorizontalBar(barBackLeft_, barBack_, barBackRight_,
                          bounds, bounds.width);
    } else {
        DrawRectangleRec(bounds, {31, 24, 22, 235});
    }

    Texture2D fill = barGreen_;
    if (color == UiBarColor::Red) {
        fill = barRed_;
    } else if (color == UiBarColor::Yellow) {
        fill = barYellow_;
    } else if (color == UiBarColor::Purple ||
               color == UiBarColor::Blue) {
        fill = {};
    }
    Rectangle filled = bounds;
    filled.width *= fraction;
    if (filled.width <= 0.0F) {
        return;
    }
    if (color == UiBarColor::Purple ||
        color == UiBarColor::Blue) {
        const bool blue = color == UiBarColor::Blue;
        const Rectangle purpleFill{
            filled.x + 2.0F, filled.y + 2.0F,
            std::max(0.0F, filled.width - 4.0F),
            std::max(0.0F, filled.height - 4.0F)};
        if (purpleFill.width > 0.0F) {
            DrawRectangleRounded(purpleFill, 0.42F, 8,
                                 blue ? Color{42, 137, 190, 255}
                                      : Color{113, 76, 199, 255});
            const Rectangle highlight{
                purpleFill.x + 1.0F, purpleFill.y + 1.0F,
                std::max(0.0F, purpleFill.width - 2.0F),
                std::max(1.0F, purpleFill.height * 0.38F)};
            DrawRectangleRounded(highlight, 0.5F, 8,
                                 blue ? Color{131, 226, 255, 220}
                                      : Color{190, 157, 255, 210});
            DrawRectangleRoundedLinesEx(
                purpleFill, 0.42F, 8, 1.0F,
                blue ? Color{183, 239, 255, 220}
                     : Color{218, 197, 255, 210});
        }
    } else if (IsTextureValid(fill)) {
        Texture2D fillLeft = barGreenLeft_;
        Texture2D fillRight = barGreenRight_;
        if (color == UiBarColor::Red) {
            fillLeft = barRedLeft_;
            fillRight = barRedRight_;
        } else if (color == UiBarColor::Yellow) {
            fillLeft = barYellowLeft_;
            fillRight = barYellowRight_;
        }
        drawHorizontalBar(fillLeft, fill, fillRight,
                          bounds, filled.width);
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
        const bool pressed =
            CheckCollisionPointRec(GetMousePosition(), bounds) &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const Texture2D buttonTexture =
            pressed && IsTextureValid(buttonPressed_)
                ? buttonPressed_ : button_;
        if (IsTextureValid(buttonTexture)) {
            DrawTexturePro(
                buttonTexture,
                {0.0F, 0.0F,
                 static_cast<float>(buttonTexture.width),
                 static_cast<float>(buttonTexture.height)},
                bounds, {0.0F, 0.0F}, 0.0F, WHITE);
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
    const std::string localized = localizedCopy(text);
    const int previousSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const float checkSpace = active ? 34.0F : 0.0F;
    GuiSetStyle(
        DEFAULT, TEXT_SIZE,
        fittedRayguiTextSize(
            localized, bounds, 24.0F + checkSpace, 10.0F));
    const bool clicked = withNullTerminatedUiText(
        localized, [bounds](const char* value) {
            return GuiButton(bounds, value) != 0;
        });
    GuiSetStyle(DEFAULT, TEXT_SIZE, previousSize);
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
        DrawTexturePro(
            texture,
            {0.0F, 0.0F, static_cast<float>(texture.width),
             static_cast<float>(texture.height)},
            {bounds.x, bounds.y + offset,
             bounds.width, bounds.height},
            {0.0F, 0.0F}, 0.0F,
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
        {70, 52, 34, alpha});
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
    drawHorizontalBar(barBackLeft_, barBack_, barBackRight_,
                      bounds, bounds.width);
    drawHorizontalBar(barBlueLeft_, barBlue_, barBlueRight_,
                      bounds, bounds.width * displayed);

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
