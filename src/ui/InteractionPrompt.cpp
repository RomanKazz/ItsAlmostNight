#include "ui/InteractionPrompt.hpp"

#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/UiText.hpp"
#include "ui/TargetHealthBarAnchor.hpp"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {
namespace {

constexpr float SwitchDelay = 0.0F;
constexpr float MissingGrace = 0.0F;
constexpr float PromptMaxDistance = 16.0F;

float approach(float current, float target, float seconds,
               float deltaSeconds) {
    const float blend = 1.0F - std::exp(
        -deltaSeconds / std::max(seconds, 0.001F));
    return current + (target - current) * blend;
}

unsigned char alphaScale(unsigned char alpha, float opacity) {
    return static_cast<unsigned char>(std::clamp(
        std::lround(static_cast<float>(alpha) * opacity),
        0L, 255L));
}

Color withOpacity(Color color, float opacity) {
    color.a = alphaScale(color.a, opacity);
    return color;
}

void drawText(std::string_view text, Vector2 position,
              float size, Color color, float opacity) {
    drawUiText(text, position, size, withOpacity(color, opacity));
}

struct WorldPromptPlacement {
    Vector2 center{};
    Vector2 right{1.0F, 0.0F};
    float angle{};
    float pixelsPerWorldUnit{};
};

std::optional<WorldPromptPlacement> worldPromptPlacement(
    const InteractionPrompt& prompt, const Camera3D& camera) {
    const Vector2 projected = GetWorldToScreen(
        prompt.worldAnchor, camera);
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(cameraForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, cameraForward));
    const Vector2 projectedUp = GetWorldToScreen(
        Vector3Add(prompt.worldAnchor, cameraUp), camera);
    // Size world prompts from the camera projection at their anchor depth.
    const float pixelsPerWorldUnit = std::hypot(
        projectedUp.x - projected.x,
        projectedUp.y - projected.y);
    if (!std::isfinite(pixelsPerWorldUnit) ||
        pixelsPerWorldUnit <= 0.0F) {
        return std::nullopt;
    }
    const Vector3 surfaceNormal = Vector3Normalize(
        prompt.surfaceNormal);
    Vector3 tangentRight = Vector3Subtract(
        cameraRight,
        Vector3Scale(surfaceNormal,
                     Vector3DotProduct(cameraRight, surfaceNormal)));
    if (Vector3LengthSqr(tangentRight) < 0.01F) {
        tangentRight = cameraRight;
    }
    // Follow the surface tilt without turning edge-on as the camera
    // walks around the object.
    const Vector3 rightWorld = Vector3Add(
        prompt.worldAnchor,
        Vector3Scale(Vector3Normalize(tangentRight), 0.8F));
    const Vector2 projectedRight = GetWorldToScreen(
        rightWorld, camera);
    Vector2 right{
        projectedRight.x - projected.x,
        projectedRight.y - projected.y,
    };
    const float rightLength = std::hypot(right.x, right.y);
    if (std::isfinite(rightLength) && rightLength > 1.0F) {
        right.x /= rightLength;
        right.y /= rightLength;
        if (right.x < 0.0F) {
            right.x = -right.x;
            right.y = -right.y;
        }
    } else {
        right = {1.0F, 0.0F};
    }
    return WorldPromptPlacement{
        .center = projected,
        .right = right,
        .angle = std::atan2(right.y, right.x) * RAD2DEG,
        .pixelsPerWorldUnit = pixelsPerWorldUnit,
    };
}

