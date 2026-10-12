#include "graphics/Renderer.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"
#include "graphics/EnemyPresentation.hpp"
#include "graphics/EliteEnemyDetails.hpp"


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

constexpr std::array<const char*, 23> GeneralAnimationBones{
    "root", "hips", "upperleg.l", "lowerleg.l", "foot.l",
    "toes.l", "spine", "chest", "head", "upperarm.l",
    "lowerarm.l", "wrist.l", "hand.l", "handslot.l",
    "upperarm.r", "lowerarm.r", "wrist.r", "hand.r",
    "handslot.r", "upperleg.r", "lowerleg.r", "foot.r",
    "toes.r",
};

constexpr std::array<const char*, 23> MovementAnimationBones{
    "root", "hips", "upperleg.r", "lowerleg.r", "foot.r",
    "toes.r", "spine", "chest", "head", "upperarm.r",
    "lowerarm.r", "wrist.r", "hand.r", "handslot.r",
    "upperarm.l", "lowerarm.l", "wrist.l", "hand.l",
    "handslot.l", "upperleg.l", "lowerleg.l", "foot.l",
    "toes.l",
};

struct EnemyAnimationSource {
    const ModelAnimationsResource* animations{};
    const std::array<const char*, 23>* sourceBones{};
    const char* clipName{};
    bool nativeSkeleton{};
};

// Authoring/import scale is shared by regular enemy rendering, instancing,
// collision bounds and target UI. Gameplay archetype scale is supplied by
// AppRenderSupport::enemyVisualScale.
constexpr float EnemyImportScale = 0.82F;
ModelResource* enemyModelFor(
    GraphicsResources& resources, EnemyModelVisual visual) {
    switch (visual) {
    case EnemyModelVisual::Minion:
        return &resources.enemyMinionModel();
    case EnemyModelVisual::Rogue:
        return &resources.enemyRogueModel();
    case EnemyModelVisual::Warrior:
        return &resources.enemyWarriorModel();
    case EnemyModelVisual::Mage:
        return &resources.enemyMageModel();
    case EnemyModelVisual::Sapper:
        return &resources.enemySapperModel();
    case EnemyModelVisual::Flying:
        return &resources.enemyFlyingModel();
    case EnemyModelVisual::Boss:
        return &resources.enemyBossModel();
    case EnemyModelVisual::Splitter:
        return &resources.enemySplitterModel();
    case EnemyModelVisual::Splitling:
        return &resources.enemySplitlingModel();
    }
    return nullptr;
}

const ModelResource* enemyModelFor(
    const GraphicsResources& resources, EnemyModelVisual visual) {
    switch (visual) {
    case EnemyModelVisual::Minion:
        return &resources.enemyMinionModel();
    case EnemyModelVisual::Rogue:
        return &resources.enemyRogueModel();
    case EnemyModelVisual::Warrior:
        return &resources.enemyWarriorModel();
    case EnemyModelVisual::Mage:
        return &resources.enemyMageModel();
    case EnemyModelVisual::Sapper:
        return &resources.enemySapperModel();
    case EnemyModelVisual::Flying:
        return &resources.enemyFlyingModel();
    case EnemyModelVisual::Boss:
        return &resources.enemyBossModel();
    case EnemyModelVisual::Splitter:
        return &resources.enemySplitterModel();
    case EnemyModelVisual::Splitling:
        return &resources.enemySplitlingModel();
    }
    return nullptr;
}

