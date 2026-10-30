#include "TestHarness.hpp"
#include "core/Geometry.hpp"
#include "graphics/CameraCulling.hpp"
#include <raymath.h>

#include <limits>

void runGeometryTests() {
    using namespace ian;

    const camera_culling::Frustum clipBox(MatrixIdentity());
    require(clipBox.intersects({{-0.5F, -0.5F, -0.5F},
                                {0.5F, 0.5F, 0.5F}}),
            "frustum retains interior bounds");
    for (int axis = 0; axis < 3; ++axis) {
        for (const float sign : {-1.0F, 1.0F}) {
            Vector3 center{};
            if (axis == 0) center.x = sign * 2.0F;
            if (axis == 1) center.y = sign * 2.0F;
            if (axis == 2) center.z = sign * 2.0F;
            require(!clipBox.intersects({center, center}),
                    "frustum rejects outside each of its six planes");
        }
    }
    const BoundingBox windEdge{{1.05F, -0.1F, -0.1F},
                               {1.1F, 0.1F, 0.1F}};
    require(!clipBox.intersects(windEdge) &&
                clipBox.intersects(windEdge, 0.12F),
            "wind padding retains geometry that can sway into view");
    require(clipBox.intersects({{0.9F, -0.1F, -0.1F},
                               {2.0F, 0.1F, 0.1F}}),
            "frustum retains a mesh straddling an edge");
    const Matrix downwardView = MatrixLookAt(
        {0.0F, 10.0F, 0.0F}, {}, {0.0F, 0.0F, -1.0F});
    const camera_culling::Frustum downwardFrustum(MatrixMultiply(
        downwardView, MatrixPerspective(PI / 2.0, 2.0, 0.1, 100.0)));
    require(downwardFrustum.intersects({{-1.0F, 0.0F, -1.0F},
                                       {1.0F, 1.0F, 1.0F}}),
            "pitched camera retains grass below it");
    require(!downwardFrustum.intersects({{-1.0F, 12.0F, -1.0F},
                                        {1.0F, 13.0F, 1.0F}}),
            "pitched camera rejects bounds behind its view");
    require(downwardFrustum.intersects({{17.0F, 0.0F, 0.0F},
                                       {18.0F, 0.1F, 0.1F}}) &&
                !downwardFrustum.intersects({{0.0F, 0.0F, 17.0F},
                                             {0.1F, 0.1F, 18.0F}}),
            "frustum uses the render target aspect ratio");

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
