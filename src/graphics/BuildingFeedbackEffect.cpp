#include "graphics/BuildingFeedbackEffect.hpp"
#include "core/DeterministicRandom.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace ian {
namespace {

// Counterclockwise silhouettes extruded into solid, closed low-poly shapes.
constexpr std::array<Vector2, 12> Cross{{
    {-.17F, .5F}, {-.17F, .17F}, {-.5F, .17F}, {-.5F, -.17F},
    {-.17F, -.17F}, {-.17F, -.5F}, {.17F, -.5F}, {.17F, -.17F},
    {.5F, -.17F}, {.5F, .17F}, {.17F, .17F}, {.17F, .5F},
}};
constexpr std::array<Vector2, 7> Arrow{{
    {0, .55F}, {-.52F, .02F}, {-.18F, .02F}, {-.18F, -.5F},
    {.18F, -.5F}, {.18F, .02F}, {.52F, .02F},
}};

float smoothUnit(float value) {
    value = std::clamp(value, 0.0F, 1.0F);
    return value * value * (3.0F - 2.0F * value);
}

struct Symbol {
    Vector3 position;
    float size;
    float yaw;
    float tilt;
    float opacity;
    float distanceSquared;
};

void emitSymbol(const Symbol& symbol, BuildingFeedbackKind kind) {
    const std::span<const Vector2> contour = kind == BuildingFeedbackKind::Repair
        ? std::span<const Vector2>{Cross} : std::span<const Vector2>{Arrow};
    // Arrow shoulders share this cap center, so the fan never crosses the
    // concave notches between its head and stem.
    const Vector2 capCenter{0, kind == BuildingFeedbackKind::Repair ? 0 : .02F};
    const Color base = kind == BuildingFeedbackKind::Repair
        ? Color{58, 245, 100, 255} : Color{255, 202, 48, 255};
    const Matrix rotation = MatrixMultiply(
        MatrixRotateZ(symbol.tilt), MatrixRotateY(symbol.yaw));
    const auto vertex = [&](Vector2 point, float depth) {
        const Vector3 local{point.x * symbol.size, point.y * symbol.size,
                            depth * symbol.size};
        const Vector3 world = Vector3Add(
            symbol.position, Vector3Transform(local, rotation));
        rlTexCoord2f(.5F, .5F);
        rlVertex3f(world.x, world.y, world.z);
    };
    const auto shade = [&](Vector3 normal) {
        const Vector3 worldNormal = Vector3Transform(normal, rotation);
        constexpr Vector3 Light{-.36F, .8F, .48F};
        const float brightness = .68F + .32F * std::max(
            0.0F, Vector3DotProduct(worldNormal, Light));
        rlColor4ub(static_cast<unsigned char>(base.r * brightness),
                   static_cast<unsigned char>(base.g * brightness),
                   static_cast<unsigned char>(base.b * brightness),
                   static_cast<unsigned char>(255.0F * symbol.opacity));
    };
    constexpr float HalfDepth = .12F;
    for (std::size_t edge = 0; edge < contour.size(); ++edge) {
        const Vector2 a = contour[edge];
        const Vector2 b = contour[(edge + 1) % contour.size()];
        shade({0, 0, 1});
        vertex(capCenter, HalfDepth); vertex(a, HalfDepth); vertex(b, HalfDepth);
        shade({0, 0, -1});
        vertex(capCenter, -HalfDepth); vertex(b, -HalfDepth); vertex(a, -HalfDepth);
        shade(Vector3Normalize({b.y - a.y, a.x - b.x, 0}));
        vertex(a, HalfDepth); vertex(a, -HalfDepth); vertex(b, -HalfDepth);
        vertex(a, HalfDepth); vertex(b, -HalfDepth); vertex(b, HalfDepth);
    }
}

} // namespace

BuildingFeedbackHighlight sampleBuildingFeedbackHighlight(
    float progress, float bottomY, float topY, BuildingFeedbackKind kind) {
    if (!std::isfinite(progress) || !std::isfinite(bottomY) ||
        !std::isfinite(topY) || progress <= 0 || progress >= 1) return {};
    const float height = std::max(.2F, topY - bottomY);
    const float halfWidth = std::max(.18F, height * .33F);
    const float travel = smoothUnit(progress);
    const float center = bottomY - halfWidth +
        travel * (height + 2.0F * halfWidth);
    const float strength = .72F * smoothUnit(progress / .12F) *
        (1.0F - smoothUnit((progress - .78F) / .22F));
    return {{center, halfWidth, strength},
        kind == BuildingFeedbackKind::Repair
            ? Vector3{.23F, .96F, .39F} : Vector3{1.0F, .79F, .19F}};
}

void drawBuildingFeedbackEffect(
    Vector3 origin, Vector3 cameraPosition, float progress, float scale,
    BuildingFeedbackKind kind, std::uint32_t seed) {
    if (!std::isfinite(progress) || !std::isfinite(scale) ||
        progress <= 0 || progress >= 1) return;
    scale = std::clamp(scale, .5F, 2.5F);
    std::array<Symbol, 18> symbols{};
    std::size_t count = 0;
    const int particleCount = kind == BuildingFeedbackKind::Repair ? 16 : 18;
    for (int index = 0; index < particleCount; ++index) {
        const auto random = [seed, index](std::uint64_t channel) {
            return static_cast<float>(unitRandom(
                (static_cast<std::uint64_t>(seed) << 32U) ^
                (static_cast<std::uint64_t>(index + 1) * 0x9e3779b97f4a7c15ULL) ^
                (channel * 0x85ebca6bU)));
        };
        const float birth = random(1) * .28F;
        const float lifetime = .55F + random(2) * .17F;
        const float age = (progress - birth) / lifetime;
        if (age <= 0 || age >= 1) continue;
        const float opacity = smoothUnit(age / .10F) *
            (1.0F - smoothUnit((age - .64F) / .36F));
        const float angle = random(3) * 2.0F * PI;
        const float radius = std::max(.72F, scale) *
            (.72F + random(4) * .32F + age * .10F);
        const Vector3 position{
            origin.x + std::cos(angle) * radius + std::sin(age * PI) * .08F,
            origin.y + .75F * (.16F + random(5) * .4F * scale +
                age * (1.9F + random(6) * 1.1F) * scale),
            origin.z + std::sin(angle) * radius,
        };
        const float pop = .35F + .65F * smoothUnit(age / .12F);
        const float shrink = 1.0F - .24F * smoothUnit((age - .65F) / .35F);
        symbols[count++] = {
            .position = position,
            .size = (.26F + random(7) * .19F) * std::clamp(scale, .75F, 1.45F) * pop * shrink,
            .yaw = random(8) * 2.0F * PI +
                age * (4.0F + random(9) * 5.0F) * (random(10) < .5F ? -1.0F : 1.0F),
            .tilt = (random(11) - .5F) * .3F + std::sin(age * 5.0F + angle) * .10F,
            .opacity = opacity,
            .distanceSquared = Vector3DistanceSqr(position, cameraPosition),
        };
    }
    if (count == 0) return;
    std::sort(symbols.begin(), symbols.begin() + static_cast<std::ptrdiff_t>(count),
        [](const Symbol& a, const Symbol& b) { return a.distanceSquared > b.distanceSquared; });
    rlDrawRenderBatchActive();
    // Preserve color blending, but mark covered pixels in scene alpha. The
    // ink composite must not draw the rear object's contours over symbols.
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
        RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlDisableDepthMask();
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_TRIANGLES);
    for (std::size_t index = 0; index < count; ++index) emitSymbol(symbols[index], kind);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

} // namespace ian
