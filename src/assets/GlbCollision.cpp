#include "assets/GlbCollision.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>

namespace ian {
namespace {

using Json = nlohmann::json;

struct Matrix4 {
    std::array<double, 16> values{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };
};

std::uint32_t readLittleEndian32(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset) {
    if (offset + 4U > bytes.size()) {
        throw std::runtime_error("truncated GLB header");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

Matrix4 multiply(const Matrix4& left,
                 const Matrix4& right) {
    Matrix4 result{{}};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            double value = 0.0;
            for (int index = 0; index < 4; ++index) {
                value +=
                    left.values[static_cast<std::size_t>(
                        index * 4 + row)] *
                    right.values[static_cast<std::size_t>(
                        column * 4 + index)];
            }
            result.values[static_cast<std::size_t>(
                column * 4 + row)] = value;
        }
    }
    return result;
}

Vec3 transformPoint(const Matrix4& matrix, Vec3 point) {
    return {
        matrix.values[0] * point.x +
            matrix.values[4] * point.y +
            matrix.values[8] * point.z +
            matrix.values[12],
        matrix.values[1] * point.x +
            matrix.values[5] * point.y +
            matrix.values[9] * point.z +
            matrix.values[13],
        matrix.values[2] * point.x +
            matrix.values[6] * point.y +
            matrix.values[10] * point.z +
            matrix.values[14],
    };
}

Matrix4 nodeMatrix(const Json& node) {
    if (node.contains("matrix")) {
        const auto& source = node.at("matrix");
        if (!source.is_array() || source.size() != 16U) {
            throw std::runtime_error(
                "node matrix must contain 16 numbers");
        }
        Matrix4 result;
        for (std::size_t index = 0;
             index < result.values.size(); ++index) {
            result.values[index] =
                source.at(index).get<double>();
        }
        return result;
    }

    const auto vectorValue =
        [&node](std::string_view key,
                std::array<double, 3> fallback) {
            const auto iterator = node.find(key);
            if (iterator == node.end()) {
                return fallback;
            }
            if (!iterator->is_array() ||
                iterator->size() != 3U) {
                throw std::runtime_error(
                    std::string(key) +
                    " must contain 3 numbers");
            }
            for (std::size_t index = 0;
                 index < fallback.size(); ++index) {
                fallback[index] =
                    iterator->at(index).get<double>();
            }
            return fallback;
        };
    const auto translation = vectorValue(
        "translation", {0.0, 0.0, 0.0});
    const auto scale = vectorValue(
        "scale", {1.0, 1.0, 1.0});
    std::array<double, 4> rotation{0.0, 0.0, 0.0, 1.0};
    if (const auto iterator = node.find("rotation");
        iterator != node.end()) {
        if (!iterator->is_array() ||
            iterator->size() != 4U) {
            throw std::runtime_error(
                "rotation must contain 4 numbers");
        }
        for (std::size_t index = 0;
             index < rotation.size(); ++index) {
            rotation[index] =
                iterator->at(index).get<double>();
        }
    }
    const double x = rotation[0];
    const double y = rotation[1];
    const double z = rotation[2];
    const double w = rotation[3];
    Matrix4 result;
    result.values = {
        (1.0 - 2.0 * (y * y + z * z)) * scale[0],
        (2.0 * (x * y + z * w)) * scale[0],
        (2.0 * (x * z - y * w)) * scale[0],
        0.0,
        (2.0 * (x * y - z * w)) * scale[1],
        (1.0 - 2.0 * (x * x + z * z)) * scale[1],
        (2.0 * (y * z + x * w)) * scale[1],
        0.0,
        (2.0 * (x * z + y * w)) * scale[2],
        (2.0 * (y * z - x * w)) * scale[2],
        (1.0 - 2.0 * (x * x + y * y)) * scale[2],
        0.0,
        translation[0], translation[1], translation[2], 1.0,
    };
    return result;
}

std::string uppercase(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::toupper(character));
        });
    return value;
}

std::optional<ModelColliderType> colliderType(
    const Json& node, const std::string& upperName) {
    std::string typeName;
    if (const auto extras = node.find("extras");
        extras != node.end() && extras->is_object()) {
        if (const auto type = extras->find("collision_type");
            type != extras->end() && type->is_string()) {
            typeName = uppercase(type->get<std::string>());
        }
    }
    if (typeName.empty()) {
        if (upperName.starts_with("COL_BOX")) {
            typeName = "BOX";
        } else if (upperName.starts_with("COL_CYLINDER") ||
                   upperName.starts_with("COL_CYL_")) {
            typeName = "CYLINDER";
        } else if (upperName.starts_with("COL_SLOPE") ||
                   upperName.starts_with("COL_RAMP")) {
            typeName = "SLOPE";
        } else {
            return std::nullopt;
        }
    }
    if (typeName == "BOX") {
        return ModelColliderType::Box;
    }
    if (typeName == "CYLINDER") {
        return ModelColliderType::Cylinder;
    }
    if (typeName == "SLOPE" || typeName == "RAMP") {
        return ModelColliderType::Slope;
    }
    return std::nullopt;
}