EnemyAnimationSource enemyAnimationFor(
    const GraphicsResources& resources,
    EnemyModelVisual modelVisual,
    EnemyAnimationVisual animationVisual) {
    const ModelAnimationsResource* animations =
        &resources.enemyGeneralAnimations();
    const std::array<const char*, 23>* bones =
        &GeneralAnimationBones;
    const char* clip = "Idle_A";
    bool native = false;

    if (modelVisual == EnemyModelVisual::Minion ||
        modelVisual == EnemyModelVisual::Rogue ||
        modelVisual == EnemyModelVisual::Mage ||
        modelVisual == EnemyModelVisual::Splitter ||
        modelVisual == EnemyModelVisual::Splitling) {
        if (modelVisual == EnemyModelVisual::Rogue) {
            animations = &resources.enemyNinjaAnimations();
        } else if (modelVisual == EnemyModelVisual::Mage) {
            animations = &resources.enemyWizardAnimations();
        } else if (modelVisual == EnemyModelVisual::Splitter) {
            animations = &resources.enemySplitterAnimations();
        } else if (modelVisual == EnemyModelVisual::Splitling) {
            animations = &resources.enemySplitlingAnimations();
        } else {
            animations = &resources.enemyPinkBlobAnimations();
        }
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
        case EnemyAnimationVisual::Run:
            clip = "Walk";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::RangedAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Bite_Front";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitRecieve";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Idle";
            break;
        }
    } else if (modelVisual == EnemyModelVisual::Warrior) {
        animations = &resources.enemyMushroomKingAnimations();
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
        case EnemyAnimationVisual::Run:
            clip = "Walk";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::RangedAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Bite_Front";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitRecieve";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Idle";
            break;
        }
    } else if (modelVisual == EnemyModelVisual::Sapper) {
        animations = &resources.enemySapperAnimations();
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
        case EnemyAnimationVisual::Run:
            clip = "Walk";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::RangedAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Bite_Front";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitRecieve";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Idle";
            break;
        }
    } else if (modelVisual == EnemyModelVisual::Flying) {
        animations = &resources.enemyFlyingAnimations();
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
        case EnemyAnimationVisual::Run:
            clip = "Fast_Flying";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::RangedAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Headbutt";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitReact";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Flying_Idle";
            break;
        }
    } else if (modelVisual == EnemyModelVisual::Boss) {
        animations = &resources.enemyBossAnimations();
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
            clip = "Walk";
            break;
        case EnemyAnimationVisual::Run:
            clip = "Walk";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::RangedAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Bite_Front";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitRecieve";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Idle";
            break;
        }
    } else {
        switch (animationVisual) {
        case EnemyAnimationVisual::Idle:
            break;
        case EnemyAnimationVisual::Walk:
            animations = &resources.enemyMovementAnimations();
            bones = &MovementAnimationBones;
            clip = "Walking_A";
            break;
        case EnemyAnimationVisual::Run:
            animations = &resources.enemyMovementAnimations();
            bones = &MovementAnimationBones;
            clip = "Running_A";
            break;
        case EnemyAnimationVisual::MeleeAttack:
            clip = "Use_Item";
            break;
        case EnemyAnimationVisual::RangedAttack:
            clip = "Throw";
            break;
        case EnemyAnimationVisual::SapperAttack:
            clip = "Interact";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "Hit_A";
            break;
        case EnemyAnimationVisual::Death:
            clip = "Death_A";
            break;
        case EnemyAnimationVisual::Spawn:
            clip = "Spawn_Ground";
            break;
        }
    }
    return {animations, bones, clip, native};
}

bool finiteBoneMatrices(const Matrix* matrices, int boneCount) {
    if (matrices == nullptr || boneCount <= 0 ||
        boneCount > MaximumGpuSkinningBones) {
        return false;
    }
    for (int bone = 0; bone < boneCount; ++bone) {
        const float* values = &matrices[bone].m0;
        for (int value = 0; value < 16; ++value) {
            if (!std::isfinite(values[value])) {
                return false;
            }
        }
    }
    return true;
}

bool uploadBoneMatrices(
    Shader& shader, const Matrix* matrices, int boneCount) {
    if (!finiteBoneMatrices(matrices, boneCount) ||
        shader.locs == nullptr ||
        shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] < 0) {
        return false;
    }
    rlDrawRenderBatchActive();
    rlEnableShader(shader.id);
    rlSetUniformMatrices(
        shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
        matrices, boneCount);
    rlDisableShader();
    return true;
}

bool uploadBoneMatrices(
    ModelResource& resource, Shader& shader) {
    if (!resource.gpuSkinningCompatible() ||
        !resource.runtimeBoneMatricesFinite()) {
        return false;
    }
    const Model& model = resource.get();
    return uploadBoneMatrices(
        shader, model.boneMatrices,
        model.skeleton.boneCount);
}

