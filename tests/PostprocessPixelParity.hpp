#pragma once
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace ian::test {
inline bool postprocessPixelParity() {
    constexpr int Width = 256, Height = 144;
    std::array<Image, 4> inputs{GenImageColor(Width, Height, WHITE),
        GenImageColor(Width, Height, WHITE), GenImageColor(Width, Height, BLANK),
        GenImageColor(Width, Height, BLANK)};
    Image depth{MemAlloc(Width * Height * sizeof(float)), Width, Height, 1, PIXELFORMAT_UNCOMPRESSED_R32};
    const Matrix projection = MatrixPerspective(65 * DEG2RAD, double(Width) / Height, .01, 1000);
    const auto inverse = MatrixInvert(projection);
    for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
        const int i = y * Width + x;
        const int cell = (x / 13 + y / 17) % 5;
        const float z = 3.0F + float(cell) * 9 + float(x % 13) * .003F;
        static_cast<float*>(depth.data)[i] = cell == 0 ? 1.0F :
            .5F * ((-projection.m10 * z + projection.m14) / z + 1);
        static_cast<Color*>(inputs[0].data)[i] = {static_cast<unsigned char>(x),
            static_cast<unsigned char>(y + 90), static_cast<unsigned char>((x * 13 + y * 7) % 256),
            static_cast<unsigned char>(x % 23 == 0 ? 32 : 255)};
        static_cast<Color*>(inputs[1].data)[i] = {static_cast<unsigned char>(110 + cell * 6),
            static_cast<unsigned char>(115 + cell * 4), static_cast<unsigned char>(cell * 64 + (cell < 4 ? 32 : -1)), 255};
        static_cast<Color*>(inputs[2].data)[i] = {(x / 35) % 3 == 0 ? static_cast<unsigned char>(0) :
            static_cast<unsigned char>(x % 17 * 12), 0, 0, 255};
        static_cast<Color*>(inputs[3].data)[i] = x > 115 && x < 135 ? WHITE : BLANK;
    }
    std::array<Texture2D, 4> textures{};
    for (std::size_t i = 0; i < inputs.size(); ++i) textures[i] = LoadTextureFromImage(inputs[i]);
    SetTextureFilter(textures[2], TEXTURE_FILTER_BILINEAR);
    const auto depthTexture = LoadTextureFromImage(depth);
    std::array<Shader, 2> shaders{LoadShader(nullptr, "tests/fixtures/postprocess_reference.fs"),
        LoadShader(nullptr, "assets/shaders/postprocess.fs")};
    const auto target = LoadRenderTexture(Width, Height);
    bool valid = IsShaderValid(shaders[0]) && IsShaderValid(shaders[1]) && IsRenderTextureValid(target);
    for (int mode = 0; valid && mode < 18; ++mode) {
        std::array<Image, 2> images{};
        for (std::size_t version = 0; version < shaders.size(); ++version) {
            auto shader = shaders[version];
            BeginTextureMode(target); ClearBackground(BLANK); BeginShaderMode(shader);
            const auto scalar = [&](const char* name, float value) {
                SetShaderValue(shader, GetShaderLocation(shader, name), &value, SHADER_UNIFORM_FLOAT);
            };
            for (const auto* name : {"contrast", "saturation", "gammaValue", "outlineStrength", "outlinePixelScale"}) scalar(name, 1);
            scalar("ssaoEnabled", 1); scalar("ssaoStrength", .35F); scalar("inkOutlinesEnabled", 1);
            scalar("outlineWidth", mode % 3 == 0 ? 1 : mode % 3 == 1 ? 1.5F : 3);
            scalar("outlinePixelScale", mode >= 9 ? 2 : 1);
            scalar("selectionMaskEnabled", mode % 2 == 0 ? 1 : 0);
            scalar("outlineFogStart", 12); scalar("outlineFogEnd", 100); scalar("outlineFogFadeStrength", .75F);
            scalar("bloomEnabled", 1); scalar("bloomStrength", .28F);
            scalar("lowHealthAmount", mode % 3 == 0 ? .5F : 0);
            scalar("lowHealthTime", 2); scalar("lowHealthPulse", .4F);
            scalar("curveShadows", .28F); scalar("curveMidtones", .02F); scalar("curveHighlights", .18F);
            const Vector2 hue{0, 1}, texel{1.0F / Width, 1.0F / Height};
            SetShaderValue(shader, GetShaderLocation(shader, "hueSinCos"), &hue, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, GetShaderLocation(shader, "ssaoTexelSize"), &texel, SHADER_UNIFORM_VEC2);
            SetShaderValueMatrix(shader, GetShaderLocation(shader, "inverseProjection"), inverse);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "sceneDepth"), depthTexture);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "sceneNormal"), textures[1]);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "ssaoTexture"), textures[2]);
            SetShaderValueTexture(shader, GetShaderLocation(shader, "selectionMask"), textures[3]);
            DrawTexture(textures[0], 0, 0, WHITE);
            EndShaderMode(); EndTextureMode(); images[version] = LoadImageFromTexture(target.texture);
        }
        Color* a = LoadImageColors(images[0]); Color* b = LoadImageColors(images[1]);
        int changed = 0;
        for (int i = 0; i < Width * Height; ++i) if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) ++changed;
        if (changed) std::cerr << "Postprocess parity mode " << mode << ": " << changed << " changed pixels\n";
        valid = changed == 0;
        UnloadImageColors(a); UnloadImageColors(b); for (auto image : images) UnloadImage(image);
    }
    for (auto shader : shaders) UnloadShader(shader);
    for (auto texture : textures) UnloadTexture(texture);
    for (auto image : inputs) UnloadImage(image);
    UnloadTexture(depthTexture); UnloadImage(depth); UnloadRenderTexture(target);
    return valid;
}
} // namespace ian::test
