#include "graphics/GraphicsResources.hpp"

#include <raylib.h>

#include <iostream>

namespace {

bool checkMissingResources() {
    ian::ShaderResource shader;
    ian::TextureResource texture;
    ian::ModelResource model;
    ian::ModelAnimationsResource animations;
    ian::RenderTextureResource target;

    const bool rejected =
        !shader.load(nullptr, "assets/missing/not_found.fs") &&
        !texture.load("assets/missing/not_found.png") &&
        !model.load("assets/missing/not_found.glb") &&
        !animations.load("assets/missing/not_found.glb") &&
        !target.load(0, 64);
    shader.unload();
    shader.unload();
    texture.unload();
    texture.unload();
    model.unload();
    model.unload();
    animations.unload();
    animations.unload();
    target.unload();
    target.unload();
    return rejected;
}

} // namespace

int main() {
    std::cerr << "graphics smoke: create context\n";
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "IAN graphics resource smoke");
    if (!IsWindowReady()) {
        std::cerr << "graphics context unavailable; skipping\n";
        return 77;
    }

    int result = 0;
    {
        std::cerr << "graphics smoke: missing resources\n";
        if (!checkMissingResources()) {
            std::cerr << "missing resource fallback failed\n";
            result = 1;
        }

        ian::GraphicsSettings settings;
        settings.shadowMapSize = 64;
        settings.pixelSize = 2;
        ian::GraphicsResources resources;
        std::cerr << "graphics smoke: initialize 1\n";
        resources.initialize(settings);
        if (!resources.sceneTargetValid() ||
            !resources.selectionMaskValid() ||
            !resources.viewModelTargetValid() ||
            !resources.clubModel().valid() ||
            !resources.hammerModel().valid()) {
            std::cerr << "required graphics resource failed to load\n";
            result = 1;
        }
        resources.shutdown();
        resources.shutdown();
        std::cerr << "graphics smoke: initialize 2\n";
        resources.initialize(settings);
        resources.shutdown();
    }
    std::cerr << "graphics smoke: close context\n";
    CloseWindow();
    return result;
}
