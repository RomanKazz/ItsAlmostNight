#pragma once

#include <raylib.h>

namespace ian {

namespace ui_render_layer_detail {
inline thread_local const RenderTexture2D* current{};
}

// Text and minimap caches end their own texture pass before returning to the
// HUD. Raylib returns to the window by default; restore the HUD layer instead.
inline void restoreUiRenderLayer() {
    if (ui_render_layer_detail::current) {
        BeginTextureMode(*ui_render_layer_detail::current);
    }
}

class ScopedUiRenderLayer {
public:
    explicit ScopedUiRenderLayer(RenderTexture2D target)
        : target_(target), previous_(ui_render_layer_detail::current) {
        ui_render_layer_detail::current = &target_;
        BeginTextureMode(target_);
        ClearBackground(BLANK);
    }
    ~ScopedUiRenderLayer() {
        EndTextureMode();
        ui_render_layer_detail::current = previous_;
        restoreUiRenderLayer();
    }
    ScopedUiRenderLayer(const ScopedUiRenderLayer&) = delete;
    ScopedUiRenderLayer& operator=(const ScopedUiRenderLayer&) = delete;

private:
    RenderTexture2D target_;
    const RenderTexture2D* previous_;
};

} // namespace ian
