#include "ui/SkillTreeScreen.hpp"

#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace ian {
namespace {

constexpr float NodeRadius = 38.0F;

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
    case SkillBranch::Defenses:
        return {102, 198, 142, 255};
    case SkillBranch::Weapons:
        return {224, 104, 92, 255};
    case SkillBranch::Survival:
        return {116, 177, 225, 255};
    case SkillBranch::Root:
        return {199, 145, 240, 255};
    }
    return WHITE;
}

Vector2 cubicPoint(Vector2 start, Vector2 controlA,
                   Vector2 controlB, Vector2 end, float time) {
    const float inverse = 1.0F - time;
    return {
        inverse * inverse * inverse * start.x +
            3.0F * inverse * inverse * time * controlA.x +
            3.0F * inverse * time * time * controlB.x +
            time * time * time * end.x,
        inverse * inverse * inverse * start.y +
            3.0F * inverse * inverse * time * controlA.y +
            3.0F * inverse * time * time * controlB.y +
            time * time * time * end.y,
    };
}

void drawGrowingCurve(Vector2 start, Vector2 end, float progress,
                      float thickness, Color color) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    if (progress <= 0.0F) {
        return;
    }
    const float bend = (end.y - start.y) * 0.46F;
    const Vector2 controlA{start.x, start.y + bend};
    const Vector2 controlB{end.x, end.y - bend};
    constexpr int Segments = 32;
    Vector2 previous = start;
    const int visibleSegments = std::max(
        1, static_cast<int>(
               std::ceil(progress * static_cast<float>(Segments))));
    for (int segment = 1; segment <= visibleSegments; ++segment) {
        const float time = std::min(
            progress,
            static_cast<float>(segment) /
                static_cast<float>(Segments));
        const Vector2 current = cubicPoint(
            start, controlA, controlB, end, time);
        DrawLineEx(previous, current, thickness, color);
        previous = current;
    }
}

