#include "TestHarness.hpp"
#include "core/Geometry.hpp"
#include "graphics/CameraCulling.hpp"

#include <limits>

void runGeometryTests() {
    using namespace ian;

    requireNear(
        geometry::distanceSquared({1.0, 2.0, 3.0}, {4.0, 6.0, 3.0}),
        25.0, 1e-12, "squared distance uses all vector components");
    const Vec3 normalized = geometry::normalizedOr({0.0, 3.0, 4.0});
    requireNear(normalized.y, 0.6, 1e-12,
                "normalization preserves vector direction");
    requireNear(normalized.z, 0.8, 1e-12,
                "normalization produces unit length");
    const Vec3 cross = geometry::cross(
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
    requireNear(cross.z, 1.0, 1e-12,
                "cross product follows right-handed coordinates");
    const Vec3 clamped = geometry::lerpClamped(
        {1.0, 2.0, 3.0}, {5.0, 6.0, 7.0}, 2.0);
    requireNear(clamped.x, 5.0, 1e-12,
                "clamped interpolation cannot overshoot its target");

    const auto sphereHit = geometry::raySphereDistance(
        {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0},
        {0.0, 0.0, -5.0}, 1.0);
    require(sphereHit.has_value(), "forward ray hits sphere");
    requireNear(*sphereHit, 4.0, 1e-12,
                "sphere raycast returns nearest surface");
    require(!geometry::raySphereDistance(
                 {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0},
                 {0.0, 0.0, -5.0}, 1.0),
            "ray rejects sphere behind origin");

    const auto capsuleBodyHit = geometry::rayVerticalCapsuleDistance(
        {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0},
        {0.0, 0.0, -5.0}, 1.0, 2.0);
    require(capsuleBodyHit.has_value(), "ray hits capsule body");
    requireNear(*capsuleBodyHit, 4.0, 1e-12,
                "capsule body returns nearest surface");
    const auto capsuleCapHit = geometry::rayVerticalCapsuleDistance(
        {0.0, 3.0, 0.0}, {0.0, 0.0, -1.0},
        {0.0, 0.0, -5.0}, 1.0, 2.0);
    require(capsuleCapHit.has_value(), "ray hits capsule end cap");
    requireNear(*capsuleCapHit, 5.0, 1e-12,
                "capsule end cap uses sphere intersection");

    const camera_culling::HorizontalView horizontalView{
        .forward = {0.0F, -1.0F},
        .fovTangent = 1.0F,
    };
    require(
        camera_culling::visibleInHorizontalRange(
            0.0F, -5.0F, horizontalView, 10.0F, 1.0F),
        "horizontal culling keeps objects in front of the camera");
    require(
        !camera_culling::visibleInHorizontalRange(
            0.0F, -12.0F, horizontalView, 10.0F, 1.0F),
        "horizontal culling rejects objects beyond draw distance");
    require(
        !camera_culling::visibleInHorizontalRange(
            0.0F, 5.0F, horizontalView, 10.0F, 1.0F),
        "horizontal culling rejects objects behind the camera");
    require(
        !camera_culling::visibleInHorizontalRange(
            std::numeric_limits<float>::quiet_NaN(), -5.0F,
            horizontalView, 10.0F, 1.0F),
        "horizontal culling rejects non-finite positions");
}
