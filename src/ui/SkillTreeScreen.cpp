#include "ui/SkillTreeScreen.hpp"

#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace ian {
namespace {

constexpr float NodeHalfSize = 38.0F;
constexpr float LargeNodeHalfSize = 50.0F;
constexpr float RootNodeHalfSize = 48.0F;

float nodeHalfSize(const SkillNodeDefinition& node) {
    if (node.branch == SkillBranch::Root) {
        return RootNodeHalfSize;
    }
    return node.size == SkillNodeSize::Large
        ? LargeNodeHalfSize
        : NodeHalfSize;
}

float easeOutBack(float value) {
    value = std::clamp(value, 0.0F, 1.0F);
    constexpr float Overshoot = 1.70158F;
    const float shifted = value - 1.0F;
    return 1.0F +
           (Overshoot + 1.0F) * shifted * shifted * shifted +
           Overshoot * shifted * shifted;
}

float smoothStep(float value) {
    value = std::clamp(value, 0.0F, 1.0F);
    return value * value * (3.0F - 2.0F * value);
}

Color withAlpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(
        std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
    return color;
}

Color branchColor(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Construction:
        return {235, 173, 83, 255};
    case SkillBranch::Gathering:
        return {102, 198, 142, 255};
    case SkillBranch::Weapons:
        return {224, 104, 92, 255};
    case SkillBranch::Movement:
        return {91, 187, 235, 255};
    case SkillBranch::Root:
        return {199, 145, 240, 255};
    }
    return WHITE;
}

void drawGrowingCurve(Vector2 start, Vector2 end, float progress,
                      float thickness, Color color) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    if (progress <= 0.0F) {
        return;
    }
    DrawLineEx(start, Vector2Lerp(start, end, progress), thickness, color);
}

void drawWrappedText(std::string_view text, Vector2 position,
                     float fontSize, float maximumWidth,
                     Color color) {
    std::string line;
    float y = position.y;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find(' ', start);
        const std::string word{text.substr(
            start, end == std::string_view::npos
                       ? text.size() - start
                       : end - start)};
        const std::string candidate =
            line.empty() ? word : line + " " + word;
        if (!line.empty() &&
            measureUiText(candidate, fontSize).x > maximumWidth) {
            drawUiText(line, {position.x, y}, fontSize, color);
            y += fontSize * 1.32F;
            line = word;
        } else {
            line = candidate;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        drawUiText(line, {position.x, y}, fontSize, color);
    }
}

} // namespace

SkillTreeScreen::SkillTreeScreen(const SkillTree& tree)
    : tree_(&tree), reveal_(tree.nodes().size(), 0.0F),
      revealDelay_(tree.nodes().size(), 0.0F),
      connectionReveal_(tree.nodes().size(), 0.0F),
      connectionDelay_(tree.nodes().size(), 0.0F),
      hoverAmount_(tree.nodes().size(), 0.0F),
      pulse_(tree.nodes().size(), 0.0F),
      confirmationPulse_(tree.nodes().size(), 0.0F),
      rejectShake_(tree.nodes().size(), 0.0F) {}

void SkillTreeScreen::open() {
    open_ = true;
    opening_ = 0.0F;
    for (std::size_t index = 0; index < tree_->nodes().size(); ++index) {
        reveal_[index] = 0.0F;
        connectionReveal_[index] = 0.0F;
        hoverAmount_[index] = 0.0F;
        pulse_[index] = 0.0F;
        confirmationPulse_[index] = 0.0F;
        rejectShake_[index] = 0.0F;

        int depth = 0;
        const SkillNodeDefinition* node = &tree_->nodes()[index];
        while (!node->prerequisites.empty() && depth < 12) {
            const auto parent = tree_->indexOf(node->prerequisites.front());
            if (!parent) break;
            node = &tree_->nodes()[*parent];
            ++depth;
        }
        revealDelay_[index] = static_cast<float>(depth) * 0.24F;
        connectionDelay_[index] = std::max(0.0F, revealDelay_[index] - 0.16F);
    }
    if (!selected_) {
        selected_ = tree_->indexOf("bare_hands");
    }
}

void SkillTreeScreen::close() {
    open_ = false;
    dragging_ = false;
    hovered_.reset();
    confirmation_.reset();
}

bool SkillTreeScreen::isOpen() const {
    return open_;
}

void SkillTreeScreen::setUnlimitedPoints(bool unlimited) {
    unlimitedPoints_ = unlimited;
}

