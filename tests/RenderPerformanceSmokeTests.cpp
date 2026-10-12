#include "graphics/Renderer.hpp"
#include "graphics/ViewModelCompositeBounds.hpp"
#include "graphics/CameraCulling.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <iostream>

namespace {
bool firstCombatDraw(ian::Renderer& renderer) {
#if defined(_WIN32)
    using GetInteger = void (__stdcall *)(unsigned int, int*);
    using Finish = void (__stdcall *)();
#else
    using GetInteger = void (*)(unsigned int, int*);
    using Finish = void (*)();
#endif
    const auto getInteger = reinterpret_cast<GetInteger>(rlGetProcAddress("glGetIntegerv"));
    const auto finish = reinterpret_cast<Finish>(rlGetProcAddress("glFinish"));
    if (!getInteger || !finish) return true;
    const Camera3D camera{{0,2,5},{0,1,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    const auto savedSettings = renderer.settings();
    renderer.settings().postProcessing = true;
    renderer.settings().inkOutlines = true;
    std::array<double,3> timings{};
    bool valid = true;
    for (std::size_t frame=0;frame<timings.size();++frame) {
        renderer.beginFrame();
        if (renderer.beginSelectionMaskPass(camera)) {
            DrawCube({0,1,0},1,1,1,WHITE);
            renderer.endSelectionMaskPass();
        }
        renderer.beginWorldPass(BLACK,camera);
        BeginMode3D(camera);
        ian::WorldLighting lighting{}; lighting.cameraPosition=camera.position;
        renderer.beginWorldShader(lighting);
        const auto start=std::chrono::steady_clock::now();
        for (const bool occluder : {false,true}) {
            ian::WorldMaterialState material{};
            material.unlitAmount=1; material.inkOccluder=occluder;
            renderer.setWorldMaterial(material);
            renderer.beginColorOnlyWorldEffects();
            int normalOutput=0,coverageOutput=0;
            getInteger(0x8826,&normalOutput); getInteger(0x8827,&coverageOutput);
            valid &= coverageOutput == 0x8CE2 && normalOutput == (occluder ? 0x8CE1 : 0);
            BeginBlendMode(BLEND_ADDITIVE); rlDisableDepthMask();
            DrawTriangle3D({-1,0,0},{1,0,0},{0,2,0},{255,170,30,160});
            rlDrawRenderBatchActive(); rlEnableDepthMask(); EndBlendMode();
            renderer.endColorOnlyWorldEffects();
        }
        finish();
        timings[frame]=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
        renderer.endWorldShader(); EndMode3D();
        renderer.endWorldPass(); renderer.endFrame();
    }
    renderer.settings()=savedSettings;
    std::cout << "Combat first/repeated GPU draw ms: " << timings[0] << ' '
        << timings[1] << ' ' << timings[2] << "; coverage routing=" << valid << '\n';
    return valid;
}

bool presentWaitDiagnostics(ian::Renderer& renderer) {
    const auto savedSettings = renderer.settings();
    renderer.settings().performanceLogging = true;
    // Exercise a long EndDrawing using raylib's limiter. Depending on build
    // flags it sleeps or spins; the diagnostics must account for either.
    SetTargetFPS(25);
    const Camera3D camera{{0, 2, 5}, {0, 1, 0}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
    const auto frame = [&] {
        renderer.beginFrame();
        renderer.beginWorldPass(BLACK, camera);
        renderer.endWorldPass();
        DrawRectangle(0, 0, 32, 32, WHITE);
        renderer.endFrame();
    };
    frame(); frame();
    const auto stats = renderer.performanceStats();
    bool valid = std::isfinite(stats.finalBatchSubmissionMs) &&
        stats.finalBatchSubmissionMs >= 0.0;
#if defined(__APPLE__) || defined(__linux__)
    valid = valid && stats.presentThreadCpuValid;
#endif
    if (stats.presentThreadCpuValid) {
        valid = valid && std::isfinite(stats.presentThreadCpuMs) &&
            stats.presentThreadCpuMs >= 0.0 && stats.presentNotRunningMs >= 0.0 &&
            std::isfinite(stats.presentNotRunningMs) &&
            std::abs(stats.presentThreadCpuMs + stats.presentNotRunningMs -
                renderer.frameSwapMilliseconds()) < 0.1;
    }
    SetTargetFPS(0);
    renderer.settings().performanceLogging = false;
    frame();
    const auto& disabled = renderer.performanceStats();
    valid = valid && !disabled.presentThreadCpuValid &&
        disabled.presentThreadCpuMs == 0.0 && disabled.presentNotRunningMs == 0.0 &&
        disabled.finalBatchSubmissionMs == 0.0;
    renderer.settings() = savedSettings;
    if (!valid) std::cerr << "EndDrawing thread CPU/wait diagnostics failed: valid="
        << stats.presentThreadCpuValid << " cpu=" << stats.presentThreadCpuMs
        << " not-running=" << stats.presentNotRunningMs
        << " batch=" << stats.finalBatchSubmissionMs << '\n';
    return valid;
}

bool skyDepthParity(ian::Renderer& renderer) {
    using namespace ian;
    const auto savedSettings = renderer.settings();
    renderer.settings().renderWidth = GetScreenWidth();
    renderer.settings().renderHeight = GetScreenHeight();
    renderer.settings().pixelSize = 1;
    bool valid = true;
    for (const bool post : {false, true}) for (const float night : {0.0F, .3F, .7F, 1.0F}) {
        renderer.settings().postProcessing = post;
        const Camera3D camera{{0, 2, 5}, {0, 1, 0}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
        WorldLighting lighting{}; lighting.cameraPosition = camera.position;
        SkyState sky{}; sky.nightAmount = night; sky.timeSeconds = 17;
        const auto capture = [&](bool late) {
            renderer.beginFrame(); renderer.beginWorldPass(BLACK, camera);
            if (!late) renderer.drawSky(sky);
            BeginMode3D(camera); renderer.beginWorldShader(lighting);
            WorldMaterialState material{}; material.terrainAmount = 1;
            renderer.setWorldMaterial(material);
            DrawCube({0, -.3F, 0}, 100, .6F, 100, GREEN);
            material.terrainAmount = 0; renderer.setWorldMaterial(material);
            DrawCube({-.9F, 1, 0}, 1, 2, 1, RED);
            DrawSphere({1.1F, 1, 0}, .8F, BLUE);
            renderer.endWorldShader(); EndMode3D();
            if (late) renderer.drawSky(sky);
            // Transparent geometry must still blend over the preserved sky.
            BeginMode3D(camera); DrawCube({0, 3, -2}, 3, 1, 1, {255, 200, 100, 90}); EndMode3D();
            renderer.endWorldPass(); rlDrawRenderBatchActive();
            Image result = LoadImageFromScreen(); renderer.endFrame(); return result;
        };
        const Image before = capture(false), after = capture(true);
        Color* a = LoadImageColors(before); Color* b = LoadImageColors(after);
        int changed = 0;
        for (int i = 0; i < before.width * before.height; ++i) {
            if (std::max({std::abs(int(a[i].r)-int(b[i].r)), std::abs(int(a[i].g)-int(b[i].g)),
                          std::abs(int(a[i].b)-int(b[i].b))}) > 2) ++changed;
        }
        if (changed) std::cerr << "late sky pixel mismatch: post=" << post << " night=" << night << " count=" << changed << '\n';
        valid = valid && changed == 0;
        UnloadImageColors(a); UnloadImageColors(b); UnloadImage(before); UnloadImage(after);
    }
    renderer.settings() = savedSettings;
    return valid;
}
}

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
    valid = firstCombatDraw(renderer) && valid;
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
    UnloadRenderTexture(raw); UnloadShader(shader);
    valid = skyDepthParity(renderer) && valid;
    valid = presentWaitDiagnostics(renderer) && valid;
    renderer.shutdown(); CloseWindow();
    std::cout << "GPU sparse queries and complete viewmodel pixel parity: " << valid << '\n';
    return valid ? 0 : 1;
}
