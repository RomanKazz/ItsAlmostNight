#include "presentation/PresentationEffectQueries.hpp"

#include <algorithm>
#include <cmath>

namespace ian::presentation {
namespace {

constexpr double Pi = 3.14159265358979323846;

bool targetsResource(
    const PresentationEffect& effect, EntityId id) {
    const bool resourceHit =
        effect.type ==
            PresentationEffectType::ResourceHitWood ||
        effect.type ==
            PresentationEffectType::ResourceHitStone;
    return resourceHit && effect.entityId == id;
}

} // namespace

float resourceHitFlash(
    std::span<const PresentationEffect> effects,
    EntityId id) {
    float amount = 0.0F;
    for (const PresentationEffect& effect : effects) {
        if (!targetsResource(effect, id)) {
            continue;
        }
        amount = std::max(
            amount,
            static_cast<float>(
                effect.remaining / effect.duration));
    }
    return amount;
}

float resourceHitScale(
    std::span<const PresentationEffect> effects,
    EntityId id) {
    float scale = 1.0F;
    for (const PresentationEffect& effect : effects) {
        if (!targetsResource(effect, id)) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                effect.remaining / effect.duration),
            0.0F, 1.0F);
        constexpr float MinimumScale = 0.84F;
        constexpr float CompressionEnd = 0.18F;
        if (progress < CompressionEnd) {
            const float t = progress / CompressionEnd;
            const float eased =
                1.0F - std::pow(1.0F - t, 3.0F);
            scale = std::min(
                scale,
                1.0F -
                    (1.0F - MinimumScale) * eased);
        } else {
            const float t =
                (progress - CompressionEnd) /
                (1.0F - CompressionEnd);
            const float eased =
                1.0F - std::pow(1.0F - t, 3.0F);
            scale = std::min(
                scale,
                MinimumScale +
                    (1.0F - MinimumScale) * eased);
        }
    }
    return scale;
}

Vec3 resourceHitOffset(
    std::span<const PresentationEffect> effects,
    EntityId id, Vec3 position) {
    Vec3 offset{};
    for (const PresentationEffect& effect : effects) {
        if (!targetsResource(effect, id)) {
            continue;
        }
        const double fromHitX =
            position.x - effect.position.x;
        const double fromHitZ =
            position.z - effect.position.z;
        const double length =
            std::hypot(fromHitX, fromHitZ);
        if (length <= 1e-6) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                effect.remaining / effect.duration),
            0.0F, 1.0F);
        const double displacement =
            static_cast<double>(
                std::sin(progress * Pi) *
                (1.0F - progress) * 0.24F);
        offset.x +=
            fromHitX / length * displacement;
        offset.z +=
            fromHitZ / length * displacement;
    }
    return offset;
}

} // namespace ian::presentation