std::optional<std::size_t> SkillTreeScreen::update(float deltaSeconds) {
    if (!open_) {
        return std::nullopt;
    }
    deltaSeconds = std::clamp(deltaSeconds, 0.0F, 0.1F);
    opening_ += (1.0F - opening_) *
        (1.0F - std::exp(-11.0F * deltaSeconds));

    const float wheel = GetMouseWheelMove();
    targetZoom_ = std::clamp(
        targetZoom_ + wheel * 0.08F, 0.58F, 1.35F);
    zoom_ += (targetZoom_ - zoom_) *
        (1.0F - std::exp(-13.0F * deltaSeconds));
    camera_ = Vector2Lerp(
        camera_, targetCamera_,
        1.0F - std::exp(-12.0F * deltaSeconds));

    const Vector2 mouse = GetMousePosition();
    hovered_ = nodeAt(mouse);
    const bool beginDrag =
        IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ||
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered_);
    if (beginDrag) {
        dragging_ = true;
        previousMouse_ = mouse;
    }
    if (dragging_) {
        const bool held =
            IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (held) {
            const Vector2 delta = Vector2Subtract(mouse, previousMouse_);
            targetCamera_ = Vector2Add(
                targetCamera_, Vector2Scale(delta, 1.0F / zoom_));
            targetCamera_.x = std::clamp(targetCamera_.x, -620.0F, 620.0F);
            targetCamera_.y = std::clamp(targetCamera_.y, -460.0F, 460.0F);
            previousMouse_ = mouse;
        } else {
            dragging_ = false;
        }
    }

    std::optional<std::size_t> purchase;
    if (!dragging_ && hovered_ &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        selected_ = hovered_;
        if (tree_->state(*hovered_) == SkillNodeState::Available) {
            const int cost = tree_->nodes()[*hovered_].cost;
            if (!unlimitedPoints_ && tree_->points() < cost) {
                confirmation_.reset();
                rejectShake_[*hovered_] = 1.0F;
            } else if (confirmation_ == hovered_) {
                purchase = hovered_;
                confirmation_.reset();
                pulse_[*hovered_] = 1.0F;
            } else {
                confirmation_ = hovered_;
                confirmationPulse_[*hovered_] = 1.0F;
            }
        } else {
            confirmation_.reset();
        }
    }

    for (std::size_t index = 0; index < reveal_.size(); ++index) {
        const bool visible =
            tree_->state(index) != SkillNodeState::Hidden;
        if (revealDelay_[index] > 0.0F) {
            revealDelay_[index] = std::max(
                0.0F, revealDelay_[index] - deltaSeconds);
        } else {
            const float target = visible ? 1.0F : 0.0F;
            reveal_[index] += (target - reveal_[index]) *
                (1.0F - std::exp(-11.0F * deltaSeconds));
        }
        if (connectionDelay_[index] > 0.0F) {
            connectionDelay_[index] = std::max(
                0.0F, connectionDelay_[index] - deltaSeconds);
        } else {
            const float target = visible ? 1.0F : 0.0F;
            connectionReveal_[index] +=
                (target - connectionReveal_[index]) *
                (1.0F - std::exp(-8.0F * deltaSeconds));
        }
        const float hoverTarget = hovered_ == index ? 1.0F : 0.0F;
        hoverAmount_[index] += (hoverTarget - hoverAmount_[index]) *
            (1.0F - std::exp(-15.0F * deltaSeconds));
        pulse_[index] = std::max(
            0.0F, pulse_[index] - deltaSeconds * 1.25F);
        confirmationPulse_[index] = std::max(
            0.0F, confirmationPulse_[index] - deltaSeconds * 1.6F);
        rejectShake_[index] = std::max(
            0.0F, rejectShake_[index] - deltaSeconds * 3.5F);
    }
    return purchase;
}

