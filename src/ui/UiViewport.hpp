#pragma once

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace ian {

// A uniform UI scale keeps pixel-authored controls usable on small windows.
// The canvas has the window's aspect ratio: edge anchors still reach the real
// screen edges, with no stretching, cropping or letterboxed input coordinates.
struct UiViewport {
    Vector2 size{};
    float scale{1.0F};

    static UiViewport fromScreen(Vector2 screen) {
        screen.x = std::max(1.0F, screen.x);
        screen.y = std::max(1.0F, screen.y);
        const float upscale = std::max(1.0F,
            std::min(screen.x / 1920.0F, screen.y / 1080.0F));
        const float scale = std::min({screen.x / 1600.0F,
            screen.y / 900.0F, upscale});
        return {{screen.x / scale, screen.y / scale}, scale};
    }
    Vector2 toUi(Vector2 point) const { return {point.x / scale, point.y / scale}; }
    Rectangle toScreen(Rectangle rect) const {
        return {rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale};
    }
};

namespace ui_viewport_detail {
inline thread_local const UiViewport* current{};
inline thread_local bool drawing{};
inline Camera2D camera() { return {{}, {}, 0.0F, current->scale}; }
inline void beginCamera() {
    BeginMode2D(camera());
    // BeginMode2D resets raylib's native backing-pixel transform. Restore it
    // for Retina/HiDPI output; texture targets already use their own pixels.
    if (rlGetActiveFramebuffer() == 0U) {
        rlScalef(static_cast<float>(GetRenderWidth()) / static_cast<float>(std::max(GetScreenWidth(), 1)),
            static_cast<float>(GetRenderHeight()) / static_cast<float>(std::max(GetScreenHeight(), 1)), 1.0F);
    }
}
}

inline int uiScreenWidth() {
    return ui_viewport_detail::current
        ? static_cast<int>(std::lround(ui_viewport_detail::current->size.x)) : GetScreenWidth();
}
inline int uiScreenHeight() {
    return ui_viewport_detail::current
        ? static_cast<int>(std::lround(ui_viewport_detail::current->size.y)) : GetScreenHeight();
}
inline Vector2 uiMousePosition() {
    const Vector2 mouse = GetMousePosition();
    return ui_viewport_detail::current ? ui_viewport_detail::current->toUi(mouse) : mouse;
}
inline Vector2 uiWorldToScreen(Vector3 position, Camera3D camera) {
    const Vector2 point = GetWorldToScreen(position, camera);
    return ui_viewport_detail::current ? ui_viewport_detail::current->toUi(point) : point;
}
inline void beginUiScissor(int x, int y, int width, int height) {
    const float scale = ui_viewport_detail::current ? ui_viewport_detail::current->scale : 1.0F;
    const int left = static_cast<int>(std::floor(static_cast<float>(x) * scale));
    const int top = static_cast<int>(std::floor(static_cast<float>(y) * scale));
    BeginScissorMode(left, top,
        static_cast<int>(std::ceil(static_cast<float>(x + width) * scale)) - left,
        static_cast<int>(std::ceil(static_cast<float>(y + height) * scale)) - top);
}

// Geometry and input can use the same canvas without touching the renderer.
class ScopedUiCoordinates {
public:
    ScopedUiCoordinates() : viewport_(UiViewport::fromScreen({
        static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())})),
        owns_(!ui_viewport_detail::current) {
        if (owns_) ui_viewport_detail::current = &viewport_;
    }
    ~ScopedUiCoordinates() { if (owns_) ui_viewport_detail::current = nullptr; }
    ScopedUiCoordinates(const ScopedUiCoordinates&) = delete;
    ScopedUiCoordinates& operator=(const ScopedUiCoordinates&) = delete;
    bool owns() const { return owns_; }
    const UiViewport& viewport() const { return *ui_viewport_detail::current; }
private:
    UiViewport viewport_;
    bool owns_;
};

class ScopedUiCanvas {
public:
    ScopedUiCanvas() : owns_(!ui_viewport_detail::drawing) {
        if (owns_) {
            ui_viewport_detail::beginCamera();
            ui_viewport_detail::drawing = true;
        }
    }
    ~ScopedUiCanvas() {
        if (owns_) {
            EndMode2D();
            ui_viewport_detail::drawing = false;
        }
    }
    ScopedUiCanvas(const ScopedUiCanvas&) = delete;
    ScopedUiCanvas& operator=(const ScopedUiCanvas&) = delete;
    bool owns() const { return coordinates_.owns(); }
    const UiViewport& viewport() const { return coordinates_.viewport(); }
private:
    ScopedUiCoordinates coordinates_;
    bool owns_;
};

// Cache textures are authored in their own coordinates, then composited onto
// the UI canvas. Texture-mode transitions reset raylib's 2D camera matrix.
inline void suspendUiCanvas() {
    if (ui_viewport_detail::drawing) EndMode2D();
}
inline void resumeUiCanvas() {
    if (ui_viewport_detail::drawing) ui_viewport_detail::beginCamera();
}

} // namespace ian
