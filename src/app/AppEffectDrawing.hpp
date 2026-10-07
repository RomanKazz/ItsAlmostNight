#pragma once

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ian::app_effect_drawing {

enum class EffectSurface { Geometry, Texture };

// Preserve additive RGB while tagging the scene alpha for the world-ink pass.
// Geometry and flipbooks must use the same blend factors and differ only in
// texture coverage. If the shader is unavailable, ordinary additive blending
// remains the fallback. Call between completed batches, outside rlBegin/rlEnd.
inline void beginAdditiveEffectPass(
    Shader shader, int textureCoverageLocation,
    EffectSurface surface = EffectSurface::Geometry) {
    if (!IsShaderValid(shader)) {
        BeginBlendMode(BLEND_ADDITIVE);
        return;
    }
    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(
        RL_ONE, RL_ONE, RL_ONE, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    const float useTexture = surface == EffectSurface::Texture ? 1.0F : 0.0F;
    SetShaderValue(shader, textureCoverageLocation, &useTexture, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(shader);
}

inline void endAdditiveEffectPass(Shader shader) {
    if (IsShaderValid(shader)) EndShaderMode();
    EndBlendMode();
}

inline float atmosphereUnit(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0xffffU) / 65535.0F;
}

inline unsigned char atmosphereAlpha(float amount) {
    return static_cast<unsigned char>(std::lround(
        std::clamp(amount, 0.0F, 1.0F) * 255.0F));
}

inline void emitSoftBillboardDisc(
    Vector3 center, Vector3 right, Vector3 up,
    float radius, Color centerColor, int segments = 8) {
    Color edgeColor = centerColor;
    edgeColor.a = 0;
    for (int segment = 0; segment < segments; ++segment) {
        const float angleA = 2.0F * PI * static_cast<float>(segment) /
            static_cast<float>(segments);
        const float angleB = 2.0F * PI * static_cast<float>(segment + 1) /
            static_cast<float>(segments);
        const Vector3 edgeA = Vector3Add(
            center, Vector3Add(
                Vector3Scale(right, std::cos(angleA) * radius),
                Vector3Scale(up, std::sin(angleA) * radius)));
        const Vector3 edgeB = Vector3Add(
            center, Vector3Add(
                Vector3Scale(right, std::cos(angleB) * radius),
                Vector3Scale(up, std::sin(angleB) * radius)));
        rlColor4ub(centerColor.r, centerColor.g,
                   centerColor.b, centerColor.a);
        rlVertex3f(center.x, center.y, center.z);
        rlColor4ub(edgeColor.r, edgeColor.g, edgeColor.b, edgeColor.a);
        rlVertex3f(edgeA.x, edgeA.y, edgeA.z);
        rlVertex3f(edgeB.x, edgeB.y, edgeB.z);
    }
}

} // namespace ian::app_effect_drawing
