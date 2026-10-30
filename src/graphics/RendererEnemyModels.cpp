#include "graphics/Renderer.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"
#include "graphics/EnemyAnimationSampling.hpp"
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
    case EnemyModelVisual::AirTank: return &resources.enemyAirTankModel();
    case EnemyModelVisual::Bee: return &resources.enemyBeeModel();
    case EnemyModelVisual::BeeElite: return &resources.enemyBeeEliteModel();
    case EnemyModelVisual::Flying:
        return &resources.enemyFlyingModel();
    case EnemyModelVisual::BossSummon:
        return &resources.enemyBossSummonModel();
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
    case EnemyModelVisual::AirTank: return &resources.enemyAirTankModel();
    case EnemyModelVisual::Bee: return &resources.enemyBeeModel();
    case EnemyModelVisual::BeeElite: return &resources.enemyBeeEliteModel();
    case EnemyModelVisual::Flying:
        return &resources.enemyFlyingModel();
    case EnemyModelVisual::BossSummon:
        return &resources.enemyBossSummonModel();
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
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Idle";
            break;
        }
    } else if (modelVisual == EnemyModelVisual::AirTank || modelVisual == EnemyModelVisual::Bee || modelVisual == EnemyModelVisual::BeeElite || modelVisual == EnemyModelVisual::Flying || modelVisual == EnemyModelVisual::Boss ||
               modelVisual == EnemyModelVisual::BossSummon) {
        animations = modelVisual == EnemyModelVisual::AirTank ? &resources.enemyAirTankAnimations() :
            modelVisual == EnemyModelVisual::Bee ? &resources.enemyBeeAnimations() :
            modelVisual == EnemyModelVisual::BeeElite ? &resources.enemyBeeEliteAnimations() :
            modelVisual == EnemyModelVisual::Boss ? &resources.enemyBossAnimations() :
            modelVisual == EnemyModelVisual::BossSummon ? &resources.enemyBossSummonAnimations() :
            &resources.enemyFlyingAnimations();
        native = true;
        switch (animationVisual) {
        case EnemyAnimationVisual::Walk:
        case EnemyAnimationVisual::Run:
            clip = "Fast_Flying";
            break;
        case EnemyAnimationVisual::RangedAttack:
            clip = "Punch";
            break;
        case EnemyAnimationVisual::MeleeAttack:
        case EnemyAnimationVisual::SapperAttack:
            clip = "Headbutt";
            break;
        case EnemyAnimationVisual::Hit:
            clip = "HitReact";
            break;
        case EnemyAnimationVisual::Idle:
        case EnemyAnimationVisual::Spawn:
            clip = "Flying_Idle";
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
    case EnemyModelVisual::AirTank: return {152,174,81,tint.a};
    case EnemyModelVisual::Bee:
    case EnemyModelVisual::BeeElite: return {255,204,48,tint.a};
    case EnemyModelVisual::Flying:
        return {102, 71, 167, tint.a};
    case EnemyModelVisual::BossSummon:
        return {155, 197, 74, tint.a};
    case EnemyModelVisual::Boss:
        return {185, 207, 66, tint.a};
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

const std::vector<Matrix>& Renderer::enemyBonePose(
    EnemyModelVisual visual, EnemyAnimationVisual animationVisual, int frame) {
    static const std::vector<Matrix> Empty;
    const EnemyPoseKey key{visual, animationVisual, frame};
    const auto found = enemyBonePoseCache_.find(key);
    if (found != enemyBonePoseCache_.end()) return found->second;
    ModelResource* resource = enemyModelFor(resources_, visual);
    if (!resource || !resource->valid()) return Empty;
    Model& model = resource->get();
    const auto animation = enemyAnimationFor(resources_, visual, animationVisual);
    const auto* clip = animation.animations->find(animation.clipName);
    if (!clip || frame < 0 || frame >= clip->keyframeCount ||
        model.skeleton.boneCount <= 0 || !model.skeleton.bindPose || !model.boneMatrices) return Empty;
    if (animation.nativeSkeleton) {
        UpdateModelAnimation(model, *clip, static_cast<float>(frame));
    } else {
        const auto& mapping = enemyBoneMapping(visual, model, *animation.sourceBones);
        enemyAnimationPose_.resize(static_cast<std::size_t>(model.skeleton.boneCount));
        for (int bone = 0; bone < model.skeleton.boneCount; ++bone) {
            const int source = mapping[static_cast<std::size_t>(bone)];
            enemyAnimationPose_[static_cast<std::size_t>(bone)] = source >= 0 && source < clip->boneCount
                ? clip->keyframePoses[frame][source] : model.skeleton.bindPose[bone];
        }
        enemyAnimationFrames_.assign(1U, enemyAnimationPose_.data());
        ModelAnimation remapped{};
        remapped.boneCount = model.skeleton.boneCount;
        remapped.keyframeCount = 1;
        remapped.keyframePoses = enemyAnimationFrames_.data();
        UpdateModelAnimation(model, remapped, 0);
    }
    return enemyBonePoseCache_.emplace(key, std::vector<Matrix>(
        model.boneMatrices, model.boneMatrices + model.skeleton.boneCount)).first->second;
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
        const auto sample = sampleEnemyAnimation(animationSeconds, clip->keyframeCount,
            loop, quantizeCrowdPose);
        if (quantizeCrowdPose) motionTime = static_cast<float>(sample.frame) / EnemyAnimationFps;
        if (modelResource->gpuSkinningCompatible()) {
            const auto& from = enemyBonePose(modelVisual, animationVisual, sample.frame);
            const auto& to = enemyBonePose(modelVisual, animationVisual, sample.nextFrame);
            for (int bone = 0; bone < model.skeleton.boneCount; ++bone) {
                model.boneMatrices[bone] = blendEnemyBoneMatrices(
                    from[static_cast<std::size_t>(bone)], to[static_cast<std::size_t>(bone)], sample.blend);
            }
        } else if (animation.nativeSkeleton) {
            UpdateModelAnimation(model, *clip, static_cast<float>(sample.frame) + sample.blend);
        } else {
            const auto& mapping = enemyBoneMapping(modelVisual, model, *animation.sourceBones);
            enemyAnimationPose_.resize(static_cast<std::size_t>(model.skeleton.boneCount));
            for (int bone = 0; bone < model.skeleton.boneCount; ++bone) {
                const int source = mapping[static_cast<std::size_t>(bone)];
                Transform pose = model.skeleton.bindPose[bone];
                if (source >= 0 && source < clip->boneCount) {
                    const Transform a = clip->keyframePoses[sample.frame][source];
                    const Transform b = clip->keyframePoses[sample.nextFrame][source];
                    pose = {Vector3Lerp(a.translation, b.translation, sample.blend),
                        QuaternionSlerp(a.rotation, b.rotation, sample.blend),
                        Vector3Lerp(a.scale, b.scale, sample.blend)};
                }
                enemyAnimationPose_[static_cast<std::size_t>(bone)] = pose;
            }
            enemyAnimationFrames_.assign(1U, enemyAnimationPose_.data());
            ModelAnimation remapped{};
            remapped.boneCount = model.skeleton.boneCount;
            remapped.keyframeCount = 1;
            remapped.keyframePoses = enemyAnimationFrames_.data();
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
        // Material copies share their maps with the model. Status tint must
        // not persist into subsequent focused enemies or selection passes.
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
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
        instance.eliteAffixes, instance.tint.a, instance.modelVisual != EnemyModelVisual::AirTank);
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
        const bool smoothAnimation = !lowDetail && instance.smoothAnimation && clip &&
            clip->keyframeCount > 0;
        if (smoothAnimation && (!resource->gpuSkinningCompatible() ||
            resource->get().skeleton.boneCount > MaximumGpuSkinningBones / 2)) {
            return false; // Preserve full interpolation through the individual fallback.
        }
        const auto sampled = sampleEnemyAnimation(instance.animationSeconds,
            clip ? clip->keyframeCount : 0, instance.loop, !smoothAnimation);
        const int frame = sampled.frame;
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
            instance.inkOutlineEligible, smoothAnimation};
        EnemyBatch& batch = enemyBatches_[key];
        batch.lastUsedFrame = enemyBatchFrame_;
        if (batch.transforms.empty()) {
            batch.representative = instance;
            batch.representative.animationSeconds = static_cast<float>(frame) / EnemyAnimationFps;
            batch.representative.smoothAnimation = smoothAnimation;
            batch.animationBlend = sampled.blend;
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
            const float motionTime = smoothAnimation ? instance.animationSeconds
                : static_cast<float>(sampled.frame) / EnemyAnimationFps;
            transform = MatrixMultiply(resource->get().transform,
                enemyPresentationTransform(instance.modelVisual, instance.animationVisual,
                    motionTime, instance.position, instance.yawRadians, uniformScale,
                    instance.bodyStretch, instance.bodyRoll));
        }
        if (smoothAnimation) transform.m3 = sampled.blend;
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
        const int frame = clip && clip->keyframeCount > 0 ? std::clamp(static_cast<int>(std::lround(
            representative.animationSeconds * EnemyAnimationFps)), 0, clip->keyframeCount - 1) : 0;
        const std::vector<Matrix>& pose = enemyBonePose(
            representative.modelVisual, representative.animationVisual, frame);
        const Matrix* batchBoneMatrices = pose.empty() ? model.boneMatrices : pose.data();
        int paletteCount = model.skeleton.boneCount;
        const int poseOffset = representative.smoothAnimation && !pose.empty()
            ? model.skeleton.boneCount : 0;
        std::array<Matrix, MaximumGpuSkinningBones> palettes{};
        if (poseOffset > 0) {
            const int nextFrame = representative.loop ? (frame + 1) % clip->keyframeCount
                : std::min(frame + 1, clip->keyframeCount - 1);
            const auto& nextPose = enemyBonePose(representative.modelVisual,
                representative.animationVisual, nextFrame);
            std::copy(pose.begin(), pose.end(), palettes.begin());
            std::copy(nextPose.begin(), nextPose.end(), palettes.begin() + poseOffset);
            batchBoneMatrices = palettes.data();
            paletteCount *= 2;
        }
        const bool batchSkinningEnabled = modelSkinningEnabled &&
            finiteBoneMatrices(batchBoneMatrices, paletteCount);
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
            if (poseOffset > 0) {
                SetShaderValue(shader, worldBonePoseOffsetLocation_, &poseOffset, SHADER_UNIFORM_INT);
                SetShaderValue(shader, worldBonePoseBlendLocation_, &batch.animationBlend, SHADER_UNIFORM_FLOAT);
            }
            if (meshSkinningEnabled && !bonesUploaded) {
                bonesUploaded = uploadBoneMatrices(
                    shader, batchBoneMatrices, paletteCount);
            }
            Material material = model.materials[materialIndex];
            material.shader = shader;
            const Color original =
                material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                ColorTint(original, representative.tint);
            setInstancing(multipleInstances);
            if (multipleInstances) {
                instanceBuffers_.drawMesh(
                    model.meshes[meshIndex], material,
                    instanceBatch);
            } else {
                Matrix transform = batch.transforms.front();
                if (poseOffset > 0) transform.m3 = 0.0F;
                DrawMesh(model.meshes[meshIndex], material, transform);
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
            !instance.smoothAnimation, instance.eliteAffixes, instance.bodyStretch, instance.bodyRoll));
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
