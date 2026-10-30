#include "TestHarness.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiMenuStyle.hpp"

#include <algorithm>
#include <array>
#include <cmath>

int main() {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 360, "Solid UI shapes with MSAA");
    if (!IsWindowReady()) return 77;
    GameUi ui;
    ui.initialize();
    constexpr std::array<Rectangle, 3> bars{{
        {40.25F, 80.25F, 1180, 6}, {40.25F, 120.25F, 1180, 8},
        {40.25F, 160.25F, 1180, 18},
    }};
    for (int frame = 0; frame < 3; ++frame) {
        BeginDrawing();
        ClearBackground(menuStyle::Card);
        menuStyle::text("PROGRESS", {40, 16, 300, 50}, 24, menuStyle::Cream, UiFontRole::Display);
        for (const auto bounds : bars) menuStyle::progress(bounds, 0.45);
        EndDrawing();
    }
    Image capture = LoadImageFromScreen();
    const float scaleX = static_cast<float>(capture.width) / static_cast<float>(GetScreenWidth());
    const float scaleY = static_cast<float>(capture.height) / static_cast<float>(GetScreenHeight());
    for (const auto bounds : bars) {
        const float stroke = bounds.height * 0.15F;
        const float fillEnd = bounds.x + stroke + (bounds.width - 2 * stroke) * 0.45F;
        for (int y = static_cast<int>(std::ceil((bounds.y + bounds.height * 0.4F) * scaleY));
             y <= static_cast<int>((bounds.y + bounds.height * 0.6F) * scaleY); ++y) {
            for (int x = static_cast<int>((bounds.x + 10) * scaleX);
                 x < static_cast<int>((bounds.x + bounds.width - 10) * scaleX); ++x) {
                if (std::abs(static_cast<float>(x) / scaleX - fillEnd) < 10) continue;
                const Color expected = static_cast<float>(x) / scaleX < fillEnd
                    ? menuStyle::Green : Color{174, 179, 173, 255};
                const Color actual = GetImageColor(capture, x, y);
                if (std::abs(static_cast<int>(actual.r) - expected.r) > 2 ||
                    std::abs(static_cast<int>(actual.g) - expected.g) > 2 ||
                    std::abs(static_cast<int>(actual.b) - expected.b) > 2) {
                    ExportImage(capture, "/tmp/ian-solid-shapes-failure.png");
                    TraceLog(LOG_ERROR, "Bar height %g: pixel %d,%d is %d,%d,%d; expected %d,%d,%d",
                        bounds.height, x, y, actual.r, actual.g, actual.b, expected.r, expected.g, expected.b);
                    require(false, "MSAA progress bars have a uniform solid interior with no font-atlas artifacts");
                }
            }
        }
    }
    ExportImage(capture, "/tmp/ian-solid-shapes-msaa.png");
    UnloadImage(capture);
    ui.shutdown();
    shutdownUiText();
    CloseWindow();
}
