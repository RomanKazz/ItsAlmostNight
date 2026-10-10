#include "graphics/Renderer.hpp"
#include "graphics/ViewModelCompositeBounds.hpp"
#include "graphics/CameraCulling.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <iostream>

int main() {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Render performance regressions");
    if (!IsWindowReady()) return 77;
    SetWindowPosition(0, 0); SetWindowSize(640, 360); SetTargetFPS(0);
    bool valid = true;
    const auto crop = viewModelCompositeBounds({{-.5F,-.5F,-.5F},{.5F,.5F,.5F}}, MatrixIdentity(), 640, 360, 4);
    valid = crop && crop->x == 156 && crop->y == 86 && crop->width == 328 && crop->height == 188;
    valid = valid && !viewModelCompositeBounds({{-1,-1,-2},{1,1,0}}, MatrixIdentity(), 640, 360, 4);
    const Camera3D camera{{0,0,0},{0,0,-1},{0,1,0},60,CAMERA_PERSPECTIVE};
    const camera_culling::Frustum frustum(MatrixMultiply(GetCameraMatrix(camera),
        MatrixPerspective(60.0 * DEG2RAD, 16.0 / 9.0, .01, 100)));
    valid = valid && frustum.intersects({{-1,-1,-4},{1,1,-2}}) &&
        !frustum.intersects({{-1,-1,2},{1,1,4}}) &&
        frustum.intersects({{-100,-100,-100},{100,100,100}});
    Renderer renderer; renderer.initialize();
    renderer.settings().frameRateLimit = 0; renderer.applyFrameRateLimit();
    renderer.setGpuProfilingEnabled(true);
    std::size_t issued = 0;
    for (int frame = 0; frame < 192; ++frame) {
        renderer.beginFrame(); BeginDrawing(); ClearBackground(BLACK);
        for (std::size_t stage = 0; stage < static_cast<std::size_t>(RendererGpuStage::Count); ++stage) {
            const auto gpuStage = static_cast<RendererGpuStage>(stage);
            renderer.beginGpuTimer(gpuStage);
            DrawRectangle(0, 0, 32, 32, WHITE);
            renderer.endGpuTimer(gpuStage);
        }
        valid = valid && renderer.performanceStats().gpuQueriesIssued <= 1;
        issued += renderer.performanceStats().gpuQueriesIssued;
        EndDrawing();
    }
    valid = valid && (!renderer.performanceStats().gpuTimersSupported || (issued > 0 && issued <= 24));
    renderer.setGpuProfilingEnabled(false); renderer.beginFrame();
    renderer.beginGpuTimer(RendererGpuStage::World); renderer.endGpuTimer(RendererGpuStage::World);
    valid = valid && renderer.performanceStats().gpuQueriesIssued == 0;

    // Compare the optimized real pass with the same shader drawn fullscreen.
    // This detects clipped swinging handles, outlines and Retina conversion.
    const auto raw = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    SetTextureFilter(raw.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(raw.texture, TEXTURE_WRAP_CLAMP);
    const auto shader = LoadShader(nullptr, "assets/shaders/viewmodel_composite.fs");
    const Vector2 texel{1.0F / static_cast<float>(raw.texture.width), 1.0F / static_cast<float>(raw.texture.height)};
    SetShaderValue(shader, GetShaderLocation(shader,"texelSize"), &texel, SHADER_UNIFORM_VEC2);
    for (const auto visual : {FirstPersonToolVisual::Axe, FirstPersonToolVisual::Pickaxe, FirstPersonToolVisual::Club}) {
        for (const bool alternate : {false, true}) for (const float progress : {0.0F, .3F, .5F, .8F}) {
            const auto tuning = defaultFirstPersonToolTuning(visual);
            const auto toolCamera = firstPersonToolCamera(visual);
            BeginDrawing(); ClearBackground({23,42,57,255});
            valid = renderer.beginFirstPersonToolPass() && valid;
            BeginMode3D(toolCamera);
            valid = renderer.drawFirstPersonTool(visual, progress, .6F, .5F, tuning, 0, 0, alternate) && valid;
            EndMode3D(); renderer.endFirstPersonToolPass(tuning); rlDrawRenderBatchActive();
            Image actual = LoadImageFromScreen(); EndDrawing();
            BeginTextureMode(raw); ClearBackground(BLANK); BeginMode3D(toolCamera);
            static_cast<void>(renderer.drawFirstPersonTool(visual, progress, .6F, .5F, tuning, 0, 0, alternate));
            EndMode3D(); EndTextureMode();
            const std::array values{1.0F, tuning.outlineWidth * static_cast<float>(raw.texture.width) / static_cast<float>(std::max(GetRenderWidth(),1)),
                tuning.outlineStrength, tuning.rimStrength, tuning.brightness, tuning.saturation};
            constexpr std::array names{"outlineEnabled","outlineWidth","outlineStrength","rimStrength","brightness","saturation"};
            for (std::size_t i=0;i<names.size();++i) SetShaderValue(shader,GetShaderLocation(shader,names[i]),&values[i],SHADER_UNIFORM_FLOAT);
            BeginDrawing(); ClearBackground({23,42,57,255}); BeginShaderMode(shader);
            DrawTexturePro(raw.texture,{0,0,float(raw.texture.width),-float(raw.texture.height)},{0,0,float(GetScreenWidth()),float(GetScreenHeight())},{},0,WHITE);
            EndShaderMode(); rlDrawRenderBatchActive(); Image expected = LoadImageFromScreen(); EndDrawing();
            Color* a = LoadImageColors(actual); Color* b = LoadImageColors(expected);
            int difference = 0; int minX=actual.width,minY=actual.height,maxX=0,maxY=0;
            for (int pixel=0;pixel<actual.width*actual.height;++pixel) {
                if (std::max({std::abs(int(a[pixel].r)-int(b[pixel].r)),std::abs(int(a[pixel].g)-int(b[pixel].g)),std::abs(int(a[pixel].b)-int(b[pixel].b))}) > 2) { ++difference; minX=std::min(minX,pixel%actual.width); maxX=std::max(maxX,pixel%actual.width); minY=std::min(minY,pixel/actual.width); maxY=std::max(maxY,pixel/actual.width); }
            }
            if (difference) std::cerr << "viewmodel crop changed pixels: " << int(visual) << '/' << progress << '/' << alternate << " = " << difference << " bounds=" << minX << "," << minY << ".." << maxX << "," << maxY << '\n';
            valid = valid && difference == 0;
            UnloadImageColors(a); UnloadImageColors(b); UnloadImage(actual); UnloadImage(expected);
        }
    }
    UnloadRenderTexture(raw); UnloadShader(shader); renderer.shutdown(); CloseWindow();
    std::cout << "GPU sparse queries and complete viewmodel pixel parity: " << valid << '\n';
    return valid ? 0 : 1;
}
