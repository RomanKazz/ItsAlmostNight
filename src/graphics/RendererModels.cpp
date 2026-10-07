#include "graphics/Renderer.hpp"
#include "graphics/CrystalMineModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "buildings/BuildingSystem.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

namespace ian {
namespace {

constexpr int PlatformLegCount = 4;
constexpr int PlatformTopMeshIndex = 4;
constexpr int PlatformMeshCount = PlatformTopMeshIndex + 1;
constexpr std::size_t PlatformDeckPart = 0U;
constexpr std::size_t PlatformMinimumXPart = 1U;
constexpr std::size_t PlatformMaximumXPart = 2U;
constexpr std::size_t PlatformMinimumZPart = 3U;
constexpr std::size_t PlatformMaximumZPart = 4U;
constexpr std::array<std::size_t, 4>
    RotatedPlatformEdgeIndices{1U, 0U, 3U, 2U};
constexpr float PlatformLegTopY = -0.12480831F;
constexpr float PlatformLegSpan = 3.87519169F;
constexpr std::array<std::size_t, PlatformLegCount>
    RotatedPlatformSupportIndices{1U, 0U, 3U, 2U};

BoundingBox transformedBoundingBox(
    const BoundingBox& bounds, Matrix transform) {
    BoundingBox result{};
    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 point{
            (corner & 1) != 0 ? bounds.max.x : bounds.min.x,
            (corner & 2) != 0 ? bounds.max.y : bounds.min.y,
            (corner & 4) != 0 ? bounds.max.z : bounds.min.z,
        };
        const Vector3 transformed =
            Vector3Transform(point, transform);
        if (corner == 0) {
            result.min = transformed;
            result.max = transformed;
            continue;
        }
        result.min.x = std::min(result.min.x, transformed.x);
        result.min.y = std::min(result.min.y, transformed.y);
        result.min.z = std::min(result.min.z, transformed.z);
        result.max.x = std::max(result.max.x, transformed.x);
        result.max.y = std::max(result.max.y, transformed.y);
        result.max.z = std::max(result.max.z, transformed.z);
    }
    return result;
}

std::optional<double> rayCylinderDistance(
    Ray ray, const BoundingBox& bounds, double maxDistance) {
    const double centerX =
        (static_cast<double>(bounds.min.x) + bounds.max.x) * 0.5;
    const double centerZ =
        (static_cast<double>(bounds.min.z) + bounds.max.z) * 0.5;
    const double radius = std::max(
        static_cast<double>(bounds.max.x - bounds.min.x),
        static_cast<double>(bounds.max.z - bounds.min.z)) * 0.5;
    const double offsetX = ray.position.x - centerX;
    const double offsetZ = ray.position.z - centerZ;
    const double a = ray.direction.x * ray.direction.x +
                     ray.direction.z * ray.direction.z;
    const double b = 2.0 *
                     (offsetX * ray.direction.x +
                      offsetZ * ray.direction.z);
    const double c = offsetX * offsetX + offsetZ * offsetZ -
                     radius * radius;
    std::optional<double> closest;
    const auto accept = [&](double distance) {
        if (distance < 0.0 || distance > maxDistance) {
            return;
        }
        const double y = ray.position.y +
                         ray.direction.y * distance;
        if (y >= bounds.min.y && y <= bounds.max.y &&
            (!closest || distance < *closest)) {
            closest = distance;
        }
    };
    const double discriminant = b * b - 4.0 * a * c;
    if (a > 1e-12 && discriminant >= 0.0) {
        const double root = std::sqrt(discriminant);
        accept((-b - root) / (2.0 * a));
        accept((-b + root) / (2.0 * a));
    }
    if (std::abs(ray.direction.y) > 1e-12) {
        for (const float capY : {bounds.min.y, bounds.max.y}) {
            const double distance =
                (static_cast<double>(capY) - ray.position.y) /
                ray.direction.y;
            const double x = offsetX + ray.direction.x * distance;
            const double z = offsetZ + ray.direction.z * distance;
            if (distance >= 0.0 && distance <= maxDistance &&
                x * x + z * z <= radius * radius &&
                (!closest || distance < *closest)) {
                closest = distance;
            }
        }
    }
    return closest;
}

std::optional<double> modelColliderRaycastDistanceImpl(
    const ModelResource& resource, Matrix transform,
    Ray ray, double maxDistance) {
    const auto accept =
        [maxDistance](RayCollision collision)
            -> std::optional<double> {
            if (!collision.hit || collision.distance < 0.0F ||
                static_cast<double>(collision.distance) > maxDistance) {
                return std::nullopt;
            }
            return static_cast<double>(collision.distance);
        };

    std::optional<double> closest;
    const auto addBounds = [&](const BoundingBox& bounds,
                               ModelColliderType type) {
        const BoundingBox worldBounds =
            transformedBoundingBox(bounds, transform);
        std::optional<double> distance;
        if (type == ModelColliderType::Cylinder) {
            distance = rayCylinderDistance(
                ray, worldBounds, maxDistance);
        } else if (type == ModelColliderType::Sphere) {
            const Vector3 center{
                (worldBounds.min.x + worldBounds.max.x) * 0.5F,
                (worldBounds.min.y + worldBounds.max.y) * 0.5F,
                (worldBounds.min.z + worldBounds.max.z) * 0.5F,
            };
            const float radius = std::max({
                worldBounds.max.x - center.x,
                worldBounds.max.y - center.y,
                worldBounds.max.z - center.z,
            });
            distance = accept(GetRayCollisionSphere(
                ray, center, radius));
        } else {
            distance = accept(GetRayCollisionBox(
                ray, worldBounds));
        }
        if (distance && (!closest || *distance < *closest)) {
            closest = distance;
        }
    };

    const auto& collisionAsset = resource.collisionAsset();
    if (!collisionAsset.colliders.empty()) {
        for (const ModelCollider& collider : collisionAsset.colliders) {
            addBounds(
                {{static_cast<float>(collider.minimum.x),
                  static_cast<float>(collider.minimum.y),
                  static_cast<float>(collider.minimum.z)},
                 {static_cast<float>(collider.maximum.x),
                  static_cast<float>(collider.maximum.y),
                  static_cast<float>(collider.maximum.z)}},
                collider.type);
        }
    } else {
        addBounds(resource.visualBounds(), ModelColliderType::Box);
    }
    return closest;
}

std::array<Matrix, PlatformLegCount + 1>
platformMeshTransforms(
    Vector3 topCenter, float scale,
    const std::array<float, 4>& supportLengths) {
    std::array<Matrix, PlatformLegCount + 1> transforms{};
    const Matrix worldTranslation = MatrixTranslate(
        topCenter.x, topCenter.y, topCenter.z);
    const Matrix modelRotation = MatrixRotateY(PI);
    transforms[PlatformTopMeshIndex] = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(scale, 1.0F, scale),
            modelRotation),
        worldTranslation);
    for (int legIndex = 0; legIndex < PlatformLegCount;
         ++legIndex) {
        const std::size_t supportIndex =
            RotatedPlatformSupportIndices[
                static_cast<std::size_t>(legIndex)];
        const float length = std::max(
            supportLengths[supportIndex],
            -PlatformLegTopY);
        const float verticalScale = std::max(
            (length + PlatformLegTopY) / PlatformLegSpan,
            0.01F);
        Matrix transform = MatrixTranslate(
            0.0F, -PlatformLegTopY, 0.0F);
        transform = MatrixMultiply(
            transform,
            MatrixScale(scale, verticalScale, scale));
        transform = MatrixMultiply(
            transform,
            MatrixTranslate(0.0F, PlatformLegTopY, 0.0F));
        transform = MatrixMultiply(transform, modelRotation);
        transforms[static_cast<std::size_t>(legIndex)] =
            MatrixMultiply(transform, worldTranslation);
    }
    return transforms;
}

} // namespace