void SkillTreeScreen::draw(const GameUi& ui) const {
    if (!open_) {
        return;
    }
    const float alpha = smoothStep(opening_);
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    DrawRectangle(0, 0, width, height,
                  withAlpha({24, 11, 5, 255}, 0.66F * alpha));

    DrawRectangleGradientV(
        0, 0, width, 112,
        withAlpha({4, 10, 11, 255}, 0.94F * alpha),
        withAlpha({4, 10, 11, 255}, 0.0F));
    drawCenteredUiText(
        "TREE OF KNOWLEDGE", 25.0F, 34.0F,
        withAlpha({238, 225, 190, 255}, alpha));
    const std::string pointsLabel = unlimitedPoints_
        ? "SKILL POINTS  INFINITE"
        : "SKILL POINTS  " + std::to_string(tree_->points());
    drawCenteredUiText(
        pointsLabel,
        68.0F, 14.0F,
        withAlpha({151, 181, 157, 255}, 0.9F * alpha));

    drawConnections();
    drawNodes();
    drawDetails(ui);

    drawUiText(
        "K / ESC  CLOSE", {22.0F, static_cast<float>(height) - 34.0F},
        14.0F, withAlpha({174, 190, 176, 255}, alpha));
    const std::string navigation = "DRAG  PAN     WHEEL  ZOOM";
    const float navigationWidth = measureUiText(navigation, 14.0F).x;
    drawUiText(
        navigation,
        {static_cast<float>(width) - navigationWidth - 22.0F,
         static_cast<float>(height) - 34.0F},
        14.0F, withAlpha({174, 190, 176, 255}, alpha));
}

const SkillTree& SkillTreeScreen::tree() const {
    return *tree_;
}

Vector2 SkillTreeScreen::worldToScreen(SkillTreePoint point) const {
    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    return {
        center.x + (point.x + camera_.x) * zoom_,
        center.y + (point.y + camera_.y) * zoom_,
    };
}

std::optional<std::size_t> SkillTreeScreen::nodeAt(
    Vector2 screenPosition) const {
    for (std::size_t reverse = tree_->nodes().size(); reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        if (reveal_[index] < 0.25F ||
            tree_->state(index) == SkillNodeState::Hidden) {
            continue;
        }
        const float halfSize =
            nodeHalfSize(tree_->nodes()[index]) * zoom_;
        const Vector2 center = worldToScreen(tree_->nodes()[index].position);
        if (CheckCollisionPointRec(
                screenPosition,
                {center.x - halfSize, center.y - halfSize,
                 halfSize * 2.0F, halfSize * 2.0F})) {
            return index;
        }
    }
    return std::nullopt;
}

void SkillTreeScreen::drawConnections() const {
    const float time = static_cast<float>(GetTime());
    for (std::size_t child = 0; child < tree_->nodes().size(); ++child) {
        if (connectionReveal_[child] <= 0.01F) {
            continue;
        }
        const auto& node = tree_->nodes()[child];
        for (const std::string& prerequisite : node.prerequisites) {
            const auto parent = tree_->indexOf(prerequisite);
            if (!parent) {
                continue;
            }
            const Vector2 start = worldToScreen(
                tree_->nodes()[*parent].position);
            const Vector2 end = worldToScreen(node.position);
            const float progress = smoothStep(connectionReveal_[child]);
            const bool active =
                tree_->state(child) == SkillNodeState::Unlocked ||
                tree_->state(child) == SkillNodeState::Available;
            drawGrowingCurve(
                start, end, progress, 7.0F * zoom_,
                withAlpha({13, 10, 11, 255}, 0.92F));
            drawGrowingCurve(
                start, end, progress, 2.5F * zoom_,
                active
                    ? withAlpha({246, 184, 58, 255}, 0.92F)
                    : withAlpha({115, 112, 109, 255}, 0.36F));

            if (active && progress > 0.12F) {
                const float travel = std::fmod(
                    time * 0.38F + static_cast<float>(child) * 0.23F,
                    std::max(progress, 0.001F));
                const Vector2 spark = Vector2Lerp(start, end, travel);
                DrawCircleV(spark, 5.0F * zoom_,
                            withAlpha({246, 184, 58, 255}, 0.12F));
                DrawCircleV(spark, 2.0F * zoom_,
                            withAlpha({255, 231, 155, 255}, 0.9F));
            }

        }
    }
}