void drawChestCostPrompt(
    const InteractionPrompt& prompt, const Camera3D& camera,
    const GameUi& ui, float opacity, float scale) {
    const auto placement = worldPromptPlacement(prompt, camera);
    if (!placement) {
        return;
    }
    const Vector2 projected = placement->center;
    const Vector2 right = placement->right;
    const float angle = placement->angle;

    const int current = prompt.availableCurrency.value_or(0);
    const int price = prompt.cost ? prompt.cost->crystals : 0;
    const std::string amount =
        std::to_string(current) + "/" + std::to_string(price);
    const float worldScale = placement->pixelsPerWorldUnit * scale;
    constexpr float UiTextScale = 1.68F;
    const float iconSize = 0.19F * worldScale;
    const float amountFontSize = 0.25F * worldScale / UiTextScale;
    const float gap = 0.025F * worldScale;
    const float amountWidth = measureUiText(amount, amountFontSize).x;
    const float totalWidth = iconSize + gap + amountWidth;
    const Vector2 iconCenter{
        projected.x + right.x * (-totalWidth * 0.5F + iconSize * 0.5F),
        projected.y + right.y * (-totalWidth * 0.5F + iconSize * 0.5F),
    };
    const Texture2D icon = ui.resourceTexture(UiResourceIcon::Coin);
    if (IsTextureValid(icon)) {
        DrawTexturePro(
            icon,
            {0.0F, 0.0F, static_cast<float>(icon.width),
             static_cast<float>(icon.height)},
            {iconCenter.x, iconCenter.y, iconSize, iconSize},
            {iconSize * 0.5F, iconSize * 0.5F}, angle,
            withOpacity(WHITE, opacity));
    }

    const Vector2 textCenter{
        projected.x + right.x *
            (-totalWidth * 0.5F + iconSize + gap + amountWidth * 0.5F),
        projected.y + right.y *
            (-totalWidth * 0.5F + iconSize + gap + amountWidth * 0.5F),
    };
    SdfUiTextStyle textStyle{};
    textStyle.fillColor = withOpacity(UiWhite, opacity);
    textStyle.outlineColor = withOpacity(
        {49, 49, 49, 255}, opacity);
    const float hudScale = amountFontSize / (52.0F / UiTextScale);
    textStyle.outlinePixels = 3.0F * hudScale;
    textStyle.shadowColor = withOpacity(
        {82, 82, 82, 255}, opacity);
    textStyle.shadowOffsetPixels = {0.0F, 4.0F * hudScale};
    drawRotatedStyledUiText(
        amount, textCenter, amountFontSize, angle, textStyle);
}

void drawChallengeKeyPrompt(
    const InteractionPrompt& prompt, const Camera3D& camera,
    const ControlSettings& controls, float opacity, float scale) {
    const auto placement = worldPromptPlacement(prompt, camera);
    if (!placement) {
        return;
    }
    const std::string key = InputKeycap::label(controls, prompt.input);
    const float worldScale = placement->pixelsPerWorldUnit * scale;
    const float size = 0.34F * worldScale;
    constexpr float UiTextScale = 1.68F;
    float fontSize = size * 0.55F / UiTextScale;
    const float width = std::max(
        size, measureUiText(key, fontSize).x + size * 0.25F);
    const float labelFontSize = 0.22F * worldScale / UiTextScale;
    const float labelWidth = measureUiText(
        prompt.actionText, labelFontSize).x;
    const float gap = 0.065F * worldScale;
    const float totalWidth = width + gap + labelWidth;
    const float keyOffset = -totalWidth * 0.5F + width * 0.5F;
    const Vector2 keyCenter{
        placement->center.x + placement->right.x * keyOffset,
        placement->center.y + placement->right.y * keyOffset,
    };
    const float left = keyCenter.x - width * 0.5F;
    const float top = keyCenter.y - size * 0.5F;
    const bool pressed = InputKeycap::held(controls, prompt.input);
    const float faceDrop = pressed ? size * 0.045F : 0.0F;
    const float inset = size * 0.045F;
    const float innerWidth = width - inset * 2.0F;
    const float innerHeight = size - inset * 2.0F;
    DrawRectangleRounded(
        {left, top, width, size}, 0.25F, 8,
        withOpacity({104, 60, 37, 255}, opacity));
    DrawRectangleRounded(
        {left + inset, top + inset, innerWidth, innerHeight},
        0.23F, 8, withOpacity({147, 115, 73, 255}, opacity));
    const Rectangle face{
        left + inset,
        top + inset + faceDrop,
        innerWidth,
        innerHeight - size * 0.075F,
    };
    DrawRectangleRounded(
        face, 0.23F, 8,
        withOpacity(pressed
                        ? Color{205, 158, 113, 255}
                        : Color{230, 188, 147, 255},
                    opacity));
    DrawRectangleRoundedLinesEx(
        face, 0.23F, 8, std::max(1.0F, size * 0.007F),
        withOpacity({246, 215, 177, 255}, opacity));

    while (measureUiText(key, fontSize).x > width * 0.72F &&
           fontSize > 5.0F) {
        fontSize -= 1.0F;
    }
    SdfUiTextStyle textStyle{};
    textStyle.fillColor = withOpacity(UiWhite, opacity);
    textStyle.outlineColor = withOpacity(
        {61, 39, 26, 255}, opacity);
    textStyle.outlinePixels = std::clamp(
        size * 0.042F, 1.0F, 4.0F);
    drawRotatedStyledUiText(
        key,
        {keyCenter.x,
         keyCenter.y - size * 0.035F + faceDrop},
        fontSize, 0.0F, textStyle);

    const float labelOffset =
        -totalWidth * 0.5F + width + gap + labelWidth * 0.5F;
    const Vector2 labelCenter{
        placement->center.x + placement->right.x * labelOffset,
        placement->center.y + placement->right.y * labelOffset,
    };
    SdfUiTextStyle labelStyle{};
    labelStyle.fillColor = withOpacity(UiWhite, opacity);
    labelStyle.outlineColor = withOpacity({49, 49, 49, 255}, opacity);
    labelStyle.outlinePixels = std::clamp(
        size * 0.035F, 1.0F, 3.0F);
    labelStyle.shadowColor = withOpacity({82, 82, 82, 255}, opacity);
    labelStyle.shadowOffsetPixels = {0.0F, size * 0.04F};
    drawRotatedStyledUiText(
        prompt.actionText, labelCenter, labelFontSize,
        placement->angle, labelStyle);
}

