#pragma once

#include <raylib.h>
#include <cstdint>
#include <array>
#include <utility>
#include "progression/RunUpgrade.hpp"

namespace ian {
// Generated tomes share the building icons' chunky illustrated style. Kept
// separate from upgrade IDs: saved runs and gameplay metadata stay unchanged.
inline constexpr std::array<std::pair<RunUpgradeEffect, const char*>, 23> RunTomeIconPaths{{
    {RunUpgradeEffect::BuildingHealth, "assets/ui/tomes/fortitude.png"},
    {RunUpgradeEffect::BuildingCapacity, "assets/ui/tomes/expansion.png"},
    {RunUpgradeEffect::ProductionSpeed, "assets/ui/tomes/industry.png"},
    {RunUpgradeEffect::Luck, "assets/ui/tomes/fortune.png"},
    {RunUpgradeEffect::ResourceCapacity, "assets/ui/tomes/capacity.png"},
    {RunUpgradeEffect::MaximumHealth, "assets/ui/tomes/vitality.png"},
    {RunUpgradeEffect::RecoverableArmor, "assets/ui/tomes/guard.png"},
    {RunUpgradeEffect::MoveSpeed, "assets/ui/tomes/swift.png"},
    {RunUpgradeEffect::AttackSpeed, "assets/ui/tomes/rapid.png"},
    {RunUpgradeEffect::Damage, "assets/ui/tomes/might.png"},
    {RunUpgradeEffect::Regeneration, "assets/ui/tomes/regen.png"},
    {RunUpgradeEffect::BloodHarvest, "assets/ui/tomes/vampiric.png"},
    {RunUpgradeEffect::Bounces, "assets/ui/tomes/bounce.png"},
    {RunUpgradeEffect::ProjectileCount, "assets/ui/tomes/volley.png"},
    {RunUpgradeEffect::Pierce, "assets/ui/tomes/piercing.png"},
    {RunUpgradeEffect::GatheringSpeed, "assets/ui/tomes/gathering.png"},
    {RunUpgradeEffect::ProjectileSpeed, "assets/ui/tomes/velocity.png"},
    {RunUpgradeEffect::AttackDuration, "assets/ui/tomes/duration.png"},
    {RunUpgradeEffect::AttackSize, "assets/ui/tomes/size.png"},
    {RunUpgradeEffect::CriticalChance, "assets/ui/tomes/precision.png"},
    {RunUpgradeEffect::CriticalDamage, "assets/ui/tomes/ferocity.png"},
    {RunUpgradeEffect::AttackKnockback, "assets/ui/tomes/impact.png"},
    {RunUpgradeEffect::PoisonChance, "assets/ui/tomes/venom.png"},
}};

[[nodiscard]] constexpr const char* runTomeIconPath(RunUpgradeEffect effect) {
    for (const auto& [tome, path] : RunTomeIconPaths)
        if (tome == effect) return path;
    return nullptr;
}

// Cached once by GameUi. Existing upgrade symbols become embossed book covers.
inline Texture2D createRunTomeIcon(Texture2D symbol, std::uint32_t rgb) {
    const RenderTexture2D target = LoadRenderTexture(256, 256);
    if (!IsRenderTextureValid(target)) return {};
    const Color cover{static_cast<unsigned char>(rgb >> 16),
        static_cast<unsigned char>(rgb >> 8), static_cast<unsigned char>(rgb), 255};
    const Color edge{static_cast<unsigned char>(cover.r * .60F),
        static_cast<unsigned char>(cover.g * .60F), static_cast<unsigned char>(cover.b * .60F), 255};
    const auto quad = [](Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
        DrawTriangle(a, d, b, color);
        DrawTriangle(b, d, c, color);
    };
    BeginTextureMode(target);
    ClearBackground(BLANK);
    quad({39, 39}, {184, 19}, {220, 207}, {65, 234}, {20, 25, 31, 255});
    quad({64, 70}, {199, 49}, {210, 202}, {81, 222}, {255, 241, 188, 255});
    for (int line = 0; line < 4; ++line) {
        const float y = 184.0F + static_cast<float>(line) * 7.0F;
        DrawLineEx({80, y + 22}, {210, y}, 2, {200, 170, 119, 255});
    }
    quad({137, 200}, {159, 197}, {162, 238}, {141, 231}, {229, 76, 97, 255});
    quad({36, 34}, {174, 14}, {194, 197}, {57, 219}, {20, 25, 31, 255});
    quad({44, 42}, {167, 23}, {184, 190}, {64, 208}, cover);
    quad({44, 42}, {66, 39}, {85, 205}, {64, 208}, edge);
    DrawLineEx({73, 49}, {159, 36}, 3, {255, 239, 173, 255});
    DrawLineEx({88, 190}, {174, 178}, 3, {255, 239, 173, 255});
    DrawLineEx({52, 76}, {73, 73}, 4, {255, 239, 173, 255});
    DrawLineEx({63, 171}, {84, 168}, 4, {255, 239, 173, 255});
    DrawCircle(126, 118, 45, edge);
    if (IsTextureValid(symbol)) {
        DrawTexturePro(symbol, {0, 0, static_cast<float>(symbol.width), static_cast<float>(symbol.height)},
            {125, 117, 83, 83}, {41.5F, 41.5F}, -8, WHITE);
    }
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    const Texture2D result = LoadTextureFromImage(image);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return result;
}
} // namespace ian