std::optional<double>
renderer_model_detail::modelColliderRaycastDistance(
    const ModelResource& resource, Matrix transform,
    Ray ray, double maxDistance) {
    return modelColliderRaycastDistanceImpl(
        resource, transform, ray, maxDistance);
}

Shader* Renderer::activeModelShader(bool selectionEligible) {
    if (selectionEligible && selectionMaskPassOpen_ && resources_.selectionMaskShader().valid()) {
        return &resources_.selectionMaskShader().get();
    }
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        return &resources_.shadowShader().get();
    }
    if (worldShaderActive_ && resources_.worldShader().valid()) {
        return &resources_.worldShader().get();
    }
    return nullptr;
}

Shader* Renderer::prepareModelShader(Model& model, bool selectionEligible) {
    Shader* shader = activeModelShader(selectionEligible);
    if (shader != nullptr && model.materials != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    return shader;
}

using namespace renderer_model_detail;

void Renderer::shutdownPlatformTopPartMeshes() {
    for (Mesh& mesh : platformTopPartMeshes_) {
        if (mesh.vertexCount > 0) {
            UnloadMesh(mesh);
        }
        mesh = {};
    }
    platformTopPartMeshesReady_ = false;
}

void Renderer::initializePlatformTopPartMeshes() {
    shutdownPlatformTopPartMeshes();
    auto& resource = resources_.platformModel();
    if (!resource.valid() ||
        resource.get().meshCount <= PlatformTopMeshIndex) {
        return;
    }
    const Mesh& source =
        resource.get().meshes[PlatformTopMeshIndex];
    if (source.vertices == nullptr || source.triangleCount <= 0) {
        return;
    }

    // PlatformTop contains eleven disconnected wooden pieces in one authored
    // mesh: the deck/cross-beams and four deep perimeter rails. Split those
    // rails once so adjacent 2x2 tiles can omit the two internal copies while
    // retaining the original textured material.
    std::array<std::vector<int>, PlatformTopPartCount>
        sourceVertices;
    const auto vertexIndex = [&source](int indexOffset) {
        return source.indices != nullptr
            ? static_cast<int>(source.indices[indexOffset])
            : indexOffset;
    };
    for (int triangle = 0; triangle < source.triangleCount;
         ++triangle) {
        std::array<int, 3> indices{};
        Vector3 center{};
        float maximumY = -std::numeric_limits<float>::infinity();
        for (int corner = 0; corner < 3; ++corner) {
            const int index = vertexIndex(triangle * 3 + corner);
            indices[static_cast<std::size_t>(corner)] = index;
            const int coordinate = index * 3;
            center.x += source.vertices[coordinate];
            center.y += source.vertices[coordinate + 1];
            center.z += source.vertices[coordinate + 2];
            maximumY = std::max(
                maximumY, source.vertices[coordinate + 1]);
        }
        center = Vector3Scale(center, 1.0F / 3.0F);
        std::size_t part = PlatformDeckPart;
        if (maximumY < -0.28F) {
            if (std::abs(center.x) > std::abs(center.z)) {
                part = center.x < 0.0F
                    ? PlatformMinimumXPart
                    : PlatformMaximumXPart;
            } else {
                part = center.z < 0.0F
                    ? PlatformMinimumZPart
                    : PlatformMaximumZPart;
            }
        }
        sourceVertices[part].insert(
            sourceVertices[part].end(),
            indices.begin(), indices.end());
    }

    const auto copyAttribute = [](float* destination,
                                  const float* sourceAttribute,
                                  int sourceIndex,
                                  int componentCount) {
        for (int component = 0; component < componentCount;
             ++component) {
            destination[component] =
                sourceAttribute[sourceIndex * componentCount + component];
        }
    };
    for (std::size_t part = 0; part < sourceVertices.size(); ++part) {
        const std::vector<int>& indices = sourceVertices[part];
        if (indices.empty()) {
            shutdownPlatformTopPartMeshes();
            return;
        }
        Mesh& mesh = platformTopPartMeshes_[part];
        mesh.vertexCount = static_cast<int>(indices.size());
        mesh.triangleCount = mesh.vertexCount / 3;
        mesh.vertices = static_cast<float*>(MemAlloc(
            static_cast<unsigned int>(
                static_cast<std::size_t>(mesh.vertexCount) *
                3U * sizeof(float))));
        if (source.normals != nullptr) {
            mesh.normals = static_cast<float*>(MemAlloc(
                static_cast<unsigned int>(
                    static_cast<std::size_t>(mesh.vertexCount) *
                    3U * sizeof(float))));
        }
        if (source.texcoords != nullptr) {
            mesh.texcoords = static_cast<float*>(MemAlloc(
                static_cast<unsigned int>(
                    static_cast<std::size_t>(mesh.vertexCount) *
                    2U * sizeof(float))));
        }
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const int sourceIndex = indices[static_cast<std::size_t>(vertex)];
            copyAttribute(
                &mesh.vertices[vertex * 3], source.vertices,
                sourceIndex, 3);
            if (mesh.normals != nullptr) {
                copyAttribute(
                    &mesh.normals[vertex * 3], source.normals,
                    sourceIndex, 3);
            }
            if (mesh.texcoords != nullptr) {
                copyAttribute(
                    &mesh.texcoords[vertex * 2], source.texcoords,
                    sourceIndex, 2);
            }
        }
        UploadMesh(&mesh, false);
    }
    platformTopPartMeshesReady_ = true;
}

