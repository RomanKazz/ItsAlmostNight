#include "app/App.hpp"
#include "game/ChallengeArena.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {
namespace {

constexpr int ChallengeFencePegCount = 36;
constexpr float ChallengeFencePegHeight = 2.25F;
constexpr float ChallengeFenceRopeHeight = 1.56F;
constexpr int ChallengeFenceRopeSegments = 5;

float challengeFencePegProgress(float fenceProgress, int index) {
    const float phase =
        static_cast<float>(index) /
        static_cast<float>(ChallengeFencePegCount) * 0.34F;
    const float amount = std::clamp(
        (fenceProgress - phase) / 0.66F, 0.0F, 1.0F);
    return amount * amount * (3.0F - 2.0F * amount);
}

float challengeFenceAngle(int index) {
    return 2.0F * PI * static_cast<float>(index) /
        static_cast<float>(ChallengeFencePegCount);
}

} // namespace

void App::drawChallengeFence(
    const ChallengeColumnInstance& column, bool drawRopes) {
    const float fenceProgress = std::clamp(
        static_cast<float>(column.fenceProgress), 0.0F, 1.0F);
    if (fenceProgress <= 0.001F) return;

    std::array<Vector3, ChallengeFencePegCount> ropeAnchors{};
    std::array<float, ChallengeFencePegCount> pegProgress{};
    for (int index = 0; index < ChallengeFencePegCount; ++index) {
        const float angle = challengeFenceAngle(index);
        const float x = static_cast<float>(column.position.x) +
            std::cos(angle) *
                static_cast<float>(challenge_arena::FenceRadius);
        const float z = static_cast<float>(column.position.z) +
            std::sin(angle) *
                static_cast<float>(challenge_arena::FenceRadius);
        const float groundY = static_cast<float>(
            simulation_.terrain().getHeight(x, z));
        const float amount = challengeFencePegProgress(
            fenceProgress, index);
        pegProgress[static_cast<std::size_t>(index)] = amount;
        const float bounce = std::sin(amount * PI) *
            (1.0F - amount) * 0.16F;
        const float buried = (1.0F - amount) *
            ChallengeFencePegHeight * 0.42F;
        const float baseY = groundY - buried + bounce;
        ropeAnchors[static_cast<std::size_t>(index)] = {
            x,
            baseY + ChallengeFenceRopeHeight * amount,
            z,
        };
        if (amount <= 0.002F) continue;
        // Broad face is tangent to the circle; its normal points radially.
        const float yaw = PI * 0.5F - angle;
        static_cast<void>(renderer_->drawChallengeArenaPeg(
            {x, baseY, z}, yaw, WHITE, amount));
    }

    if (!drawRopes) return;
    const Color ropeShadow{145, 94, 58, 255};
    const Color ropeHighlight{228, 170, 108, 255};
    for (int index = 0; index < ChallengeFencePegCount; ++index) {
        const int nextIndex = (index + 1) % ChallengeFencePegCount;
        const float visibility = std::min(
            pegProgress[static_cast<std::size_t>(index)],
            pegProgress[static_cast<std::size_t>(nextIndex)]);
        if (visibility <= 0.04F) continue;
        const Vector3 start =
            ropeAnchors[static_cast<std::size_t>(index)];
        const Vector3 end =
            ropeAnchors[static_cast<std::size_t>(nextIndex)];
        Vector3 previous = start;
        for (int segment = 1;
             segment <= ChallengeFenceRopeSegments; ++segment) {
            const float t = static_cast<float>(segment) /
                static_cast<float>(ChallengeFenceRopeSegments);
            Vector3 point{
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t,
            };
            point.y -= 0.52F * 4.0F * t * (1.0F - t) * visibility;
            const float outerRadius = 0.192F * visibility;
            const float innerRadius = 0.114F * visibility;
            const Vector3 segmentDelta = Vector3Subtract(point, previous);
            const float segmentLength = Vector3Length(segmentDelta);
            const Vector3 segmentDirection = segmentLength > 0.0001F
                ? Vector3Scale(segmentDelta, 1.0F / segmentLength)
                : Vector3{};
            const float overlap = std::min(
                outerRadius * 0.82F, segmentLength * 0.18F);
            const Vector3 joinedStart = Vector3Subtract(
                previous, Vector3Scale(segmentDirection, overlap));
            const Vector3 joinedEnd = Vector3Add(
                point, Vector3Scale(segmentDirection, overlap));
            DrawCylinderEx(
                joinedStart, joinedEnd,
                outerRadius, outerRadius,
                6, ropeShadow);
            Vector3 highlightStart = joinedStart;
            Vector3 highlightEnd = joinedEnd;
            highlightStart.y += 0.096F * visibility;
            highlightEnd.y += 0.096F * visibility;
            DrawCylinderEx(
                highlightStart, highlightEnd,
                innerRadius, innerRadius,
                6, ropeHighlight);
            previous = point;
        }
    }
}

} // namespace ian
