#pragma once
#include <raylib.h>
namespace ian {
struct WorldUiOcclusion {
    bool nativeDepth{};
    Shader shader{};
    Texture2D depth{};
    Vector2 viewport{};
    int depthLocation{-1};
    int viewportLocation{-1};
    [[nodiscard]] bool valid() const { return shader.id != 0 && depth.id != 0; }
};
} // namespace ian