Color crowdLodTint(EnemyModelVisual visual, Color tint) {
    if (tint.r != 255 || tint.g != 255 || tint.b != 255) {
        return tint;
    }
    switch (visual) {
    case EnemyModelVisual::Minion:
        return {235, 112, 185, tint.a};
    case EnemyModelVisual::Rogue:
        return {191, 104, 52, tint.a};
    case EnemyModelVisual::Warrior:
        return {93, 60, 105, tint.a};
    case EnemyModelVisual::Mage:
        return {129, 77, 193, tint.a};
    case EnemyModelVisual::Sapper:
        return {170, 118, 43, tint.a};
    case EnemyModelVisual::Flying:
        return {102, 71, 167, tint.a};
    case EnemyModelVisual::Boss:
        return {74, 35, 45, tint.a};
    case EnemyModelVisual::Splitter:
        return {67, 154, 73, tint.a};
    case EnemyModelVisual::Splitling:
        return {82, 190, 91, tint.a};
    }
    return tint;
}

} // namespace

const std::vector<int>& Renderer::enemyBoneMapping(
    EnemyModelVisual visual, const Model& model,
    const std::array<const char*, 23>& sourceBones) {
    const std::size_t modelIndex =
        static_cast<std::size_t>(visual);
    const std::size_t sourceIndex =
        &sourceBones == &MovementAnimationBones ? 1U : 0U;
    std::vector<int>& mapping =
        enemyBoneMappings_[modelIndex][sourceIndex];
    const std::size_t boneCount = static_cast<std::size_t>(
        model.skeleton.boneCount);
    if (mapping.size() == boneCount) {
        return mapping;
    }

    mapping.assign(boneCount, -1);
    for (std::size_t modelBone = 0;
         modelBone < boneCount; ++modelBone) {
        for (std::size_t sourceBone = 0;
             sourceBone < sourceBones.size(); ++sourceBone) {
            if (std::strcmp(
                    model.skeleton.bones[modelBone].name,
                    sourceBones[sourceBone]) == 0) {
                mapping[modelBone] =
                    static_cast<int>(sourceBone);
                break;
            }
        }
    }
    return mapping;
}

