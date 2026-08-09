#include "ui/InteractionPrompt.hpp"

#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/UiText.hpp"

#include <raymath.h>

#include <algorithm>
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
        prompt.cost && prompt.cost->gold > 0 &&
        prompt.availableGold &&
        *prompt.availableGold < prompt.cost->gold &&
        !prompt.hint.has_value();
    const bool hasHint = prompt.hint.has_value() || insufficient;
    const bool hasObjectName = !prompt.objectName.empty();
    const float maximumPanelWidth = std::max(
        180.0F,
        static_cast<float>(GetScreenWidth()) - 28.0F);
    const float maximumContentWidth =
        maximumPanelWidth - PaddingX * 2.0F;
    const float preferredCostWidth =
        prompt.cost && prompt.cost->gold > 0
            ? 18.0F + 5.0F + measureUiText(
                  std::to_string(prompt.cost->gold),
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
    const float costWidth = prompt.cost && prompt.cost->gold > 0
        ? 18.0F + 5.0F + measureUiText(
              std::to_string(prompt.cost->gold), mainFontSize).x
        : 0.0F;
    const float lineWidth = keySize.x + KeyGap + actionWidth +
        ((costWidth > 0.0F && !insufficient) ? CostGap + costWidth : 0.0F);
    const float hintWidth = prompt.hint
        ? measureUiText(*prompt.hint, hintFontSize).x
        : (insufficient
               ? 18.0F + 5.0F +
                     measureUiText(
                         std::to_string(*prompt.availableGold) +
                             " / " +
                             std::to_string(prompt.cost->gold),
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
        prompt.targetKind == InteractionPromptTargetKind::Chest
            ? UiResourceIcon::Gold
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
            std::to_string(prompt.cost->gold),
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
                std::to_string(*prompt.availableGold),
                {currentX, hintY}, hintFontSize,
                {224, 108, 96, 245}, opacity);
            const float slashX = currentX + measureUiText(
                std::to_string(*prompt.availableGold),
                hintFontSize).x;
            drawText(
                " / " + std::to_string(prompt.cost->gold),
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