void Renderer::drawTerrainAlignedModel(
    Model& model, Vector3 position, float yawRadians,
    Vector3 scale, Color tint) const {
    const Matrix rotation = terrainAlignedRotation(
        position.x, position.z, yawRadians);
    Vector3 axis{0.0F, 1.0F, 0.0F};
    float angle = 0.0F;
    QuaternionToAxisAngle(
        QuaternionFromMatrix(rotation), &axis, &angle);
    if (Vector3LengthSqr(axis) <= 0.0001F) {
        axis = {0.0F, 1.0F, 0.0F};
        angle = 0.0F;
    }
    DrawModelEx(
        model, position, axis, angle * RAD2DEG,
        scale, tint);
}

Color lootRarityColor(LootRarity rarity) {
    // The three tiers intentionally read as blue -> crystals -> red in-world:
    // common, rare (Uncommon in the data model), legendary (Rare).
    if (rarity == LootRarity::Legendary)
        return {255, 126, 38, 255};
    if (rarity == LootRarity::Rare)
        return {255, 170, 170, 255};
    if (rarity == LootRarity::Uncommon)
        return {255, 228, 148, 255};
    return {185, 225, 255, 255};
}

std::optional<double>
Renderer::platformFrameRaycastDistance(
    Vector3 topCenter, float scale,
    const std::array<float, 4>& supportLengths,
    Ray ray, double maxDistance,
    const std::array<bool, 4>& supportVisibility) {
    auto& resource = resources_.platformModel();
    if (!resource.valid() || maxDistance <= 0.0 ||
        resource.get().meshCount <= PlatformTopMeshIndex) {
        return std::nullopt;
    }
    const Model& model = resource.get();
    const auto meshBounds = resource.meshBounds();
    if (meshBounds.size() <
        static_cast<std::size_t>(PlatformMeshCount)) {
        return std::nullopt;
    }
    const auto transforms = platformMeshTransforms(
        topCenter, scale, supportLengths);
    std::optional<double> closest;
    for (int meshIndex = 0; meshIndex < PlatformMeshCount;
         ++meshIndex) {
        if (meshIndex < PlatformLegCount &&
            !supportVisibility[
                RotatedPlatformSupportIndices[
                    static_cast<std::size_t>(meshIndex)]]) {
            continue;
        }
        const Matrix transform = MatrixMultiply(
            model.transform,
            transforms[static_cast<std::size_t>(meshIndex)]);
        const BoundingBox worldBounds = transformedBoundingBox(
            meshBounds[static_cast<std::size_t>(meshIndex)],
            transform);
        const RayCollision boundsHit =
            GetRayCollisionBox(ray, worldBounds);
        if (!boundsHit.hit || boundsHit.distance < 0.0F ||
            static_cast<double>(boundsHit.distance) >
                maxDistance) {
            continue;
        }
        const double distance = boundsHit.distance;
        if (!closest || distance < *closest) {
            closest = distance;
        }
    }
    return closest;
}

