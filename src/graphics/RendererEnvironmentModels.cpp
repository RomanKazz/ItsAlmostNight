#include "graphics/Renderer.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"


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

using namespace renderer_model_detail;

bool Renderer::drawRock(Vector3 position, Color tint,
                        float scale, std::size_t visualVariant,
                        float yawRadians) {
    const std::size_t variant =
        visualVariant % StoneVisualVariantCount;
    auto& resource = resources_.rockModel(variant);
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    scale *= worldRevealScaleAt(
        {position.x, position.z});
    if (scale <= 0.001F) {
        return true;
    }
    position.y += rockGroundOffset(variant) * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {RockModelScale * scale, RockModelScale * scale,
         RockModelScale * scale},
        tint);
    return true;
}

bool Renderer::drawCrystalResource(
    Vector3 position, Color tint, float scale,
    float yawRadians, Vector3 surfaceNormal) {
    auto& resource = resources_.crystalResourceModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    scale *= worldRevealScaleAt({position.x, position.z});
    if (scale <= 0.001F) return true;
    const float modelScale =
        static_cast<float>(CrystalVisualModelScale) * scale;
    position.y +=
        static_cast<float>(CrystalVisualGroundOffset) * modelScale;
    surfaceNormal = Vector3Normalize(surfaceNormal);
    if (Vector3LengthSqr(surfaceNormal) < 0.5F) {
        surfaceNormal = {0.0F, 1.0F, 0.0F};
    }
    const Quaternion groundAlignment =
        QuaternionFromVector3ToVector3(
            {0.0F, 1.0F, 0.0F}, surfaceNormal);
    const Quaternion surfaceYaw = QuaternionFromAxisAngle(
        surfaceNormal, yawRadians);
    const Quaternion rotation = QuaternionMultiply(
        surfaceYaw, groundAlignment);
    Vector3 rotationAxis{0.0F, 1.0F, 0.0F};
    float rotationAngle = 0.0F;
    QuaternionToAxisAngle(
        rotation, &rotationAxis, &rotationAngle);
    DrawModelEx(
        model, position, rotationAxis,
        rotationAngle * RAD2DEG,
        {modelScale, modelScale, modelScale}, tint);
    return true;
}

bool Renderer::drawDestructibleProp(
    ResourceType type, Vector3 position, float yawRadians,
    Color tint, float scale) {
    if (!isDestructibleProp(type)) return false;
    ModelResource& resource = resources_.destructiblePropModel(
        propModelIndex(type));
    if (!resource.valid()) return false;
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ && resources_.selectionMaskShader().valid())
        shader = &resources_.selectionMaskShader().get();
    else if (shadowPassOpen_ && resources_.shadowShader().valid())
        shader = &resources_.shadowShader().get();
    else if (worldShaderActive_ && resources_.worldShader().valid())
        shader = &resources_.worldShader().get();
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index)
            model.materials[index].shader = *shader;
    }
    scale *= worldRevealScaleAt({position.x, position.z});
    if (scale <= 0.001F) return true;
    drawTerrainAlignedModel(
        model, position, yawRadians, {scale, scale, scale}, tint);
    return true;
}

BoundingBox Renderer::destructiblePropWorldBounds(
    ResourceType type, Vector3 position, float yawRadians, float scale) {
    if (!isDestructibleProp(type)) return {};
    ModelResource& resource = resources_.destructiblePropModel(
        propModelIndex(type));
    if (!resource.valid()) return {};
    const Matrix rotation = terrainAlignedRotation(
        position.x, position.z, yawRadians);
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(MatrixScale(scale, scale, scale),
                       MatrixMultiply(rotation,
                                      MatrixTranslate(position.x, position.y,
                                                      position.z))));
    return world_transforms::transformBounds(resource.visualBounds(), transform);
}