void SkillTreeScreen::drawNodes() const {
    const double time = GetTime();
    for (std::size_t index = 0; index < tree_->nodes().size(); ++index) {
        if (reveal_[index] <= 0.01F ||
            tree_->state(index) == SkillNodeState::Hidden) {
            continue;
        }
        const auto& node = tree_->nodes()[index];
        const SkillNodeState state = tree_->state(index);
        Vector2 center = worldToScreen(node.position);
        if (rejectShake_[index] > 0.0F) {
            center.x += std::sin(rejectShake_[index] * 31.0F) *
                        rejectShake_[index] * 8.0F;
        }
        const float baseHalfSize = nodeHalfSize(node);
        const float revealScale = easeOutBack(reveal_[index]);
        const float hoverScale = 1.0F + smoothStep(hoverAmount_[index]) * 0.14F;
        const float pulseScale =
            1.0F + std::sin(pulse_[index] * PI) * 0.34F;
        const float halfSize = baseHalfSize * zoom_ * revealScale *
                               hoverScale * pulseScale;
        const Rectangle nodeBounds{
            center.x - halfSize, center.y - halfSize,
            halfSize * 2.0F, halfSize * 2.0F};
        const Color color = branchColor(node.branch);
        const bool active = state == SkillNodeState::Unlocked ||
                            state == SkillNodeState::Available;
        if (node.branch == SkillBranch::Root) {
            const float wave = std::fmod(
                static_cast<float>(time) * 0.34F, 1.0F);
            DrawRing(center,
                     (baseHalfSize + 7.0F + wave * 24.0F) * zoom_,
                     (baseHalfSize + 9.0F + wave * 24.0F) * zoom_,
                     0.0F, 360.0F, 32,
                     withAlpha(color, (1.0F - wave) * 0.2F * reveal_[index]));
        }
        if (active) {
            const float breath = 0.5F + 0.5F * std::sin(
                static_cast<float>(time) * 2.2F +
                static_cast<float>(index));
            const float glow = (8.0F + breath * 6.0F) * zoom_;
            DrawRectangleRounded(
                {nodeBounds.x - glow, nodeBounds.y - glow,
                 nodeBounds.width + glow * 2.0F,
                 nodeBounds.height + glow * 2.0F},
                0.18F, 6, withAlpha(color, 0.06F + breath * 0.05F));
        }
        DrawRectangleRounded(
            {nodeBounds.x - 4.0F * zoom_, nodeBounds.y - 4.0F * zoom_,
             nodeBounds.width + 8.0F * zoom_,
             nodeBounds.height + 8.0F * zoom_},
            0.18F, 6, withAlpha({18, 14, 17, 255}, 0.98F));
        DrawRectangleRounded(
            nodeBounds,
            0.18F, 6,
            state == SkillNodeState::Unlocked
                ? withAlpha(color, 0.68F)
                : state == SkillNodeState::Available
                      ? withAlpha({72, 54, 47, 255}, 0.98F)
                      : withAlpha({24, 24, 29, 255}, 0.96F));
        DrawRectangleRoundedLinesEx(
            nodeBounds, 0.18F, 6, 2.5F * zoom_,
            state == SkillNodeState::Locked
                ? withAlpha({91, 88, 91, 255}, 0.35F)
                : withAlpha({231, 188, 111, 255}, 0.9F));
        if (state == SkillNodeState::Available) {
            const float pulse = 0.55F + 0.45F * std::sin(
                static_cast<float>(time) * 3.2F);
            DrawRectangleRoundedLinesEx(
                {nodeBounds.x - 7.0F * zoom_,
                 nodeBounds.y - 7.0F * zoom_,
                 nodeBounds.width + 14.0F * zoom_,
                 nodeBounds.height + 14.0F * zoom_},
                0.18F, 6, 2.0F * zoom_,
                withAlpha({246, 184, 58, 255}, pulse));
        }

        if (hoverAmount_[index] > 0.02F) {
            const float orbitRadius = (baseHalfSize + 14.0F) * zoom_ * hoverScale;
            const float orbitAlpha = smoothStep(hoverAmount_[index]);
            for (int marker = 0; marker < 4; ++marker) {
                const float angle = static_cast<float>(time) * 0.9F +
                    static_cast<float>(marker) * PI * 0.5F;
                const Vector2 markerPosition{
                    center.x + std::cos(angle) * orbitRadius,
                    center.y + std::sin(angle) * orbitRadius,
                };
                DrawCircleV(markerPosition, 2.5F * zoom_,
                            withAlpha(color, orbitAlpha * 0.82F));
            }
        }
        if (confirmationPulse_[index] > 0.0F) {
            const float progress = 1.0F - confirmationPulse_[index];
            const float radius = (baseHalfSize + 8.0F + progress * 34.0F) * zoom_;
            DrawRing(center, radius, radius + 2.5F * zoom_,
                     0.0F, 360.0F, 32,
                     withAlpha({255, 214, 91, 255}, confirmationPulse_[index] * 0.9F));
        }

        if (node.icon == "ice_wand") {
            const Color iconColor = withAlpha(
                {142, 229, 255, 255},
                state == SkillNodeState::Locked ? 0.32F : 0.98F);
            const float orbRadius = 9.0F * zoom_;
            DrawCircleV(center, orbRadius,
                        withAlpha({33, 140, 218, 255},
                                  state == SkillNodeState::Locked ? 0.25F : 0.66F));
            DrawCircleLines(static_cast<int>(center.x),
                            static_cast<int>(center.y),
                            orbRadius + 2.0F * zoom_, iconColor);
            DrawLineEx(
                {center.x - 16.0F * zoom_, center.y + 13.0F * zoom_},
                {center.x + 7.0F * zoom_, center.y - 9.0F * zoom_},
                4.0F * zoom_, iconColor);
            DrawLineEx(
                {center.x + 4.0F * zoom_, center.y - 12.0F * zoom_},
                {center.x + 11.0F * zoom_, center.y - 4.0F * zoom_},
                3.0F * zoom_, iconColor);
        } else {
            const char* glyph = state == SkillNodeState::Unlocked
                                    ? "✓"
                                    : state == SkillNodeState::Available
                                          ? "+"
                                          : "·";
            const float glyphSize = 27.0F * zoom_;
            const Vector2 glyphMeasure = measureUiText(glyph, glyphSize);
            drawUiText(
                glyph,
                {center.x - glyphMeasure.x * 0.5F,
                 center.y - glyphMeasure.y * 0.5F - 1.0F},
                glyphSize,
                withAlpha(RAYWHITE,
                          state == SkillNodeState::Locked ? 0.32F : 0.98F));
        }

        const float labelSize = std::clamp(13.0F * zoom_, 10.0F, 17.0F);
        const Vector2 labelMeasure = measureUiText(node.title, labelSize);
        drawUiText(
            node.title,
            {center.x - labelMeasure.x * 0.5F,
             center.y + halfSize + 11.0F * zoom_},
            labelSize,
            withAlpha(state == SkillNodeState::Locked
                          ? Color{130, 135, 132, 255}
                          : Color{234, 226, 205, 255},
                      reveal_[index]));
    }
}