std::optional<double> Renderer::rampRaycastDistance(
    Vector3 footprintCenter, float yawRadians,
    float scale, Ray ray, double maxDistance) {
    auto& resource = resources_.rampModel();
    if (!resource.valid() || maxDistance <= 0.0) {
        return std::nullopt;
    }
    const Model& model = resource.get();
    const Matrix transform = MatrixMultiply(
        model.transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(scale, scale, scale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(
                footprintCenter.x,
                footprintCenter.y,
                footprintCenter.z)));
    const Matrix inverse = MatrixInvert(transform);
    const Vector3 localOrigin =
        Vector3Transform(ray.position, inverse);
    const Vector3 localEnd = Vector3Transform(
        Vector3Add(ray.position, ray.direction), inverse);
    const Vector3 localDirection =
        Vector3Subtract(localEnd, localOrigin);

    bool hasSlopeCollider = false;
    for (const ModelCollider& collider :
         resource.collisionAsset().colliders) {
        if (collider.type != ModelColliderType::Slope) {
            continue;
        }
        hasSlopeCollider = true;
        double nearDistance = 0.0;
        double farDistance = maxDistance;
        const auto clip =
            [&nearDistance, &farDistance](
                double value, double delta,
                double minimum, double maximum) {
                constexpr double Epsilon = 1e-9;
                if (std::abs(delta) <= Epsilon) {
                    return value >= minimum && value <= maximum;
                }
                double first = (minimum - value) / delta;
                double second = (maximum - value) / delta;
                if (first > second) {
                    std::swap(first, second);
                }
                nearDistance = std::max(nearDistance, first);
                farDistance = std::min(farDistance, second);
                return nearDistance <= farDistance;
            };
        if (!clip(
                localOrigin.x, localDirection.x,
                collider.minimum.x, collider.maximum.x) ||
            !clip(
                localOrigin.z, localDirection.z,
                collider.minimum.z, collider.maximum.z)) {
            continue;
        }

        const double run =
            collider.maximum.z - collider.minimum.z;
        if (run <= 1e-9) {
            continue;
        }
        const double rise = collider.maximum.y;
        const double progressOrigin =
            (localOrigin.z - collider.minimum.z) / run;
        const double progressDirection =
            localDirection.z / run;
        const double surfaceGapOrigin =
            localOrigin.y - rise * progressOrigin;
        const double surfaceGapDirection =
            localDirection.y - rise * progressDirection;
        if (clip(
                surfaceGapOrigin, surfaceGapDirection,
                collider.minimum.y, 0.0)) {
            return nearDistance;
        }
    }
    if (hasSlopeCollider) {
        return std::nullopt;
    }
    return modelColliderRaycastDistance(
        resource, transform, ray, maxDistance);
}