bool Renderer::drawRocksInstanced(
    std::span<const RockDrawInstance> instances) {
    const bool shadowInstancing = shadowPassOpen_ &&
        resources_.shadowShader().valid();
    const bool worldInstancing = worldShaderActive_ &&
        resources_.worldShader().valid();
    if (instances.empty() || selectionMaskPassOpen_ ||
        (!shadowInstancing && !worldInstancing)) {
        return false;
    }

    for (auto& variantTransforms : resourceRockTransforms_) {
        variantTransforms.clear();
        variantTransforms.reserve(
            instances.size() / StoneVisualVariantCount + 1U);
    }
    for (const RockDrawInstance& instance : instances) {
        const std::size_t variant =
            instance.visualVariant % StoneVisualVariantCount;
        ModelResource& resource = resources_.rockModel(variant);
        if (!resource.valid()) {
            return false;
        }
        const float scale = instance.scale * worldRevealScaleAt(
            {instance.position.x, instance.position.z});
        if (scale <= 0.001F) {
            continue;
        }
        const float modelScale = RockModelScale * scale;
        Vector3 position = instance.position;
        position.y += rockGroundOffset(variant) * scale;
        const Matrix rotation = MatrixRotateY(
            instance.yawRadians);
        resourceRockTransforms_[variant].push_back(MatrixMultiply(
            resource.get().transform,
            MatrixMultiply(
                MatrixScale(
                    modelScale, modelScale, modelScale),
                MatrixMultiply(
                    rotation,
                    MatrixTranslate(
                        position.x, position.y, position.z)))));
    }
    const bool anyTransforms = std::ranges::any_of(
        resourceRockTransforms_,
        [](const auto& transforms) { return !transforms.empty(); });
    if (!anyTransforms) {
        return true;
    }

    Shader& shader = shadowInstancing
        ? resources_.shadowShader().get()
        : resources_.worldShader().get();
    const int instancingLocation = shadowInstancing
        ? shadowInstancingEnabledLocation_
        : worldInstancingEnabledLocation_;
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, instancingLocation, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (std::size_t variant = 0;
         variant < StoneVisualVariantCount; ++variant) {
        if (resourceRockTransforms_[variant].empty()) continue;
        ModelResource& resource = resources_.rockModel(variant);
        Model& model = resource.get();
        static_cast<void>(instanceBuffers_.drawModel(
            model, shader, resourceRockTransforms_[variant]));
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, instancingLocation, &disabled,
        SHADER_UNIFORM_INT);
    return true;
}

bool Renderer::drawTree(Vector3 position, Color tint,
                        float scale, std::size_t visualVariant,
                        float yawRadians) {
    const std::size_t variant =
        visualVariant % TreeVisualVariantCount;
    auto& resource = resources_.treeModel(variant);
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return false;
    }
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr && model.materials != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    scale *= worldRevealScaleAt(
        {position.x, position.z});
    if (scale <= 0.001F) {
        return true;
    }
    // Resource trees remain upright. Their ground offset is vertical only;
    // terrain normals are intentionally not used for resource spawning.
    position.y += static_cast<float>(
        TreeVisualGroundOffsets[
            variant] * scale * TreeVisualHeightScale);
    const bool conifer = variant >= 3U && variant <= 5U;
    if (worldShaderActive_ && !shadowPassOpen_ &&
        !selectionMaskPassOpen_) {
        WorldMaterialState treeMaterial = worldMaterial_;
        treeMaterial.coniferAmount = conifer ? 1.0F : 0.0F;
        uploadWorldMaterial(treeMaterial);
    }
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {TreeModelScale * scale,
         TreeModelScale * scale * TreeVisualHeightScale,
         TreeModelScale * scale},
        tint);
    if (worldShaderActive_ && !shadowPassOpen_ &&
        !selectionMaskPassOpen_) {
        uploadWorldMaterial(worldMaterial_);
    }
    return true;
}