bool Renderer::drawEnemy(
    EnemyModelVisual modelVisual,
    EnemyAnimationVisual animationVisual,
    float animationSeconds, Vector3 position,
    float yawRadians, Color tint, float scale, bool loop,
    bool inkOutlineEligible, bool quantizeCrowdPose,
    std::uint8_t eliteAffixes, Vector3 bodyStretch, float bodyRoll) {
    ModelResource* modelResource =
        enemyModelFor(resources_, modelVisual);
    if (modelResource == nullptr || !modelResource->valid()) {
        return false;
    }

    const EnemyAnimationSource animation =
        enemyAnimationFor(resources_, modelVisual, animationVisual);

    Model& model = modelResource->get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return false;
    }
    const ModelAnimation* clip =
        animation.animations->find(animation.clipName);
    const bool skeletonUsable =
        model.skeleton.boneCount > 0 &&
        model.skeleton.bones != nullptr &&
        model.skeleton.bindPose != nullptr;
    float motionTime = animationSeconds;
    if (clip != nullptr && clip->keyframeCount > 0 &&
        skeletonUsable) {
        const float frameValue =
            std::max(0.0F, animationSeconds) * 30.0F;
        int frame = static_cast<int>(frameValue);
        frame = loop
            ? frame % clip->keyframeCount
            : std::min(frame, clip->keyframeCount - 1);
        if (quantizeCrowdPose) {
            constexpr int CrowdPoseCount = 6;
            const int pose = std::min(
                CrowdPoseCount - 1,
                frame * CrowdPoseCount / clip->keyframeCount);
            frame = pose * clip->keyframeCount /
                CrowdPoseCount;
        }
        if (quantizeCrowdPose) motionTime = static_cast<float>(frame) / 30.0F;
        if (animation.nativeSkeleton) {
            UpdateModelAnimation(
                model, *clip,
                static_cast<float>(frame));
        } else {
            const std::vector<int>& boneMapping =
                enemyBoneMapping(
                    modelVisual, model,
                    *animation.sourceBones);
            enemyAnimationPose_.resize(
                static_cast<std::size_t>(model.skeleton.boneCount));
            for (int modelBone = 0;
                 modelBone < model.skeleton.boneCount; ++modelBone) {
                const int sourceBone = boneMapping[
                    static_cast<std::size_t>(modelBone)];
                enemyAnimationPose_[
                    static_cast<std::size_t>(modelBone)] =
                    sourceBone >= 0 && sourceBone < clip->boneCount
                        ? clip->keyframePoses[frame][sourceBone]
                        : model.skeleton.bindPose[modelBone];
            }
            enemyAnimationFrames_.assign(
                1U, enemyAnimationPose_.data());
            ModelAnimation remapped{};
            remapped.boneCount = model.skeleton.boneCount;
            remapped.keyframeCount = 1;
            remapped.keyframePoses =
                enemyAnimationFrames_.data();
            UpdateModelAnimation(model, remapped, 0);
        }
    }

    Shader* shader = prepareModelShader(model);

    const WorldMaterialState originalMaterial = worldMaterial_;
    if (worldShaderActive_) {
        WorldMaterialState material = worldMaterial_;
        material.inkOutlineEligible = inkOutlineEligible;
        material.enemyPresentationAmount = 1.0F;
        setWorldMaterial(material);
    }
    const bool skinningEnabled =
        modelResource->gpuSkinningCompatible() &&
        modelResource->runtimeBoneMatricesFinite();
    const Matrix transform = MatrixMultiply(model.transform,
        enemyPresentationTransform(modelVisual, animationVisual, motionTime,
            position, yawRadians, EnemyImportScale * scale, bodyStretch, bodyRoll));
    bool bonesUploaded = false;
    for (int meshIndex = 0; meshIndex < model.meshCount;
         ++meshIndex) {
        if (!modelResource->meshValid(
                static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        const bool meshSkinningEnabled = skinningEnabled &&
            modelResource->meshHasSkinning(
                static_cast<std::size_t>(meshIndex));
        if (shader != nullptr) {
            setSkinningEnabled(*shader, meshSkinningEnabled);
            if (meshSkinningEnabled && !bonesUploaded) {
                bonesUploaded = uploadBoneMatrices(
                    *modelResource, *shader);
            }
        }
        Material material = model.materials[materialIndex];
        if (shader != nullptr) {
            material.shader = *shader;
        }
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material, transform);
    }
    if (shader != nullptr) {
        setSkinningEnabled(*shader, false);
    }
    drawEliteEnemyDetails({.modelVisual = modelVisual, .animationVisual = animationVisual,
        .animationSeconds = motionTime, .position = position, .yawRadians = yawRadians,
        .tint = tint, .whiteFlashAmount = worldMaterial_.enemyWhiteFlashAmount, .scale = scale,
        .loop = loop, .inkOutlineEligible = inkOutlineEligible, .eliteAffixes = eliteAffixes,
        .bodyStretch = bodyStretch, .bodyRoll = bodyRoll});
    if (worldShaderActive_) setWorldMaterial(originalMaterial);
    return true;
}

void Renderer::drawEliteEnemyDetails(const EnemyDrawInstance& instance) {
    if (!instance.eliteAffixes) return;
    ModelResource* resource = enemyModelFor(resources_, instance.modelVisual);
    if (!resource || !resource->valid()) return;
    const Model& model = resource->get();
    Matrix head = MatrixIdentity();
    if (model.skeleton.bones && model.boneMatrices && resource->runtimeBoneMatricesFinite()) {
        for (int bone = 0; bone < model.skeleton.boneCount; ++bone) {
            if (std::strcmp(model.skeleton.bones[bone].name, "head") == 0 ||
                std::strcmp(model.skeleton.bones[bone].name, "Head") == 0) {
                head = model.boneMatrices[bone];
                break;
            }
        }
    }
    const Matrix world = MatrixMultiply(model.transform,
        enemyPresentationTransform(instance.modelVisual, instance.animationVisual, instance.animationSeconds,
            instance.position, instance.yawRadians, EnemyImportScale * instance.scale,
            instance.bodyStretch, instance.bodyRoll));
    const WorldMaterialState original = worldMaterial_;
    if (worldShaderActive_) {
        WorldMaterialState material = original;
        material.enemyPresentationAmount = 1;
        material.enemyWhiteFlashAmount = instance.whiteFlashAmount;
        material.inkOutlineEligible = instance.inkOutlineEligible;
        setWorldMaterial(material);
    }
    elite_details::draw(resource->visualBounds(), MatrixMultiply(head, world),
        instance.eliteAffixes, instance.tint.a);
    if (worldShaderActive_) setWorldMaterial(original);
}