void drawBuildingUpgradePrompt(
    const InteractionPrompt& prompt, const Camera3D& camera,
    const GameUi& ui, float opacity) {
    if (!prompt.cost) return;
    const auto placement = worldPromptPlacement(prompt, camera);
    if (!placement) return;

    // Match the health bar's world-space anchor and perspective scaling.
    const float size = placement->pixelsPerWorldUnit * BuildingUpgradePromptWorldHeight;
    const float iconSize = size * 0.76F;
    const float fontSize = iconSize / 1.68F;
    const float gap = size * 0.10F;
    struct Price { UiResourceIcon icon; int amount; int available; };
    const std::array prices{
        Price{UiResourceIcon::Wood, prompt.cost->wood, prompt.availableResources.wood},
        Price{UiResourceIcon::Stone, prompt.cost->stone, prompt.availableResources.stone},
        Price{UiResourceIcon::Crystal, prompt.cost->crystals, prompt.availableResources.crystals},
    };
    float width = size;
    for (const auto& price : prices) {
        if (price.amount > 0)
            width += gap + iconSize + gap * 0.5F +
                measureUiText(std::to_string(price.amount), fontSize).x;
    }
    const float left = placement->center.x - width * 0.5F;
    const float centerY = placement->center.y;
    const Texture2D arrow = ui.coreMenuTexture(CoreMenuTexture::UpgradeGreen);
    if (IsTextureValid(arrow)) {
        DrawTexturePro(arrow,
            {0.0F, 0.0F, static_cast<float>(arrow.width), static_cast<float>(arrow.height)},
            {left, centerY - size * 0.5F, size, size}, {}, 0.0F,
            withOpacity(WHITE, opacity));
    }

    float cursor = left + size;
    SdfUiTextStyle style{};
    style.outlineColor = withOpacity({20, 24, 20, 255}, opacity);
    style.outlinePixels = size * 0.065F;
    style.shadowColor = withOpacity({0, 0, 0, 140}, opacity);
    style.shadowOffsetPixels = {0.0F, size * 0.055F};
    for (const auto& price : prices) {
        if (price.amount <= 0) continue;
        cursor += gap;
        ui.drawResourceIcon({cursor, centerY - iconSize * 0.5F,
            iconSize, iconSize}, price.icon, withOpacity(WHITE, opacity));
        cursor += iconSize + gap * 0.5F;
        const std::string amount = std::to_string(price.amount);
        const Vector2 measured = measureUiText(amount, fontSize);
        style.fillColor = withOpacity(price.available >= price.amount
            ? UiWhite : Color{255, 104, 90, 255}, opacity);
        drawStyledSdfUiText(amount, {cursor, centerY - measured.y * 0.5F},
            fontSize, style);
        cursor += measured.x;
    }
}

} // namespace

InteractionPromptRenderer::TargetKey InteractionPromptRenderer::keyFor(
    const InteractionPrompt& prompt) {
    return {prompt.targetKind, prompt.targetId};
}