bool Renderer::drawPlatformFrameModel(
    Vector3 topCenter, Color tint, float scale,
    const std::array<float, 4>& supportLengths,
    const std::array<bool, 4>& supportVisibility,
    const std::array<bool, 4>& edgeVisibility) {
    auto& resource = resources_.platformModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    if (model.meshCount <= PlatformTopMeshIndex) {
        return false;
    }
    prepareModelShader(model);
    const auto drawSpecificMesh = [&model, tint](
                                      const Mesh& mesh,
                                      int materialIndex,
                                      Matrix transform) {
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(
            mesh, material,
            MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };
    const auto drawMesh = [&model, &drawSpecificMesh](
                              int meshIndex,
                              Matrix transform) {
        drawSpecificMesh(
            model.meshes[meshIndex],
            model.meshMaterial[meshIndex], transform);
    };

    const auto transforms = platformMeshTransforms(
        topCenter, scale, supportLengths);
    if (platformTopPartMeshesReady_) {
        const int topMaterial =
            model.meshMaterial[PlatformTopMeshIndex];
        drawSpecificMesh(
            platformTopPartMeshes_[PlatformDeckPart],
            topMaterial, transforms[PlatformTopMeshIndex]);
        constexpr std::array<std::size_t, 4> EdgeParts{
            PlatformMinimumXPart, PlatformMaximumXPart,
            PlatformMinimumZPart, PlatformMaximumZPart};
        for (std::size_t localEdge = 0;
             localEdge < EdgeParts.size(); ++localEdge) {
            if (!edgeVisibility[
                    RotatedPlatformEdgeIndices[localEdge]]) {
                continue;
            }
            drawSpecificMesh(
                platformTopPartMeshes_[EdgeParts[localEdge]],
                topMaterial, transforms[PlatformTopMeshIndex]);
        }
    } else {
        drawMesh(
            PlatformTopMeshIndex,
            transforms[PlatformTopMeshIndex]);
    }
    for (int legIndex = 0; legIndex < PlatformLegCount;
         ++legIndex) {
        if (!supportVisibility[
                RotatedPlatformSupportIndices[
                    static_cast<std::size_t>(legIndex)]]) {
            continue;
        }
        drawMesh(
            legIndex,
            transforms[static_cast<std::size_t>(legIndex)]);
    }
    return true;
}

bool Renderer::drawRampModel(
    Vector3 footprintCenter, float yawRadians,
    Color tint, float scale) {
    auto& resource = resources_.rampModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);

    DrawModelEx(
        model, footprintCenter,
        {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {scale, scale, scale}, tint);
    return true;
}

bool Renderer::drawRampSupportBeam(
    Vector3 baseCenter, float yawRadians,
    Color tint, float scale) {
    auto& resource = resources_.rampModel();
    if (!resource.valid() ||
        !IsModelValid(rampSupportBeamModel_) ||
        scale <= 0.0F) {
        return false;
    }
    Model& ramp = resource.get();
    if (ramp.meshCount <= 0 ||
        ramp.materialCount <= 0) {
        return false;
    }
    Shader* shader = activeModelShader();
    const int rampMeshIndex = ramp.meshCount - 1;
    const int materialIndex =
        ramp.meshMaterial[rampMeshIndex];
    if (materialIndex < 0 ||
        materialIndex >= ramp.materialCount) {
        return false;
    }
    Material& material = ramp.materials[materialIndex];
    if (shader != nullptr) {
        material.shader = *shader;
    }
    const Color original =
        material.maps[MATERIAL_MAP_DIFFUSE].color;
    material.maps[MATERIAL_MAP_DIFFUSE].color =
        ColorTint(original, tint);
    const Matrix transform = MatrixMultiply(
        MatrixScale(scale, scale, scale),
        MatrixMultiply(
            MatrixRotateY(yawRadians),
            MatrixTranslate(
                baseCenter.x, baseCenter.y,
                baseCenter.z)));
    DrawMesh(
        rampSupportBeamModel_.meshes[0],
        material, transform);
    material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    return true;
}

bool Renderer::drawResourceProducer(
    BuildingType type, Vector3 position, float yawRadians,
    Color tint, float scale) {
    ModelResource* resource = nullptr;
    float modelScale = 1.0F;
    if (type == BuildingType::CrystalMine) {
        resource = &resources_.mineModel();
        modelScale = 2.1F;
    } else if (type == BuildingType::LumberMill) {
        resource = &resources_.lumberMillModel();
        modelScale = 1.65F;
    } else if (type == BuildingType::Quarry) {
        resource = &resources_.quarryModel();
        modelScale = 1.3F;
    }
    if (resource == nullptr || !resource->valid()) {
        return false;
    }
    Model& model = resource->get();
    prepareModelShader(model);
    position.y += 0.005F;
    if (type == BuildingType::CrystalMine && model.meshCount == 2) {
        const float uniformScale = modelScale * scale * 0.5F;
        const Matrix transform = MatrixMultiply(
            model.transform,
            MatrixMultiply(MatrixScale(uniformScale, uniformScale, uniformScale),
                MatrixMultiply(MatrixRotateY(yawRadians),
                    MatrixTranslate(position.x, position.y, position.z))));
        const double seconds = GetTime();
        for (int index = 0; index < model.meshCount; ++index) {
            Material& material = model.materials[model.meshMaterial[index]];
            const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
            DrawMesh(model.meshes[index], material,
                MatrixMultiply(crystal_mine_model_detail::meshTransform(index, seconds), transform));
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        }
        return true;
    }
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {modelScale * scale * 0.5F, modelScale * scale * 0.5F,
         modelScale * scale * 0.5F},
        tint);
    return true;
}

