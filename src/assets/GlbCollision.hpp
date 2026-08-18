#pragma once

#include "core/Types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

enum class ModelColliderType {
    Box,
    Cylinder,
    Sphere,
    Slope,
};

struct ModelCollider {
    std::string name;
    ModelColliderType type{ModelColliderType::Box};
    bool walkable{};
    Vec3 minimum;
    Vec3 maximum;
};

struct ModelSocket {
    std::string name;
    Vec3 position;
    Vec3 forward;
    Vec3 up;
};

struct GlbCollisionAsset {
    std::vector<ModelCollider> colliders;
    std::vector<ModelSocket> sockets;
    std::vector<std::size_t> renderMeshIndices;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const {
        return errors.empty();
    }
};

[[nodiscard]] GlbCollisionAsset
parseGlbCollisionJson(std::string_view json);

[[nodiscard]] GlbCollisionAsset
loadGlbCollisionAsset(std::string_view path);

} // namespace ian