void drawLeaf(Vector2 position, float angle, float scale,
              Color color) {
    if (scale <= 0.0F) {
        return;
    }
    const Vector2 direction{std::cos(angle), std::sin(angle)};
    const Vector2 side{-direction.y, direction.x};
    const Vector2 tip = Vector2Add(
        position, Vector2Scale(direction, 13.0F * scale));
    const Vector2 base = Vector2Subtract(
        position, Vector2Scale(direction, 4.0F * scale));
    const Vector2 left = Vector2Add(
        position, Vector2Scale(side, 6.0F * scale));
    const Vector2 right = Vector2Subtract(
        position, Vector2Scale(side, 6.0F * scale));
    DrawTriangle(tip, left, base, color);
    DrawTriangle(tip, base, right, color);
    DrawLineEx(base, tip, std::max(1.0F, scale),
               withAlpha(RAYWHITE, 0.2F));
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

SkillTreeScreen::SkillTreeScreen()
    : reveal_(tree_.nodes().size(), 0.0F),
      revealDelay_(tree_.nodes().size(), 0.0F),
      pulse_(tree_.nodes().size(), 0.0F) {}

void SkillTreeScreen::open() {
    open_ = true;
    opening_ = 0.0F;
    for (std::size_t index = 0; index < tree_.nodes().size(); ++index) {
        if (tree_.state(index) != SkillNodeState::Hidden) {
            reveal_[index] = 1.0F;
        }
    }
    if (!selected_) {
        selected_ = tree_.indexOf("core");
    }
}

void SkillTreeScreen::close() {
    open_ = false;
    dragging_ = false;
    hovered_.reset();
}

bool SkillTreeScreen::isOpen() const {
    return open_;
}

bool SkillTreeScreen::update(float deltaSeconds) {
    if (!open_) {
        return false;
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

    bool unlocked = false;
    if (!dragging_ && hovered_ &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        selected_ = hovered_;
        if (tree_.unlock(*hovered_)) {
            unlocked = true;
            pulse_[*hovered_] = 1.0F;
            const auto children = tree_.childrenOf(
                tree_.nodes()[*hovered_].id);
            float delay = 0.04F;
            for (const std::size_t child : children) {
                reveal_[child] = 0.0F;
                revealDelay_[child] = delay;
                delay += 0.1F;
            }
        }
    }

    for (std::size_t index = 0; index < reveal_.size(); ++index) {
        const bool visible =
            tree_.state(index) != SkillNodeState::Hidden;
        if (revealDelay_[index] > 0.0F) {
            revealDelay_[index] = std::max(
                0.0F, revealDelay_[index] - deltaSeconds);
        } else {
            const float target = visible ? 1.0F : 0.0F;
            reveal_[index] += (target - reveal_[index]) *
                (1.0F - std::exp(-8.5F * deltaSeconds));
        }
        pulse_[index] = std::max(
            0.0F, pulse_[index] - deltaSeconds * 1.25F);
    }
    return unlocked;
}

void SkillTreeScreen::draw(const GameUi& ui) const {
    if (!open_) {
        return;
    }
    const float alpha = smoothStep(opening_);
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    DrawRectangleGradientV(
        0, 0, width, height,
        withAlpha({7, 17, 18, 255}, alpha),
        withAlpha({19, 12, 25, 255}, alpha));

    const double time = GetTime();
    for (int mote = 0; mote < 48; ++mote) {
        const float seed = static_cast<float>(mote);
        const float x = std::fmod(
            seed * 173.3F + static_cast<float>(time) *
                (4.0F + std::fmod(seed, 5.0F)),
            static_cast<float>(std::max(width, 1)));
        const float y = std::fmod(
            seed * 91.7F + std::sin(
                static_cast<float>(time) * 0.4F + seed) * 24.0F,
            static_cast<float>(std::max(height, 1)));
        DrawCircleV({x, y}, 1.0F + std::fmod(seed, 3.0F) * 0.45F,
                    withAlpha({166, 222, 166, 255}, 0.12F * alpha));
    }

    DrawRectangleGradientV(
        0, 0, width, 112,
        withAlpha({4, 10, 11, 255}, 0.94F * alpha),
        withAlpha({4, 10, 11, 255}, 0.0F));
    drawCenteredUiText(
        "TREE OF KNOWLEDGE", 25.0F, 34.0F,
        withAlpha({238, 225, 190, 255}, alpha));
    drawCenteredUiText(
        "PROGRESSION FRAMEWORK  •  NO GAMEPLAY EFFECTS YET",
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
    return tree_;
}

Vector2 SkillTreeScreen::worldToScreen(SkillTreePoint point) const {
    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.55F,
    };
    return {
        center.x + (point.x + camera_.x) * zoom_,
        center.y + (point.y + camera_.y) * zoom_,
    };
}

std::optional<std::size_t> SkillTreeScreen::nodeAt(
    Vector2 screenPosition) const {
    for (std::size_t reverse = tree_.nodes().size(); reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        if (reveal_[index] < 0.25F ||
            tree_.state(index) == SkillNodeState::Hidden) {
            continue;
        }
        const float radius =
            (tree_.nodes()[index].branch == SkillBranch::Root
                 ? 51.0F
                 : NodeRadius) *
            zoom_;
        if (Vector2Distance(
                worldToScreen(tree_.nodes()[index].position),
                screenPosition) <= radius) {
            return index;
        }
    }
    return std::nullopt;
}

void SkillTreeScreen::drawConnections() const {
    for (std::size_t child = 0; child < tree_.nodes().size(); ++child) {
        if (reveal_[child] <= 0.01F) {
            continue;
        }
        const auto& node = tree_.nodes()[child];
        const Color color = branchColor(node.branch);
        for (const std::string& prerequisite : node.prerequisites) {
            const auto parent = tree_.indexOf(prerequisite);
            if (!parent) {
                continue;
            }
            const Vector2 start = worldToScreen(
                tree_.nodes()[*parent].position);
            const Vector2 end = worldToScreen(node.position);
            const float progress = smoothStep(reveal_[child]);
            const bool active =
                tree_.state(child) == SkillNodeState::Unlocked ||
                tree_.state(child) == SkillNodeState::Available;
            drawGrowingCurve(
                start, end, progress, 8.0F * zoom_,
                withAlpha({17, 32, 25, 255}, 0.9F));
            drawGrowingCurve(
                start, end, progress, 2.5F * zoom_,
                withAlpha(color, active ? 0.82F : 0.22F));

            constexpr std::array<float, 4> LeafTimes{
                0.25F, 0.44F, 0.64F, 0.82F};
            const float bend = (end.y - start.y) * 0.46F;
            const Vector2 controlA{start.x, start.y + bend};
            const Vector2 controlB{end.x, end.y - bend};
            for (std::size_t leaf = 0; leaf < LeafTimes.size(); ++leaf) {
                const float leafTime = LeafTimes[leaf];
                const float leafReveal = smoothStep(
                    (progress - leafTime) * 8.0F);
                const Vector2 position = cubicPoint(
                    start, controlA, controlB, end, leafTime);
                drawLeaf(
                    position,
                    leaf % 2U == 0U ? -0.55F : 3.7F,
                    leafReveal * zoom_ * 0.72F,
                    withAlpha(color, active ? 0.72F : 0.16F));
            }
        }
    }
}

void SkillTreeScreen::drawNodes() const {
    const double time = GetTime();
    for (std::size_t index = 0; index < tree_.nodes().size(); ++index) {
        if (reveal_[index] <= 0.01F ||
            tree_.state(index) == SkillNodeState::Hidden) {
            continue;
        }
        const auto& node = tree_.nodes()[index];
        const SkillNodeState state = tree_.state(index);
        const Vector2 center = worldToScreen(node.position);
        const float baseRadius =
            node.branch == SkillBranch::Root ? 51.0F : NodeRadius;
        const float revealScale = easeOutBack(reveal_[index]);
        const float hoverScale = hovered_ == index ? 1.08F : 1.0F;
        const float pulseScale =
            1.0F + std::sin(pulse_[index] * PI) * 0.34F;
        const float radius = baseRadius * zoom_ * revealScale *
                             hoverScale * pulseScale;
        const Color color = branchColor(node.branch);
        const bool active = state == SkillNodeState::Unlocked ||
                            state == SkillNodeState::Available;
        if (active) {
            const float breath = 0.5F + 0.5F * std::sin(
                static_cast<float>(time) * 2.2F +
                static_cast<float>(index));
            DrawCircleV(center, radius + (8.0F + breath * 6.0F) * zoom_,
                        withAlpha(color, 0.06F + breath * 0.05F));
        }
        if (state == SkillNodeState::Unlocked) {
            constexpr int Petals = 7;
            for (int petal = 0; petal < Petals; ++petal) {
                const float angle =
                    static_cast<float>(petal) * 2.0F * PI /
                    static_cast<float>(Petals) - 1.57F;
                const Vector2 leafPosition = Vector2Add(
                    center,
                    {std::cos(angle) * radius * 0.96F,
                     std::sin(angle) * radius * 0.96F});
                drawLeaf(leafPosition, angle, zoom_ * 0.75F,
                         withAlpha(color, 0.86F));
            }
        }
        DrawCircleV(center, radius + 4.0F * zoom_,
                    withAlpha({9, 19, 17, 255}, 0.96F));
        DrawCircleV(
            center, radius,
            state == SkillNodeState::Unlocked
                ? withAlpha(color, 0.92F)
                : state == SkillNodeState::Available
                      ? withAlpha({40, 58, 44, 255}, 0.98F)
                      : withAlpha({28, 30, 31, 255}, 0.94F));
        DrawRing(
            center, radius - 3.0F * zoom_, radius,
            0.0F, 360.0F, 48,
            withAlpha(color,
                      state == SkillNodeState::Locked ? 0.22F : 0.95F));
        if (state == SkillNodeState::Available) {
            const float arc = std::fmod(
                static_cast<float>(time) * 55.0F, 360.0F);
            DrawRing(center, radius + 4.0F * zoom_,
                     radius + 7.0F * zoom_, arc, arc + 88.0F,
                     24, withAlpha(color, 0.92F));
        }

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

        const float labelSize = std::clamp(13.0F * zoom_, 10.0F, 17.0F);
        const Vector2 labelMeasure = measureUiText(node.title, labelSize);
        drawUiText(
            node.title,
            {center.x - labelMeasure.x * 0.5F,
             center.y + radius + 11.0F * zoom_},
            labelSize,
            withAlpha(state == SkillNodeState::Locked
                          ? Color{130, 135, 132, 255}
                          : Color{234, 226, 205, 255},
                      reveal_[index]));
    }
}

void SkillTreeScreen::drawDetails(const GameUi& ui) const {
    if (!selected_ || *selected_ >= tree_.nodes().size()) {
        return;
    }
    const auto& node = tree_.nodes()[*selected_];
    const SkillNodeState state = tree_.state(*selected_);
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
        status = "UNLOCKED  •  PLACEHOLDER";
        statusColor = branchColor(node.branch);
    } else if (state == SkillNodeState::Available) {
        status = "CLICK TO DISCOVER  •  COST " +
                 std::to_string(node.cost);
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