BoundingBox Renderer::enemyWorldBounds(
    EnemyModelVisual modelVisual, Vector3 position,
    float yawRadians, float scale, bool eliteDetails) const {
    const ModelResource* resource =
        enemyModelFor(resources_, modelVisual);
    if (resource == nullptr || !resource->valid() ||
        !world_transforms::finite(position) ||
        !std::isfinite(yawRadians) || !std::isfinite(scale) ||
        scale <= 0.0F) {
        return {};
    }
    const float modelScale = EnemyImportScale * scale;
    const Matrix transform = MatrixMultiply(
        resource->get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    BoundingBox bounds = resource->visualBounds();
    if (eliteDetails) {
        const float radius = std::max(.15F, std::min(bounds.max.x - bounds.min.x,
            bounds.max.z - bounds.min.z) * .34F);
        const float centerX = (bounds.min.x + bounds.max.x) * .5F;
        const float centerZ = (bounds.min.z + bounds.max.z) * .5F;
        bounds.max.y += radius * 1.3F;
        bounds.min.x = std::min(bounds.min.x, centerX - radius * 1.8F);
        bounds.max.x = std::max(bounds.max.x, centerX + radius * 1.8F);
        bounds.min.z = std::min(bounds.min.z, centerZ - radius * 1.5F);
        bounds.max.z = std::max(bounds.max.z, centerZ + radius * 1.5F);
    }
    return world_transforms::transformBounds(bounds, transform);
}

bool Renderer::drawEnemiesInstanced(
    std::span<const EnemyDrawInstance> instances) {
    if (instances.empty() || shadowPassOpen_ ||
        selectionMaskPassOpen_ || !worldShaderActive_ ||
        !resources_.worldShader().valid()) {
        return false;
    }
    const auto drawStart = PerformanceClock::now();
    ++enemyBatchFrame_;
    if (enemyBatchFrame_ == 0U) {
        enemyBatchFrame_ = 1U;
        enemyBatches_.clear();
    }

    const auto modelFor = [this](EnemyModelVisual visual)
        -> ModelResource* {
        return enemyModelFor(resources_, visual);
    };
    const auto animationFor =
        [this](EnemyModelVisual model,
               EnemyAnimationVisual animation) {
        return enemyAnimationFor(resources_, model, animation);
    };

    constexpr int CrowdPoseCount = 6;
    constexpr float SourceAnimationFps = 30.0F;
    const bool lodAvailable = IsModelValid(enemyCrowdLodModel_);
    for (EnemyBatch* batch : activeEnemyBatches_) {
        batch->transforms.clear();
    }
    activeEnemyBatches_.clear();
    for (const EnemyDrawInstance& instance : instances) {
        // Elites are rare; draw the full model once with its exact head pose
        // instead of putting animated accessories into ordinary crowd batches.
        if (instance.eliteAffixes) continue;
        const bool lowDetail = instance.lowDetail && lodAvailable;
        ModelResource* resource = lowDetail
            ? nullptr
            : modelFor(instance.modelVisual);
        if (!lowDetail &&
            (resource == nullptr || !resource->valid())) {
            return false;
        }
        if (!lowDetail) {
            const Model& model = resource->get();
            if (model.meshCount <= 0 || model.meshes == nullptr ||
                model.meshMaterial == nullptr ||
                model.materials == nullptr) {
                return false;
            }
        }
        EnemyAnimationSource animation{};
        const ModelAnimation* clip = nullptr;
        if (!lowDetail) {
            animation = animationFor(
                instance.modelVisual,
                instance.animationVisual);
            clip = animation.animations->find(
                animation.clipName);
        }
        int frame = 0;
        if (!lowDetail && clip != nullptr &&
            clip->keyframeCount > 0) {
            const int sourceFrame = static_cast<int>(
                std::max(0.0F, instance.animationSeconds) *
                SourceAnimationFps);
            const int wrappedFrame =
                instance.loop
                    ? sourceFrame % clip->keyframeCount
                    : std::min(
                          sourceFrame,
                          clip->keyframeCount - 1);
            const int pose =
                std::min(
                    CrowdPoseCount - 1,
                    wrappedFrame * CrowdPoseCount /
                        clip->keyframeCount);
            frame =
                pose * clip->keyframeCount /
                CrowdPoseCount;
        }
        const std::uint32_t tint =
            static_cast<std::uint32_t>(instance.tint.r) |
            (static_cast<std::uint32_t>(instance.tint.g) << 8U) |
            (static_cast<std::uint32_t>(instance.tint.b) << 16U) |
            (static_cast<std::uint32_t>(instance.tint.a) << 24U);
        const EnemyAnimationVisual batchAnimation = lowDetail
            ? EnemyAnimationVisual::Idle
            : instance.animationVisual;
        const int batchFrame = lowDetail ? 0 : frame;
        const bool batchLoop = lowDetail ? true : instance.loop;
        const std::uint8_t whiteFlashLevel =
            static_cast<std::uint8_t>(std::clamp(
                std::lround(instance.whiteFlashAmount * 8.0F),
                0L, 8L));
        const EnemyBatchKey key{
            instance.modelVisual, batchAnimation, batchFrame,
            tint, whiteFlashLevel, batchLoop, lowDetail,
            instance.inkOutlineEligible};
        EnemyBatch& batch = enemyBatches_[key];
        batch.lastUsedFrame = enemyBatchFrame_;
        if (batch.transforms.empty()) {
            batch.representative = instance;
            batch.representative.animationSeconds =
                static_cast<float>(frame) /
                SourceAnimationFps;
            activeEnemyBatches_.push_back(&batch);
        }
        const float uniformScale = EnemyImportScale * instance.scale;
        Matrix transform{};
        if (lowDetail) {
            Vector3 proxyPosition = instance.position;
            proxyPosition.y += uniformScale * instance.bodyStretch.y;
            transform = MatrixMultiply(
                MatrixScale(
                    uniformScale * 0.9F * instance.bodyStretch.x,
                    uniformScale * 2.0F * instance.bodyStretch.y,
                    uniformScale * 0.9F * instance.bodyStretch.z),
                MatrixMultiply(
                    MatrixMultiply(MatrixRotateZ(instance.bodyRoll), MatrixRotateY(instance.yawRadians)),
                    MatrixTranslate(
                        proxyPosition.x, proxyPosition.y,
                        proxyPosition.z)));
        } else {
            const float motionTime = static_cast<float>(frame) / SourceAnimationFps;
            transform = MatrixMultiply(resource->get().transform,
                enemyPresentationTransform(instance.modelVisual, instance.animationVisual,
                    motionTime, instance.position, instance.yawRadians, uniformScale,
                    instance.bodyStretch, instance.bodyRoll));
        }
        batch.transforms.push_back(transform);
    }

    Shader& shader = resources_.worldShader().get();
    int instancingValue = 1;
    bool instancingEnabled = true;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &instancingValue,
        SHADER_UNIFORM_INT);
    const auto setInstancing = [this, &shader, &instancingEnabled,
                                &instancingValue](bool enabled) {
        if (enabled == instancingEnabled) {
            return;
        }
        rlDrawRenderBatchActive();
        instancingValue = enabled ? 1 : 0;
        SetShaderValue(
            shader, worldInstancingEnabledLocation_, &instancingValue,
            SHADER_UNIFORM_INT);
        instancingEnabled = enabled;
    };

    std::size_t nonEmptyBatchCount = 0U;
    const WorldMaterialState originalMaterial = worldMaterial_;
    for (int detailPass = 0; detailPass < 2; ++detailPass) {
        const bool lowDetailPass = detailPass == 0;
        for (EnemyBatch* batchPointer : activeEnemyBatches_) {
            EnemyBatch& batch = *batchPointer;
            if (batch.representative.lowDetail != lowDetailPass) {
                continue;
            }
            ++nonEmptyBatchCount;
            const EnemyDrawInstance& representative =
                batch.representative;
            const bool multipleInstances =
                batch.transforms.size() > 1U;
            const InstanceBatch instanceBatch = multipleInstances
                ? instanceBuffers_.upload(batch.transforms)
                : InstanceBatch{};
            WorldMaterialState batchMaterial = worldMaterial_;
            batchMaterial.enemyPresentationAmount = 1.0F;
            batchMaterial.inkOutlineEligible =
                representative.inkOutlineEligible;
            batchMaterial.enemyWhiteFlashAmount =
                representative.whiteFlashAmount;
            setWorldMaterial(batchMaterial);
            if (lowDetailPass) {
                setSkinningEnabled(shader, false);
                Material material = enemyCrowdLodModel_.materials[0];
                material.shader = shader;
                const Color original =
                    material.maps[MATERIAL_MAP_DIFFUSE].color;
                material.maps[MATERIAL_MAP_DIFFUSE].color =
                    crowdLodTint(
                        representative.modelVisual,
                        representative.tint);
                setInstancing(multipleInstances);
                if (multipleInstances) {
                    instanceBuffers_.drawMesh(
                        enemyCrowdLodModel_.meshes[0], material,
                        instanceBatch);
                } else {
                    DrawMesh(
                        enemyCrowdLodModel_.meshes[0], material,
                        batch.transforms.front());
                }
                material.maps[MATERIAL_MAP_DIFFUSE].color = original;
                continue;
            }
        ModelResource* resource =
            modelFor(representative.modelVisual);
        Model& model = resource->get();
        const bool modelSkinningEnabled =
            resource->gpuSkinningCompatible() &&
            resource->runtimeBoneMatricesFinite();
        const EnemyAnimationSource animation =
            animationFor(representative.modelVisual,
                         representative.animationVisual);
        const ModelAnimation* clip =
            animation.animations->find(animation.clipName);
        auto poseIt = enemyBonePoseCache_.end();
        if (clip != nullptr && clip->keyframeCount > 0 &&
            model.skeleton.boneCount > 0 &&
            model.skeleton.bones != nullptr &&
            model.skeleton.bindPose != nullptr) {
            const int frame = std::min(
                static_cast<int>(
                    representative.animationSeconds * 30.0F),
                clip->keyframeCount - 1);
            const EnemyPoseKey poseKey{
                representative.modelVisual,
                representative.animationVisual,
                frame};
            poseIt = enemyBonePoseCache_.find(poseKey);
            if (poseIt == enemyBonePoseCache_.end()) {
                if (animation.nativeSkeleton) {
                    UpdateModelAnimation(
                        model, *clip,
                        static_cast<float>(frame));
                } else {
                    const std::vector<int>& boneMapping =
                        enemyBoneMapping(
                            representative.modelVisual, model,
                            *animation.sourceBones);
                    enemyAnimationPose_.resize(
                        static_cast<std::size_t>(
                            model.skeleton.boneCount));
                    for (int modelBone = 0;
                         modelBone < model.skeleton.boneCount;
                         ++modelBone) {
                        const int sourceBone = boneMapping[
                            static_cast<std::size_t>(modelBone)];
                        enemyAnimationPose_[
                            static_cast<std::size_t>(modelBone)] =
                            sourceBone >= 0 &&
                                    sourceBone < clip->boneCount
                                ? clip->keyframePoses[frame][sourceBone]
                                : model.skeleton.bindPose[modelBone];
                    }
                    enemyAnimationFrames_.assign(
                        1U, enemyAnimationPose_.data());
                    ModelAnimation remapped{};
                    remapped.boneCount = model.skeleton.boneCount;
                    remapped.keyframeCount = 1;
                    remapped.keyframePoses =
                        enemyAnimationFrames_.data();
                    UpdateModelAnimation(model, remapped, 0);
                }
                if (model.boneMatrices != nullptr) {
                    std::vector<Matrix> cachedPose(
                        static_cast<std::size_t>(
                            model.skeleton.boneCount));
                    std::copy_n(
                        model.boneMatrices,
                        model.skeleton.boneCount,
                        cachedPose.begin());
                    poseIt = enemyBonePoseCache_.emplace(
                        poseKey, std::move(cachedPose)).first;
                }
            }
        }

        const Matrix* batchBoneMatrices = model.boneMatrices;
        if (poseIt != enemyBonePoseCache_.end()) {
            batchBoneMatrices = poseIt->second.data();
        }
        const bool batchSkinningEnabled = modelSkinningEnabled &&
            finiteBoneMatrices(
                batchBoneMatrices, model.skeleton.boneCount);
        bool bonesUploaded = false;
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            if (!resource->meshValid(
                    static_cast<std::size_t>(meshIndex))) {
                continue;
            }
            const int materialIndex =
                model.meshMaterial[meshIndex];
            if (materialIndex < 0 || materialIndex >= model.materialCount) {
                continue;
            }
            const bool meshSkinningEnabled = batchSkinningEnabled &&
                resource->meshHasSkinning(
                    static_cast<std::size_t>(meshIndex));
            setSkinningEnabled(shader, meshSkinningEnabled);
            if (meshSkinningEnabled && !bonesUploaded) {
                bonesUploaded = uploadBoneMatrices(
                    shader, batchBoneMatrices,
                    model.skeleton.boneCount);
            }
            Material material = model.materials[materialIndex];
            material.shader = shader;
            const Color original =
                material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                representative.tint;
            setInstancing(multipleInstances);
            if (multipleInstances) {
                instanceBuffers_.drawMesh(
                    model.meshes[meshIndex], material,
                    instanceBatch);
            } else {
                DrawMesh(
                    model.meshes[meshIndex], material,
                    batch.transforms.front());
            }
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        }
        }
    }

    setInstancing(false);
    setSkinningEnabled(shader, false);
    setWorldMaterial(originalMaterial);
    for (const auto& instance : instances) {
        if (!instance.eliteAffixes) continue;
        WorldMaterialState material = originalMaterial;
        material.enemyWhiteFlashAmount = instance.whiteFlashAmount;
        setWorldMaterial(material);
        static_cast<void>(drawEnemy(instance.modelVisual, instance.animationVisual,
            instance.animationSeconds, instance.position, instance.yawRadians, instance.tint,
            instance.scale, instance.loop, instance.inkOutlineEligible,
            false, instance.eliteAffixes, instance.bodyStretch, instance.bodyRoll));
    }
    setWorldMaterial(originalMaterial);
    instancedEnemyMillisecondsThisFrame_ +=
        performanceMilliseconds(drawStart);
    const std::size_t lowDetailCount = static_cast<std::size_t>(
        std::count_if(
            instances.begin(), instances.end(),
            [](const EnemyDrawInstance& instance) {
                return instance.lowDetail;
            }));
    performanceStats_.instancedEnemyCount = std::max(
        performanceStats_.instancedEnemyCount, instances.size());
    performanceStats_.enemyBatchCount = std::max(
        performanceStats_.enemyBatchCount, nonEmptyBatchCount);
    performanceStats_.lowDetailEnemyCount = std::max(
        performanceStats_.lowDetailEnemyCount, lowDetailCount);
    constexpr std::size_t MaximumCachedEnemyBatches = 256U;
    constexpr std::uint64_t EnemyBatchRetentionFrames = 120U;
    if (enemyBatches_.size() > MaximumCachedEnemyBatches &&
        enemyBatchFrame_ % EnemyBatchRetentionFrames == 0U) {
        std::erase_if(
            enemyBatches_,
            [this](const auto& entry) {
                return enemyBatchFrame_ -
                        entry.second.lastUsedFrame >
                    EnemyBatchRetentionFrames;
            });
    }
    return true;
}

} // namespace ian
