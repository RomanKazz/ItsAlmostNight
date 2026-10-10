#pragma once

#include "enemies/EnemySystem.hpp"

#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace ian::elite_details {

// Opaque, depth-tested geometry in bind-head space. No particles, resources,
// per-frame allocations or gameplay collision changes.
inline void draw(BoundingBox bounds, Matrix headTransform,
                 EliteAffixMask affixes, unsigned char opacity) {
    if (!affixes || opacity == 0) return;
    const float width = bounds.max.x - bounds.min.x;
    const float depth = bounds.max.z - bounds.min.z;
    const float radius = std::max(.15F, std::min(width, depth) * .34F);
    const Vector3 origin{(bounds.min.x + bounds.max.x) * .5F,
        bounds.max.y - radius * .14F, (bounds.min.z + bounds.max.z) * .5F};
    const Color gold{255, 201, 40, opacity}, lightGold{255, 234, 101, opacity};
    const Color darkGold{192, 103, 20, opacity};
    const Color ruby{255, 66, 88, opacity}, blue{58, 179, 255, opacity};
    const Color orange{255, 135, 31, opacity}, ivory{255, 241, 208, opacity};
    const auto vertex = [&](Vector3 p, Vector3 normal, Color color) {
        p = Vector3Transform(Vector3Add(origin, Vector3Scale(p, radius)), headTransform);
        normal = Vector3Normalize({
            normal.x * headTransform.m0 + normal.y * headTransform.m4 + normal.z * headTransform.m8,
            normal.x * headTransform.m1 + normal.y * headTransform.m5 + normal.z * headTransform.m9,
            normal.x * headTransform.m2 + normal.y * headTransform.m6 + normal.z * headTransform.m10});
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(normal.x, normal.y, normal.z);
        rlTexCoord2f(0, 0);
        rlVertex3f(p.x, p.y, p.z);
    };
    const auto face = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 outward, Color color) {
        Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        if (Vector3DotProduct(n, outward) < 0) { std::swap(b, c); n = Vector3Negate(n); }
        vertex(a, n, color); vertex(b, n, color); vertex(c, n, color);
    };
    const auto quad = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 n, Color color) {
        face(a, b, c, n, color); face(a, c, d, n, color);
    };
    const auto orb = [&](Vector3 center, Vector3 size, Color color) {
        constexpr int Rings = 6, Sides = 12;
        const auto point = [&](float latitude, float longitude) {
            return Vector3{std::cos(latitude) * std::sin(longitude), std::sin(latitude),
                std::cos(latitude) * std::cos(longitude)};
        };
        const auto triangle = [&](Vector3 a, Vector3 b, Vector3 c) {
            if (Vector3DotProduct(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)),
                    Vector3Add(Vector3Add(a, b), c)) < 0) std::swap(b, c);
            for (Vector3 n : {a, b, c}) {
                const Vector3 p = Vector3Add(center, Vector3Multiply(n, size));
                const Vector3 smooth = Vector3Normalize({n.x / size.x, n.y / size.y, n.z / size.z});
                vertex(p, smooth, color);
            }
        };
        for (int ring = 0; ring < Rings; ++ring) for (int side = 0; side < Sides; ++side) {
            const float a = -PI * .5F + PI * float(ring) / Rings;
            const float b = -PI * .5F + PI * float(ring + 1) / Rings;
            const float u = 2 * PI * float(side) / Sides, v = 2 * PI * float(side + 1) / Sides;
            triangle(point(a, u), point(a, v), point(b, v));
            triangle(point(a, u), point(b, v), point(b, u));
        }
    };
    const auto spike = [&](Vector3 base, Vector3 tip, float thickness, Color color) {
        const Vector3 axis = Vector3Normalize(Vector3Subtract(tip, base));
        const Vector3 u = Vector3Normalize(Vector3CrossProduct(axis, {0, 0, 1}));
        const Vector3 v = Vector3CrossProduct(axis, u);
        for (int side = 0; side < 8; ++side) {
            const auto radial = [&](int index) {
                const float angle = float(index) * 2 * PI / 8;
                return Vector3Add(Vector3Scale(u, std::cos(angle)), Vector3Scale(v, std::sin(angle)));
            };
            const Vector3 n = radial(side), next = radial(side + 1);
            face(Vector3Add(base, Vector3Scale(n, thickness)), tip,
                Vector3Add(base, Vector3Scale(next, thickness)), n, color);
        }
    };

    rlCheckRenderBatchLimit(4096);
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    // Thick open crown band, six large triangular peaks, polished pearl tips.
    constexpr int Sides = 12;
    for (int side = 0; side < Sides; ++side) {
        const auto ringPoint = [&](int index, float r, float y) {
            const float angle = float(index) * 2 * PI / Sides;
            return Vector3{std::sin(angle) * r, y, std::cos(angle) * r};
        };
        const Vector3 a = ringPoint(side, 1, 0), b = ringPoint(side + 1, 1, 0);
        const Vector3 c = ringPoint(side + 1, 1, .32F), d = ringPoint(side, 1, .32F);
        const Vector3 n{a.x, 0, a.z};
        quad(a, b, c, d, n, gold);
        quad(ringPoint(side, .83F, 0), ringPoint(side, .83F, .32F),
            ringPoint(side + 1, .83F, .32F), ringPoint(side + 1, .83F, 0), Vector3Negate(n), darkGold);
        quad(d, c, ringPoint(side + 1, .83F, .32F), ringPoint(side, .83F, .32F), {0, 1, 0}, lightGold);
        if (side % 2 == 0) {
            const Vector3 tip = ringPoint(side + 1, 1.13F, .98F);
            const Vector3 left = ringPoint(side, 1, .29F), right = ringPoint(side + 2, 1, .29F);
            face(left, right, tip, n, lightGold);
            const Vector3 innerLeft{left.x * .86F, left.y, left.z * .86F};
            const Vector3 innerRight{right.x * .86F, right.y, right.z * .86F};
            const Vector3 innerTip{tip.x * .86F, tip.y, tip.z * .86F};
            face(innerLeft, innerTip, innerRight, Vector3Negate(n), gold);
            quad(left, tip, innerTip, innerLeft, {0, 1, 0}, gold);
            quad(tip, right, innerRight, innerTip, {0, 1, 0}, gold);
            orb(tip, {.10F, .10F, .10F}, lightGold);
        }
    }
    const Color gem = hasEliteAffix(affixes, EliteAffix::Warden) ? blue
        : hasEliteAffix(affixes, EliteAffix::Volatile) ? orange : ruby;
    orb({0, .26F, 1.03F}, {.19F, .24F, .11F}, gem);
    orb({-.045F, .34F, 1.13F}, {.045F, .06F, .025F}, ivory);
    if (hasEliteAffix(affixes, EliteAffix::Berserker)) {
        for (float sign : {-1.0F, 1.0F}) {
            orb({sign * .95F, .13F, 0}, {.27F, .30F, .28F}, ruby);
            spike({sign * 1.02F, .19F, 0}, {sign * 1.48F, .55F, 0}, .23F, ivory);
            spike({sign * 1.39F, .47F, 0}, {sign * 1.47F, 1.13F, .05F}, .16F, ivory);
        }
    }
    if (hasEliteAffix(affixes, EliteAffix::Warden)) {
        // Wing-shaped enamel shields hang beside the temples, leave eyes clear.
        for (float sign : {-1.0F, 1.0F}) {
            const Vector3 a{sign * 1.12F, .32F, .30F}, b{sign * 1.42F, .03F, .25F};
            const Vector3 c{sign * 1.10F, -.74F, .48F}, d{sign * .93F, -.12F, .50F};
            const Vector3 center{sign * 1.19F, -.13F, .65F}, n{sign * .3F, 0, 1};
            face(a, b, center, n, blue); face(b, c, center, n, {33, 115, 209, opacity});
            face(c, d, center, n, blue); face(d, a, center, n, {140, 223, 255, opacity});
            face(a, c, b, Vector3Negate(n), darkGold); face(a, d, c, Vector3Negate(n), gold);
            orb({sign * 1.19F, -.11F, .67F}, {.07F, .14F, .06F}, lightGold);
        }
    }
    if (hasEliteAffix(affixes, EliteAffix::Volatile)) {
        for (float sign : {-1.0F, 1.0F}) {
            const Vector3 center{sign * 1.18F, -.18F, -.66F};
            orb(center, {.33F, .38F, .33F}, orange);
            orb(Vector3Add(center, {0, .35F, 0}), {.14F, .10F, .14F}, darkGold);
            spike(Vector3Add(center, {0, .40F, 0}), Vector3Add(center, {sign * .11F, .72F, 0}), .06F, ivory);
            orb(Vector3Add(center, {sign * .11F, .73F, 0}), {.10F, .13F, .10F}, ruby);
        }
    }
    rlEnd();
}

} // namespace ian::elite_details