bool Renderer::drawSpikeTrap(
    Vector3 position, float yawRadians, float animationSeconds,
    Color tint, float scaleFactor) {
    auto& resource = resources_.spikeTrapModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);

    constexpr float ModelScale = 1.2F;
    constexpr float HiddenOffset = -0.15F;
    constexpr float AnimationDuration = 1.2F;
    float exposure = 0.0F;
    // This GLB animates an unskinned child node, which raylib does not expose
    // through ModelAnimation. Reproduce its authored show-hide translation:
    // 0.2 s rise, 0.8 s hold, 0.2 s retract.
    if (animationSeconds >= 0.0F &&
        animationSeconds < AnimationDuration) {
        constexpr float ShowSeconds = 0.20F;
        constexpr float HideStartSeconds = 1.0F;
        const auto authoredEase = [](float t) {
            const float rise = t * t * t;
            const float fall =
                (1.0F - t) * (1.0F - t) * (1.0F - t);
            return rise / std::max(rise + fall, 0.0001F);
        };
        if (animationSeconds < ShowSeconds) {
            const float t = animationSeconds / ShowSeconds;
            exposure = authoredEase(t);
        } else if (animationSeconds < HideStartSeconds) {
            exposure = 1.0F;
        } else {
            const float t = std::clamp(
                (animationSeconds - HideStartSeconds) /
                    (AnimationDuration - HideStartSeconds),
                0.0F, 1.0F);
            exposure = 1.0F - authoredEase(t);
        }
    }
    const float spikeOffset = HiddenOffset * (1.0F - exposure);
    position.y += 0.005F;
    const float scale = ModelScale * scaleFactor;
    const Matrix scaleMatrix = MatrixScale(scale, scale, scale);
    const Matrix rotation = MatrixRotateY(yawRadians);
    const auto transformAt = [&](float y) {
        return MatrixMultiply(
            model.transform,
            MatrixMultiply(
                MatrixMultiply(scaleMatrix, rotation),
                MatrixTranslate(position.x, y, position.z)));
    };
    const auto drawMesh = [&model, tint](int meshIndex,
                                         Matrix transform) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material, transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };
    if (model.meshCount < 2) {
        DrawModelEx(
            model, position, {0.0F, 1.0F, 0.0F},
            yawRadians * RAD2DEG, {scale, scale, scale}, tint);
        return true;
    }
    drawMesh(0, transformAt(position.y));
    drawMesh(
        1, transformAt(position.y + spikeOffset * scale));
    for (int meshIndex = 2; meshIndex < model.meshCount;
         ++meshIndex) {
        drawMesh(meshIndex, transformAt(position.y));
    }
    return true;
}