bool InteractionPromptRenderer::projectable(
    const InteractionPrompt& prompt, const Camera3D& camera) {
    if (prompt.occluded) {
        return false;
    }
    const Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 offset = Vector3Subtract(
        prompt.worldAnchor, camera.position);
    if (Vector3DotProduct(offset, forward) <= 0.05F ||
        Vector3Length(offset) > PromptMaxDistance) {
        return false;
    }
    const Vector2 screen = GetWorldToScreen(
        prompt.worldAnchor, camera);
    return std::isfinite(screen.x) && std::isfinite(screen.y) &&
           screen.x > -96.0F &&
           screen.x < static_cast<float>(GetScreenWidth()) + 96.0F &&
           screen.y > -96.0F &&
           screen.y < static_cast<float>(GetScreenHeight()) + 96.0F;
}

void InteractionPromptRenderer::activate(
    const InteractionPrompt& prompt) {
    activeTarget_ = keyFor(prompt);
    activePrompt_ = prompt;
    opacity_ = 0.0F;
    scale_ = 0.96F;
    liftPixels_ = 5.0F;
    progress_ = 0.0F;
    progressOpacity_ = 0.0F;
    missingSeconds_ = 0.0;
}

void InteractionPromptRenderer::updateAnimation(
    float deltaSeconds, bool wantsVisible,
    const InteractionPrompt* prompt) {
    if (prompt && prompt->recentFailure) {
        if (!failureSignalActive_) {
            failureRemaining_ = 0.16F;
        }
        failureSignalActive_ = true;
    } else {
        failureSignalActive_ = false;
    }
    if (prompt && prompt->recentSuccess) {
        successRemaining_ = 0.18F;
    }
    failureRemaining_ = std::max(
        0.0F, failureRemaining_ - deltaSeconds);
    successRemaining_ = std::max(
        0.0F, successRemaining_ - deltaSeconds);

    opacity_ = approach(
        opacity_, wantsVisible ? 1.0F : 0.0F,
        wantsVisible ? 0.035F : 0.045F, deltaSeconds);
    scale_ = approach(
        scale_, wantsVisible ? 1.0F : 0.96F,
        wantsVisible ? 0.045F : 0.050F, deltaSeconds);
    liftPixels_ = approach(
        liftPixels_, wantsVisible ? 0.0F : 5.0F,
        wantsVisible ? 0.045F : 0.050F, deltaSeconds);

    const bool showProgress = prompt && prompt->showProgress &&
                              prompt->targetKind !=
                                  InteractionPromptTargetKind::Resource &&
                              wantsVisible;
    if (showProgress) {
        progress_ = approach(
            progress_, std::clamp(prompt->progress, 0.0F, 1.0F),
            0.08F, deltaSeconds);
        progressOpacity_ = approach(
            progressOpacity_, 1.0F, 0.08F, deltaSeconds);
    } else {
        progressOpacity_ = approach(
            progressOpacity_, 0.0F, 0.52F, deltaSeconds);
    }
}

void InteractionPromptRenderer::draw(
    const std::optional<InteractionPrompt>& prompt,
    const Camera3D& camera, const GameUi& ui,
    const ControlSettings& controls) {
    const float deltaSeconds = std::clamp(
        GetFrameTime(), 0.0F, 0.10F);
    const bool visibleCandidate =
        prompt && projectable(*prompt, camera);
    const TargetKey candidateKey = visibleCandidate
        ? keyFor(*prompt)
        : TargetKey{InteractionPromptTargetKind::Resource, {}};

    if (visibleCandidate) {
        if (!activeTarget_ || *activeTarget_ == candidateKey) {
            if (!activeTarget_) {
                activate(*prompt);
            } else if (activePrompt_) {
                *activePrompt_ = *prompt;
            }
            candidateTarget_.reset();
            candidateSeconds_ = 0.0;
            missingSeconds_ = 0.0;
        } else {
            missingSeconds_ += deltaSeconds;
            if (!candidateTarget_ ||
                *candidateTarget_ != candidateKey) {
                candidateTarget_ = candidateKey;
                candidateSeconds_ = 0.0;
            } else {
                candidateSeconds_ += deltaSeconds;
            }
            if (candidateSeconds_ >= SwitchDelay) {
                activate(*prompt);
                candidateTarget_.reset();
                candidateSeconds_ = 0.0;
            }
        }
    } else {
        candidateTarget_.reset();
        candidateSeconds_ = 0.0;
        if (activeTarget_) {
            missingSeconds_ += deltaSeconds;
        }
        if (missingSeconds_ >= MissingGrace) {
            activeTarget_.reset();
        }
    }

    const bool wantsVisible = activeTarget_.has_value() &&
                              activePrompt_.has_value() &&
                              (visibleCandidate ||
                               missingSeconds_ < MissingGrace);
    updateAnimation(
        deltaSeconds, wantsVisible,
        wantsVisible && visibleCandidate ? &*prompt : nullptr);
    if (opacity_ <= 0.005F || !activePrompt_) {
        return;
    }
    drawPrompt(*activePrompt_, camera, ui, controls,
               opacity_, scale_, liftPixels_);
}

