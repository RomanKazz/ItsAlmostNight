#pragma once

#include "graphics/FireWandEffect.hpp"

namespace ian::ice_wand_effect {

inline void emitProjectile(const IceWandProjectile& projectile, Vector3 camera,
                           double time, float interpolation) {
    if (!projectile.active || projectile.element != WandElement::Ice || projectile.radius <= 0) return;
    using namespace fire_wand_effect;
    const auto point = [](Vec3 p) {
        return Vector3{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    };
    const Vector3 head = Vector3Lerp(point(projectile.previousPosition), point(projectile.position),
        std::clamp(interpolation, 0.0F, 1.0F));
    const Vector3 front = normalized(Vector3Subtract(camera, head), {0, 0, 1});
    const Vector3 right = normalized(Vector3CrossProduct({0, 1, 0}, front), {1, 0, 0});
    const Vector3 up = normalized(Vector3CrossProduct(front, right), {0, 1, 0});
    const double clock = time + static_cast<double>(projectile.id.index % 97U) * .13;
    const float radius = static_cast<float>(projectile.radius) * 1.18F *
        (1 + .035F * static_cast<float>(std::sin(clock * 9)));
    rlNormal3f(front.x, front.y, front.z);
    // Same solid, layered silhouette as the fire core. A blue rim, cyan face
    // and milky heart give ice its own palette without wire rings or a wake.
    disc(head, right, up, radius, {62, 202, 255, 255}, {35, 100, 211, 255});
    Vector3 face = Vector3Add(head, Vector3Scale(front, radius * .08F));
    face = planePoint(face, right, up, -radius * .12F, radius * .12F);
    disc(face, right, up, radius * .78F, {191, 251, 255, 255}, {57, 214, 243, 255});
    const float drift = .06F * static_cast<float>(std::sin(clock * 3));
    const Vector3 core = planePoint(Vector3Add(face, Vector3Scale(front, radius * .04F)),
        right, up, drift * radius, drift * radius);
    disc(core, right, up, radius * .39F, {250, 255, 255, 255}, {207, 248, 255, 255});
    const Vector3 glint = planePoint(Vector3Add(face, Vector3Scale(front, radius * .06F)),
        right, up, -radius * .39F, radius * .36F);
    disc(glint, right, up, radius * .105F, {255, 255, 255, 255}, {227, 255, 255, 255}, 12);
}

} // namespace ian::ice_wand_effect