bool colliderWalkable(const Json& node,
                      const std::string& upperName) {
    if (const auto extras = node.find("extras");
        extras != node.end() && extras->is_object()) {
        if (const auto walkable = extras->find("walkable");
            walkable != extras->end() &&
            walkable->is_boolean()) {
            return walkable->get<bool>();
        }
    }
    return upperName.find("_WALK") != std::string::npos;
}

struct MeshBounds {
    Vec3 minimum{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
    };
    Vec3 maximum{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    std::vector<std::size_t> renderMeshIndices;
    bool valid{};
};

MeshBounds meshBounds(
    const Json& document, std::size_t meshIndex,
    std::size_t renderMeshOffset) {
    const auto& meshes = document.at("meshes");
    const auto& accessors = document.at("accessors");
    const auto& primitives =
        meshes.at(meshIndex).at("primitives");
    MeshBounds result;
    std::size_t triangleOffset = 0U;
    for (std::size_t primitiveIndex = 0;
         primitiveIndex < primitives.size(); ++primitiveIndex) {
        const auto& primitive = primitives.at(primitiveIndex);
        constexpr int TrianglesMode = 4;
        if (primitive.value("mode", TrianglesMode) !=
            TrianglesMode) {
            continue;
        }
        const std::size_t renderMeshIndex =
            renderMeshOffset + triangleOffset;
        ++triangleOffset;
        const auto& attributes = primitive.at("attributes");
        if (!attributes.contains("POSITION")) {
            continue;
        }
        const std::size_t accessorIndex =
            attributes.at("POSITION").get<std::size_t>();
        const auto& accessor = accessors.at(accessorIndex);
        if (!accessor.contains("min") ||
            !accessor.contains("max")) {
            continue;
        }
        const auto& minimum = accessor.at("min");
        const auto& maximum = accessor.at("max");
        for (std::size_t axis = 0; axis < 3U; ++axis) {
            const double minimumValue =
                minimum.at(axis).get<double>();
            const double maximumValue =
                maximum.at(axis).get<double>();
            if (axis == 0U) {
                result.minimum.x = std::min(
                    result.minimum.x, minimumValue);
                result.maximum.x = std::max(
                    result.maximum.x, maximumValue);
            } else if (axis == 1U) {
                result.minimum.y = std::min(
                    result.minimum.y, minimumValue);
                result.maximum.y = std::max(
                    result.maximum.y, maximumValue);
            } else {
                result.minimum.z = std::min(
                    result.minimum.z, minimumValue);
                result.maximum.z = std::max(
                    result.maximum.z, maximumValue);
            }
        }
        result.renderMeshIndices.push_back(
            renderMeshIndex);
        result.valid = true;
    }
    return result;
}

std::pair<Vec3, Vec3> transformedBounds(
    Vec3 minimum, Vec3 maximum,
    const Matrix4& transform) {
    Vec3 transformedMinimum{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
    };
    Vec3 transformedMaximum{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 point{
            (corner & 1) != 0 ? maximum.x : minimum.x,
            (corner & 2) != 0 ? maximum.y : minimum.y,
            (corner & 4) != 0 ? maximum.z : minimum.z,
        };
        const Vec3 transformed =
            transformPoint(transform, point);
        transformedMinimum.x = std::min(
            transformedMinimum.x, transformed.x);
        transformedMinimum.y = std::min(
            transformedMinimum.y, transformed.y);
        transformedMinimum.z = std::min(
            transformedMinimum.z, transformed.z);
        transformedMaximum.x = std::max(
            transformedMaximum.x, transformed.x);
        transformedMaximum.y = std::max(
            transformedMaximum.y, transformed.y);
        transformedMaximum.z = std::max(
            transformedMaximum.z, transformed.z);
    }
    return {transformedMinimum, transformedMaximum};
}

} // namespace