void SkillTreeScreen::drawDetails(const GameUi& ui) const {
    const std::optional<std::size_t> details = hovered_ ? hovered_ : selected_;
    if (!details || *details >= tree_->nodes().size()) {
        return;
    }
    const auto& node = tree_->nodes()[*details];
    const SkillNodeState state = tree_->state(*details);
    constexpr float Width = 332.0F;
    constexpr float Height = 198.0F;
    const Rectangle bounds{
        static_cast<float>(GetScreenWidth()) - Width - 22.0F,
        104.0F, Width, Height,
    };
    ui.drawPanel(bounds, 241);
    ui.drawInsetPanel(
        {bounds.x + 13.0F, bounds.y + 13.0F,
         bounds.width - 26.0F, 31.0F}, 220);
    drawUiText(
        skillBranchName(node.branch),
        {bounds.x + 25.0F, bounds.y + 19.0F}, 13.0F,
        branchColor(node.branch));
    drawUiText(
        node.title, {bounds.x + 22.0F, bounds.y + 58.0F},
        23.0F, {245, 228, 193, 255});
    drawWrappedText(
        node.description,
        {bounds.x + 22.0F, bounds.y + 94.0F}, 15.0F,
        bounds.width - 44.0F, {199, 194, 181, 255});

    std::string status;
    Color statusColor{145, 151, 148, 255};
    if (state == SkillNodeState::Unlocked) {
        status = "UNLOCKED  •  " + node.icon;
        statusColor = branchColor(node.branch);
    } else if (state == SkillNodeState::Available) {
        status = confirmation_ == details
            ? "CLICK AGAIN TO CONFIRM  •  COST " + std::to_string(node.cost)
            : "CLICK TO BUY  •  COST " + std::to_string(node.cost);
        if (!unlimitedPoints_ && tree_->points() < node.cost)
            status += "  •  NOT ENOUGH POINTS";
        statusColor = {236, 205, 120, 255};
    } else {
        status = "LOCKED  •  DISCOVER PREVIOUS LEAVES";
    }
    drawUiText(
        status,
        {bounds.x + 22.0F, bounds.y + bounds.height - 35.0F},
        13.0F, statusColor);
}

} // namespace ian
