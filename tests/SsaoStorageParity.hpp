#pragma once
#include "graphics/GraphicsResources.hpp"
#include <raylib.h>
#include <raymath.h>
#include <array>
#include <cmath>
#include <iostream>

namespace ian::test {
inline bool ssaoStoragePixelParity() {
    constexpr int Width = 256, Height = 144;
    Image depth{MemAlloc(Width * Height * sizeof(float)), Width, Height, 1,
        PIXELFORMAT_UNCOMPRESSED_R32};
    Image normals = GenImageColor(Width, Height, WHITE);
    if (!depth.data || !normals.data) {
        if (depth.data) UnloadImage(depth);
        if (normals.data) UnloadImage(normals);
        return false;
    }
    const std::array shaders{LoadShader(nullptr, "assets/shaders/ssao.fs"),
        LoadShader(nullptr, "assets/shaders/ssao.fs")};
    bool valid = IsShaderValid(shaders[0]) && IsShaderValid(shaders[1]);
    int nonzeroPixels = 0, modes = 0, r8Targets = 0;
    for (int fov : {35, 65, 100}) for (int count : {4, 6, 8, 12})
    for (int pattern = 0; pattern < 4; ++pattern) for (int half = 0; half < 2; ++half) {
        if (!valid) continue;
        const Matrix projection = MatrixPerspective(
            static_cast<float>(fov) * DEG2RAD, double(Width) / Height, .01, 1000);
        const Matrix inverse = MatrixInvert(projection);
        const Matrix view = MatrixRotateX(static_cast<float>(pattern - 1) * .4F);
        for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
            const int pixel = y * Width + x;
            const float z = pattern == 0 ? 3 : pattern == 1 ?
                3 + static_cast<float>((x / 13 + y / 11) % 4) * 9 : pattern == 2 ?
                4 + std::sin(static_cast<float>(x) * .2F) * .7F +
                    std::cos(static_cast<float>(y) * .3F) * .6F :
                15 + static_cast<float>(x) * .25F;
            static_cast<float*>(depth.data)[pixel] = x > 220 ? 1.0F :
                .5F * ((-projection.m10 * z + projection.m14) / z + 1);
            static_cast<Color*>(normals.data)[pixel] = {
                static_cast<unsigned char>(100 + (x % 13) * 3),
                static_cast<unsigned char>(pattern % 2 ? 140 : 240),
                static_cast<unsigned char>(x % 31 == 0 ? 0 : 95 + (x / 31 % 3) * 64), 255};
        }
        const Texture2D depthTexture = LoadTextureFromImage(depth);
        const Texture2D normalTexture = LoadTextureFromImage(normals);
        RenderTextureResource rgba, single;
        if (!rgba.load(Width / (half + 1), Height / (half + 1)) ||
            !single.loadSingleChannel(Width / (half + 1), Height / (half + 1))) {
            UnloadTexture(depthTexture); UnloadTexture(normalTexture);
            valid = false; continue;
        }
        if (single.get().texture.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) {
            ++r8Targets;
            valid = valid && single.get().depth.id == 0;
        }
        const std::array targets{rgba.get(), single.get()};
        std::array<Image, 2> images{};
        for (std::size_t version = 0; version < shaders.size(); ++version) {
            const Shader shader = shaders[version];
            const auto target = targets[version];
            BeginTextureMode(target); ClearBackground(BLANK); BeginShaderMode(shader);
            const auto scalar = [&](const char* name, float value) {
                SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_FLOAT);
            };
            scalar("radius", pattern % 2 ? .65F : .9F);
            scalar("bias", pattern % 2 ? .055F : .045F);
            scalar("fadeStart", pattern % 2 ? 18 : 30);
            scalar("fadeEnd", pattern % 2 ? 30 : 50);
            SetShaderValue(shader, GetShaderLocation(shader, "sampleCount"), &count, SHADER_UNIFORM_INT);
            const Vector2 texel{1.0F / Width, 1.0F / Height};
            SetShaderValue(shader, GetShaderLocation(shader, "texelSize"), &texel, SHADER_UNIFORM_VEC2);
            SetShaderValueMatrix(shader, GetShaderLocation(shader, "projection"), projection);
            SetShaderValueMatrix(shader, GetShaderLocation(shader, "inverseProjection"), inverse);
            SetShaderValueMatrix(shader, GetShaderLocation(shader, "viewMatrix"), view);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "sceneDepth"), depthTexture);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "sceneNormal"), normalTexture);
            DrawTexturePro(normalTexture, {0, 0, float(Width), float(-Height)},
                {0, 0, float(target.texture.width), float(target.texture.height)}, {}, 0, WHITE);
            EndShaderMode(); EndTextureMode();
            images[version] = LoadImageFromTexture(target.texture);
        }
        if (!images[0].data || !images[1].data) valid = false;
        else {
            Color* a = LoadImageColors(images[0]); Color* b = LoadImageColors(images[1]);
            int changed = 0;
            for (int pixel = 0; pixel < images[0].width * images[0].height; ++pixel) {
                if (a[pixel].r) ++nonzeroPixels;
                if (a[pixel].r != b[pixel].r || a[pixel].g != b[pixel].g ||
                    a[pixel].b != b[pixel].b || a[pixel].a != b[pixel].a) ++changed;
            }
            if (changed) std::cerr << "SSAO storage pixel mismatch " << fov << '/' << count
                << '/' << pattern << '/' << half << ": " << changed << '\n';
            valid = valid && changed == 0;
            UnloadImageColors(a); UnloadImageColors(b);
        }
        for (auto image : images) if (image.data) UnloadImage(image);
        UnloadTexture(depthTexture); UnloadTexture(normalTexture); ++modes;
    }
    for (auto shader : shaders) if (IsShaderValid(shader)) UnloadShader(shader);
    UnloadImage(depth); UnloadImage(normals);
    RenderTextureResource invalid;
    valid = valid && !invalid.loadSingleChannel(0, 16) && !invalid.valid();
    std::cout << "SSAO storage parity: " << modes << " modes, R8 targets=" << r8Targets
        << ", nonzero AO pixels=" << nonzeroPixels << '\n';
    return valid && nonzeroPixels > 100;
}
} // namespace ian::test