void InteractionPromptRenderer::drawPrompt(
    const InteractionPrompt& prompt, const Camera3D& camera,
    const GameUi& ui, const ControlSettings& controls,
    float opacity, float scale, float liftPixels) {
    if (prompt.targetKind == InteractionPromptTargetKind::Building) {
        if (projectable(prompt, camera))
            drawBuildingUpgradePrompt(prompt, camera, ui, opacity);
        return;
    }
    if (prompt.targetKind == InteractionPromptTargetKind::Chest) {
        drawChestCostPrompt(
            prompt, camera, ui, opacity, scale);
        return;
    }
    if (prompt.targetKind == InteractionPromptTargetKind::Challenge) {
        drawChallengeKeyPrompt(
            prompt, camera, controls, opacity, scale);
        return;
    }
    const Vector2 projected = GetWorldToScreen(
        prompt.worldAnchor, camera);
    const std::string keyLabel = InputKeycap::label(
        controls, prompt.input);
    const Vector2 keySize = InputKeycap::size(keyLabel, 36.0F);
    constexpr float PreferredMainFontSize = 18.0F;
    constexpr float PreferredHintFontSize = 13.0F;
    constexpr float PreferredObjectFontSize = 12.0F;
    constexpr float PaddingX = 12.0F;
    constexpr float PaddingY = 10.0F;
    constexpr float KeyGap = 9.0F;
    constexpr float CostGap = 10.0F;
    constexpr float ProgressGap = 6.0F;
    const bool insufficient =
        prompt.cost && prompt.cost->crystals > 0 &&
        prompt.availableCurrency &&
        *prompt.availableCurrency < prompt.cost->crystals &&
        !prompt.hint.has_value();
    const bool hasHint = prompt.hint.has_value() || insufficient;
    const bool hasObjectName = !prompt.objectName.empty();
    const float maximumPanelWidth = std::max(
        180.0F,
        static_cast<float>(GetScreenWidth()) - 28.0F);
    const float maximumContentWidth =
        maximumPanelWidth - PaddingX * 2.0F;
    const float preferredCostWidth =
        prompt.cost && prompt.cost->crystals > 0
            ? 18.0F + 5.0F + measureUiText(
                  std::to_string(prompt.cost->crystals),
                  PreferredMainFontSize).x
            : 0.0F;
    const float actionMaximumWidth = std::max(
        48.0F,
        maximumContentWidth - keySize.x - KeyGap -
            ((preferredCostWidth > 0.0F && !insufficient)
                 ? CostGap + preferredCostWidth
                 : 0.0F));
    const float mainFontSize = fitUiTextSize(
        prompt.actionText, PreferredMainFontSize, 11.0F,
        actionMaximumWidth);
    const float hintFontSize = prompt.hint
        ? fitUiTextSize(
              *prompt.hint, PreferredHintFontSize, 9.0F,
              maximumContentWidth)
        : PreferredHintFontSize;
    const float objectFontSize = fitUiTextSize(
        prompt.objectName, PreferredObjectFontSize, 8.0F,
        std::max(
            48.0F,
            maximumContentWidth - keySize.x - KeyGap));
    const float actionWidth =
        measureUiText(prompt.actionText, mainFontSize).x;
    const float costWidth = prompt.cost && prompt.cost->crystals > 0
        ? 18.0F + 5.0F + measureUiText(
              std::to_string(prompt.cost->crystals), mainFontSize).x
        : 0.0F;
    const float lineWidth = keySize.x + KeyGap + actionWidth +
        ((costWidth > 0.0F && !insufficient) ? CostGap + costWidth : 0.0F);
    const float hintWidth = prompt.hint
        ? measureUiText(*prompt.hint, hintFontSize).x
        : (insufficient
               ? 18.0F + 5.0F +
                     measureUiText(
                         std::to_string(*prompt.availableCurrency) +
                             " / " +
                             std::to_string(prompt.cost->crystals),
                         hintFontSize).x
               : 0.0F);
    const float contentWidth = std::max(lineWidth, hintWidth);
    const float panelWidth = std::min(
        maximumPanelWidth,
        contentWidth + PaddingX * 2.0F);
    const float lineTop = hasObjectName ? 25.0F : PaddingY;
    const float panelHeight = lineTop + keySize.y +
        (hasHint ? 21.0F : 0.0F) + PaddingY;
    const float scaledWidth = panelWidth * scale;
    const float scaledHeight = panelHeight * scale;
    const float safeLeft = 14.0F;
    const float safeTop = 86.0F;
    const float safeRight =
        static_cast<float>(GetScreenWidth()) - 14.0F;
    const float safeBottom =
        static_cast<float>(GetScreenHeight()) - 142.0F;
    float x = projected.x - scaledWidth * 0.5F;
    float y = projected.y - scaledHeight - 14.0F - liftPixels;
    x = std::clamp(x, safeLeft,
                   std::max(safeLeft, safeRight - scaledWidth));
    y = std::clamp(y, safeTop,
                   std::max(safeTop, safeBottom - scaledHeight));

    const Rectangle crosshair{
        static_cast<float>(GetScreenWidth()) * 0.5F - 58.0F,
        static_cast<float>(GetScreenHeight()) * 0.5F - 50.0F,
        116.0F, 100.0F};
    const Rectangle panelRect{x, y, scaledWidth, scaledHeight};
    if (CheckCollisionRecs(panelRect, crosshair)) {
        y = std::clamp(
            static_cast<float>(GetScreenHeight()) * 0.5F -
                74.0F - scaledHeight,
            safeTop, std::max(safeTop, safeBottom - scaledHeight));
    }

    const Color shadow{0, 0, 0, alphaScale(100, opacity)};
    DrawRectangleRounded(
        {x + 2.0F, y + 3.0F, scaledWidth, scaledHeight},
        0.18F, 8, shadow);
    DrawRectangleRounded(
        {x, y, scaledWidth, scaledHeight},
        0.18F, 8, {17, 20, 23, alphaScale(172, opacity)});
    DrawRectangleRoundedLinesEx(
        {x, y, scaledWidth, scaledHeight},
        0.18F, 8, 1.0F,
        {224, 214, 192, alphaScale(35, opacity)});
    const float accentWidth =
        prompt.targetKind == InteractionPromptTargetKind::Loot
            ? std::min(28.0F * scale, scaledWidth - 20.0F)
            : scaledWidth - 20.0F;
    DrawRectangle(
        static_cast<int>(x + 10.0F),
        static_cast<int>(y + scaledHeight - 3.0F),
        std::max(1, static_cast<int>(accentWidth)),
        2, withOpacity(prompt.accentColor, opacity));

    const float scaledKeyWidth = keySize.x * scale;
    const float scaledKeyHeight = keySize.y * scale;
    const float successProgress = successRemaining_ > 0.0F
        ? 1.0F - std::clamp(successRemaining_ / 0.18F, 0.0F, 1.0F)
        : 1.0F;
    const float successScale = successRemaining_ > 0.0F
        ? 1.0F - std::sin(successProgress * PI) * 0.08F
        : 1.0F;
    const float failurePhase = failureRemaining_ > 0.0F
        ? 1.0F - std::clamp(failureRemaining_ / 0.16F, 0.0F, 1.0F)
        : 1.0F;
    const float shakeX = failureRemaining_ > 0.0F
        ? std::sin(failurePhase * PI * 5.0F) *
              (1.0F - failurePhase) * 3.0F
        : 0.0F;
    const float keyWidth = scaledKeyWidth * successScale;
    const float keyHeight = scaledKeyHeight * successScale;
    const float keyX = x + PaddingX * scale +
        (scaledKeyWidth - keyWidth) * 0.5F + shakeX;
    const float keyRight = keyX + keyWidth;
    const float keyY = y + lineTop * scale;
    const bool pressed = InputKeycap::held(controls, prompt.input) ||
                         successRemaining_ > 0.0F;
    InputKeycap::draw(
        ui,
        {keyX, keyY + (scaledKeyHeight - keyHeight) * 0.5F,
         keyWidth, keyHeight},
        keyLabel, pressed, alphaScale(255, opacity));

    const float textX = keyRight + KeyGap * scale;
    const float textY = y + lineTop * scale +
        (scaledKeyHeight - measureUiText(
            prompt.actionText, mainFontSize).y) * 0.5F;
    if (hasObjectName) {
        drawText(
            prompt.objectName,
            {textX, y + 8.0F * scale},
            objectFontSize, {177, 169, 153, 235}, opacity);
    }
    drawText(
        prompt.actionText, {textX, textY}, mainFontSize,
        {246, 239, 224, 255}, opacity);
    const UiResourceIcon costIcon =
        (prompt.targetKind == InteractionPromptTargetKind::Chest ||
         prompt.targetKind == InteractionPromptTargetKind::Landmark)
            ? UiResourceIcon::Coin
            : UiResourceIcon::Crystal;

    if (costWidth > 0.0F && !insufficient) {
        const float costX = textX + actionWidth * scale +
            CostGap * scale;
        const Texture2D icon = ui.resourceTexture(costIcon);
        if (IsTextureValid(icon)) {
            DrawTexturePro(
                icon,
                {0.0F, 0.0F, static_cast<float>(icon.width),
                 static_cast<float>(icon.height)},
                {costX, textY + 1.0F * scale,
                 18.0F * scale, 18.0F * scale},
                {0.0F, 0.0F}, 0.0F,
                withOpacity(WHITE, opacity));
        }
        drawText(
            std::to_string(prompt.cost->crystals),
            {costX + 23.0F * scale, textY},
            mainFontSize, {244, 233, 205, 255}, opacity);
    }

    if (hasHint) {
        const float hintX = x + PaddingX * scale;
        const float hintY = y + lineTop * scale +
            scaledKeyHeight + 2.0F * scale;
        if (prompt.hint) {
            const Color hintColor =
                prompt.state == InteractionState::Warning
                    ? Color{224, 173, 86, 245}
                    : Color{198, 191, 178, 235};
            drawText(*prompt.hint, {hintX, hintY},
                     hintFontSize, hintColor, opacity);
        } else if (insufficient) {
            const Texture2D icon = ui.resourceTexture(costIcon);
            if (IsTextureValid(icon)) {
                DrawTexturePro(
                    icon,
                    {0.0F, 0.0F, static_cast<float>(icon.width),
                     static_cast<float>(icon.height)},
                    {hintX, hintY + 1.0F * scale,
                     16.0F * scale, 16.0F * scale},
                    {0.0F, 0.0F}, 0.0F,
                    withOpacity(WHITE, opacity));
            }
            const float currentX = hintX + 21.0F * scale;
            drawText(
                std::to_string(*prompt.availableCurrency),
                {currentX, hintY}, hintFontSize,
                {224, 108, 96, 245}, opacity);
            const float slashX = currentX + measureUiText(
                std::to_string(*prompt.availableCurrency),
                hintFontSize).x;
            drawText(
                " / " + std::to_string(prompt.cost->crystals),
                {slashX, hintY}, hintFontSize,
                {220, 214, 201, 235}, opacity);
        }
    }

    if (progressOpacity_ > 0.01F) {
        const float progressY = y + scaledHeight + ProgressGap;
        const float progressAlpha = opacity * progressOpacity_;
        DrawRectangleRounded(
            {x, progressY, scaledWidth, 4.0F},
            0.5F, 4, {15, 17, 19, alphaScale(130, progressAlpha)});
        DrawRectangleRounded(
            {x, progressY, scaledWidth * std::clamp(
                progress_, 0.0F, 1.0F), 4.0F},
            0.5F, 4,
            withOpacity(prompt.accentColor, progressAlpha));
    }
}

void InteractionPromptRenderer::reset() {
    activeTarget_.reset();
    candidateTarget_.reset();
    activePrompt_.reset();
    candidateSeconds_ = 0.0;
    missingSeconds_ = 0.0;
    opacity_ = 0.0F;
    progressOpacity_ = 0.0F;
    failureRemaining_ = 0.0F;
    successRemaining_ = 0.0F;
    failureSignalActive_ = false;
}

} // namespace ian