BoundingBox Renderer::treeWorldBounds(
    Vector3 position, float scale, std::size_t visualVariant,
    float yawRadians) {
    const std::size_t variant = visualVariant % TreeVisualVariantCount;
    ModelResource& resource = resources_.treeModel(variant);
    if (!resource.valid() || !world_transforms::finite(position) ||
        !std::isfinite(scale) || !std::isfinite(yawRadians)) {
        return {};
    }
    scale *= worldRevealScaleAt({position.x, position.z});
    if (!std::isfinite(scale) || scale <= 0.001F) return {};
    position.y += static_cast<float>(
        TreeVisualGroundOffsets[variant] * scale *
        TreeVisualHeightScale);
    const float modelScale = TreeModelScale * scale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(
                    modelScale,
                    modelScale * TreeVisualHeightScale,
                    modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    BoundingBox bounds = world_transforms::transformBounds(
        resource.visualBounds(), transform);
    // Selection mask applies up to 1.35 times the authored wind vector.
    // Keep scissor bounds around the displaced crown at every wind phase.
    constexpr float MaximumLocalWindDisplacement =
        1.35F * 0.10F;
    const float windPadding =
        MaximumLocalWindDisplacement * modelScale;
    bounds.min.x -= windPadding;
    bounds.max.x += windPadding;
    bounds.min.z -= windPadding;
    bounds.max.z += windPadding;
    return bounds;
}

bool Renderer::drawTreesInstanced(
    std::span<const TreeDrawInstance> instances) {
    const bool shadowInstancing = shadowPassOpen_ &&
        resources_.shadowShader().valid();
    const bool worldInstancing = worldShaderActive_ &&
        resources_.worldShader().valid();
    if (instances.empty() || selectionMaskPassOpen_ ||
        (!shadowInstancing && !worldInstancing)) {
        return false;
    }

    for (auto& variantTransforms : resourceTreeTransforms_) {
        variantTransforms.clear();
        variantTransforms.reserve(
            instances.size() / TreeVisualVariantCount + 1U);
    }
    for (const TreeDrawInstance& instance : instances) {
        const std::size_t variant =
            instance.visualVariant % TreeVisualVariantCount;
        ModelResource& resource = resources_.treeModel(variant);
        if (!resource.valid()) {
            return false;
        }
        const float revealScale = worldRevealScaleAt(
            {instance.position.x, instance.position.z});
        const float scale = instance.scale * revealScale;
        if (scale <= 0.001F) {
            continue;
        }
        Vector3 position = instance.position;
        position.y += static_cast<float>(
            TreeVisualGroundOffsets[variant] * scale *
            TreeVisualHeightScale);
        const float modelScale = TreeModelScale * scale;
        const Matrix rotation = MatrixRotateY(
            instance.yawRadians);
        resourceTreeTransforms_[variant].push_back(MatrixMultiply(
            resource.get().transform,
            MatrixMultiply(
                MatrixMultiply(
                    MatrixScale(
                        modelScale,
                        modelScale * TreeVisualHeightScale,
                        modelScale),
                    rotation),
                MatrixTranslate(
                    position.x, position.y, position.z))));
    }

    Shader& shader = shadowInstancing
        ? resources_.shadowShader().get()
        : resources_.worldShader().get();
    const int instancingLocation = shadowInstancing
        ? shadowInstancingEnabledLocation_
        : worldInstancingEnabledLocation_;
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, instancingLocation, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (std::size_t variant = 0;
         variant < TreeVisualVariantCount; ++variant) {
        if (resourceTreeTransforms_[variant].empty()) {
            continue;
        }
        if (worldInstancing) {
            WorldMaterialState treeMaterial = worldMaterial_;
            treeMaterial.coniferAmount =
                variant >= 3U && variant <= 5U ? 1.0F : 0.0F;
            uploadWorldMaterial(treeMaterial);
        }
        Model& model = resources_.treeModel(variant).get();
        static_cast<void>(instanceBuffers_.drawModel(
            model, shader, resourceTreeTransforms_[variant]));
    }
    if (worldInstancing) {
        uploadWorldMaterial(worldMaterial_);
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, instancingLocation, &disabled,
        SHADER_UNIFORM_INT);
    return true;
}

} // namespace ian
