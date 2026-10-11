#pragma once

#include "graphics/GraphicsResources.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

inline bool checkWeaponBaseNormals(ian::GraphicsResources& resources) {
    for (auto* resource : {&resources.cannonModel(), &resources.catapultModel(), &resources.crossbowModel()}) {
        if (!resource->valid() || resource->get().meshCount < 2) return false;
        // Collider removal leaves the stationary authored carriage at index 1.
        const Mesh& mesh = resource->get().meshes[1];
        if (!mesh.vertices || !mesh.normals) return false;
        int sideTriangles = 0, bottomTriangles = 0;
        for (int triangle = 0; triangle < mesh.triangleCount; ++triangle) {
            std::array<int, 3> indices{};
            std::array<Vector3, 3> points{};
            bool radial = true, bottom = true;
            float low = 100, high = -100;
            for (int i = 0; i < 3; ++i) {
                const int index = mesh.indices ? mesh.indices[triangle * 3 + i] : triangle * 3 + i;
                if (index < 0 || index >= mesh.vertexCount) return false;
                indices[static_cast<std::size_t>(i)] = index;
                const Vector3 p{mesh.vertices[index * 3], mesh.vertices[index * 3 + 1], mesh.vertices[index * 3 + 2]};
                points[static_cast<std::size_t>(i)] = p;
                low = std::min(low, p.y); high = std::max(high, p.y);
                radial = radial && std::abs(std::hypot(p.x, p.z) - .6F) < .0001F;
                bottom = bottom && std::abs(p.y) < .00001F;
            }
            const bool side = radial && high < .251F && high - low > .00001F;
            if (!side && !bottom) continue;
            if (side) ++sideTriangles;
            if (bottom) ++bottomTriangles;
            for (std::size_t i = 0; i < indices.size(); ++i) {
                const int index = indices[i];
                const Vector3 n{mesh.normals[index * 3], mesh.normals[index * 3 + 1], mesh.normals[index * 3 + 2]};
                if (side && (std::abs(n.y) > .00001F ||
                    (n.x * points[i].x + n.z * points[i].z) / .6F < .9999F)) {
                    std::cerr << "round weapon wall normal tilts into its bottom cap\n";
                    return false;
                }
                if (bottom && (std::abs(n.y + 1) > .00001F ||
                               std::abs(n.x) + std::abs(n.z) > .00001F)) {
                    std::cerr << "weapon underside must retain a flat downward normal\n";
                    return false;
                }
            }
        }
        if (sideTriangles != 24 || bottomTriangles != 12) return false;
    }
    return true;
}
