#pragma once

#include "world/TerrainHeightfield.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ian {

// One basis per effect; ring vertices sample only their local terrain height.
struct SurfaceEffectPlane {
    Vector3 normal{0,1,0};
    Vector3 right{1,0,0};
    Vector3 forward{0,0,1};
    const TerrainHeightfield* terrain{};
    float elevation{};

    explicit SurfaceEffectPlane(Vector3 surfaceNormal={0,1,0},
                                const TerrainHeightfield* heightfield=nullptr,
                                float heightAboveTerrain=0) : terrain(heightfield), elevation(std::max(0.0F,heightAboveTerrain)) {
        const float length=Vector3LengthSqr(surfaceNormal);
        if (std::isfinite(length) && length>1e-8F) normal=Vector3Normalize(surfaceNormal);
        const Vector3 reference=std::abs(normal.z)<.95F ? Vector3{0,0,1} : Vector3{1,0,0};
        right=Vector3Normalize(Vector3CrossProduct(normal,reference));
        forward=Vector3CrossProduct(right,normal);
    }

    [[nodiscard]] Vector3 point(Vector3 center,float x,float z,float clearance=.045F) const {
        Vector3 p=Vector3Add(center,Vector3Add(Vector3Scale(right,x),Vector3Scale(forward,z)));
        p=Vector3Add(p,Vector3Scale(normal,clearance));
        if (terrain) p.y=static_cast<float>(terrain->getHeight(p.x,p.z))+clearance+elevation;
        return p;
    }
};
} // namespace ian