bool Renderer::drawWall(Vector3 position,
                        std::uint8_t connectionMask,
                        float yawRadians, Color tint,
                        float scale) {
    constexpr std::uint8_t AllConnections =
        WallConnectionNorth | WallConnectionEast |
        WallConnectionSouth | WallConnectionWest;
    connectionMask &= AllConnections;

    ModelResource* resource = nullptr;
    float modelYaw = 0.0F;
    const int connectionCount =
        std::popcount(connectionMask);

    if (connectionCount == 0) {
        resource = &resources_.wallIsolatedModel();
        modelYaw = yawRadians;
    } else if (connectionCount == 1) {
        resource = &resources_.wallEndModel();
        if ((connectionMask & WallConnectionNorth) != 0U) {
            modelYaw = PI * 0.5F;
        } else if (
            (connectionMask & WallConnectionWest) != 0U) {
            modelYaw = PI;
        } else if (
            (connectionMask & WallConnectionSouth) != 0U) {
            modelYaw = -PI * 0.5F;
        }
    } else if (connectionCount == 2) {
        const bool northSouth =
            connectionMask ==
            (WallConnectionNorth | WallConnectionSouth);
        const bool eastWest =
            connectionMask ==
            (WallConnectionEast | WallConnectionWest);
        if (northSouth || eastWest) {
            resource = &resources_.wallIsolatedModel();
            modelYaw = northSouth ? PI * 0.5F : 0.0F;
        } else {
            resource = &resources_.wallCornerModel();
            if (connectionMask ==
                (WallConnectionNorth | WallConnectionEast)) {
                modelYaw = -PI * 0.5F;
            } else if (
                connectionMask ==
                (WallConnectionEast | WallConnectionSouth)) {
                modelYaw = PI;
            } else if (
                connectionMask ==
                (WallConnectionSouth | WallConnectionWest)) {
                modelYaw = PI * 0.5F;
            }
        }
    } else if (connectionCount == 3) {
        resource = &resources_.wallTModel();
        const std::uint8_t missing =
            AllConnections ^ connectionMask;
        if (missing == WallConnectionEast) {
            modelYaw = PI * 0.5F;
        } else if (missing == WallConnectionNorth) {
            modelYaw = PI;
        } else if (missing == WallConnectionWest) {
            modelYaw = -PI * 0.5F;
        }
    } else {
        resource = &resources_.wallCrossModel();
    }

    if (resource == nullptr || !resource->valid()) {
        return false;
    }
    Model& model = resource->get();
    prepareModelShader(model);

    position.y += 0.005F;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        modelYaw * RAD2DEG, {scale, scale, scale}, tint);
    return true;
}

bool Renderer::drawGate(
    Vector3 position, float yawRadians, float openAmount,
    Color tint, float scale) {
    ModelResource& resource = resources_.gateModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    if (model.meshes == nullptr || model.meshMaterial == nullptr ||
        model.materials == nullptr || model.meshCount < 2) {
        return false;
    }

    Shader* shader = activeModelShader();

    position.y += 0.005F;
    const Matrix baseTransform = MatrixMultiply(
        model.transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(scale, scale, scale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    const Matrix doorTransform = MatrixMultiply(
        gate_model_detail::doorHingeTransform(openAmount),
        baseTransform);

    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(
                static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 ||
            materialIndex >= model.materialCount) {
            continue;
        }
        Material material = model.materials[materialIndex];
        if (shader != nullptr) {
            material.shader = *shader;
        }
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(
            model.meshes[meshIndex], material,
            meshIndex == gate_model_detail::DoorMeshIndex
                ? doorTransform
                : baseTransform);
        // Material is a shallow copy: its map array still belongs to the
        // shared gate model. Restore the tint so translucent sale/preview
        // draws cannot leak into every subsequently rendered gate.
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

} // namespace ian