GlbCollisionAsset
parseGlbCollisionJson(std::string_view json) {
    GlbCollisionAsset result;
    try {
        const Json document = Json::parse(json);
        const auto& nodes = document.at("nodes");
        const auto& meshes = document.at("meshes");

        // raylib emits meshes in node order, one mesh for every
        // triangle primitive. glTF mesh-array order is irrelevant.
        std::vector<std::size_t> nodePrimitiveOffsets(
            nodes.size(), 0U);
        std::size_t primitiveOffset = 0U;
        for (std::size_t nodeIndex = 0;
             nodeIndex < nodes.size(); ++nodeIndex) {
            nodePrimitiveOffsets[nodeIndex] = primitiveOffset;
            const auto& node = nodes.at(nodeIndex);
            if (!node.contains("mesh")) {
                continue;
            }
            const auto& primitives = meshes
                .at(node.at("mesh").get<std::size_t>())
                .at("primitives");
            primitiveOffset += static_cast<std::size_t>(
                std::count_if(
                    primitives.begin(), primitives.end(),
                    [](const Json& primitive) {
                        return primitive.value("mode", 4) == 4;
                    }));
        }

        std::vector<Matrix4> worldMatrices(nodes.size());
        std::vector<bool> visited(nodes.size(), false);
        const auto visit =
            [&](auto&& self, std::size_t nodeIndex,
                const Matrix4& parent) -> void {
                if (nodeIndex >= nodes.size()) {
                    throw std::runtime_error(
                        "node index is out of range");
                }
                const Matrix4 world = multiply(
                    parent, nodeMatrix(nodes.at(nodeIndex)));
                worldMatrices[nodeIndex] = world;
                visited[nodeIndex] = true;
                if (const auto children =
                        nodes.at(nodeIndex).find("children");
                    children != nodes.at(nodeIndex).end()) {
                    for (const auto& child : *children) {
                        self(self, child.get<std::size_t>(), world);
                    }
                }
            };
        if (document.contains("scenes")) {
            const std::size_t sceneIndex =
                document.value("scene", 0U);
            const auto& scene =
                document.at("scenes").at(sceneIndex);
            for (const auto& root : scene.at("nodes")) {
                visit(visit, root.get<std::size_t>(), Matrix4{});
            }
        }
        for (std::size_t nodeIndex = 0;
             nodeIndex < nodes.size(); ++nodeIndex) {
            if (!visited[nodeIndex]) {
                visit(visit, nodeIndex, Matrix4{});
            }
        }

        for (std::size_t nodeIndex = 0;
             nodeIndex < nodes.size(); ++nodeIndex) {
            const auto& node = nodes.at(nodeIndex);
            const std::string name =
                node.value("name", std::string{});
            const std::string upperName = uppercase(name);
            const auto type = colliderType(node, upperName);
            if (!type || !node.contains("mesh")) {
                continue;
            }
            const std::size_t meshIndex =
                node.at("mesh").get<std::size_t>();
            const MeshBounds bounds = meshBounds(
                document, meshIndex,
                nodePrimitiveOffsets[nodeIndex]);
            if (!bounds.valid) {
                result.errors.push_back(
                    name + " has no POSITION bounds");
                continue;
            }
            const auto [minimum, maximum] =
                transformedBounds(
                    bounds.minimum, bounds.maximum,
                    worldMatrices[nodeIndex]);
            result.colliders.push_back({
                .name = name,
                .type = *type,
                .walkable = colliderWalkable(
                    node, upperName),
                .minimum = minimum,
                .maximum = maximum,
            });
            result.renderMeshIndices.insert(
                result.renderMeshIndices.end(),
                bounds.renderMeshIndices.begin(),
                bounds.renderMeshIndices.end());
        }
        std::sort(
            result.renderMeshIndices.begin(),
            result.renderMeshIndices.end());
        result.renderMeshIndices.erase(
            std::unique(
                result.renderMeshIndices.begin(),
                result.renderMeshIndices.end()),
            result.renderMeshIndices.end());
    } catch (const std::exception& error) {
        result.errors.push_back(error.what());
    }
    return result;
}

GlbCollisionAsset
loadGlbCollisionAsset(std::string_view path) {
    GlbCollisionAsset result;
    try {
        std::ifstream stream(
            std::string(path),
            std::ios::binary | std::ios::ate);
        if (!stream) {
            result.errors.push_back(
                "could not open " + std::string(path));
            return result;
        }
        const std::streamsize size = stream.tellg();
        if (size < 20) {
            result.errors.push_back("GLB file is too small");
            return result;
        }
        stream.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(size));
        if (!stream.read(
                reinterpret_cast<char*>(bytes.data()), size)) {
            result.errors.push_back("could not read GLB file");
            return result;
        }
        constexpr std::uint32_t GlbMagic = 0x46546C67U;
        constexpr std::uint32_t JsonChunk = 0x4E4F534AU;
        if (readLittleEndian32(bytes, 0U) != GlbMagic ||
            readLittleEndian32(bytes, 4U) != 2U) {
            result.errors.push_back("unsupported GLB header");
            return result;
        }
        const std::size_t declaredLength =
            readLittleEndian32(bytes, 8U);
        if (declaredLength > bytes.size()) {
            result.errors.push_back("truncated GLB file");
            return result;
        }
        std::size_t offset = 12U;
        while (offset + 8U <= declaredLength) {
            const std::size_t chunkLength =
                readLittleEndian32(bytes, offset);
            const std::uint32_t chunkType =
                readLittleEndian32(bytes, offset + 4U);
            offset += 8U;
            if (offset + chunkLength > declaredLength) {
                result.errors.push_back("truncated GLB chunk");
                return result;
            }
            if (chunkType == JsonChunk) {
                const auto* characters =
                    reinterpret_cast<const char*>(
                        bytes.data() + offset);
                return parseGlbCollisionJson(
                    std::string_view(characters, chunkLength));
            }
            offset += chunkLength;
        }
        result.errors.push_back("GLB has no JSON chunk");
    } catch (const std::exception& error) {
        result.errors.push_back(error.what());
    }
    return result;
}

} // namespace ian
