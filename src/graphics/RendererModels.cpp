#include "graphics/Renderer.hpp"
#include "buildings/CannonRig.hpp"
#include "buildings/CatapultRig.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "buildings/BuildingSystem.hpp"
#include "ui/UiText.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <ranges>
#include <tuple>
#include <utility>

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

constexpr int PlatformLegCount = 4;
constexpr int PlatformTopMeshIndex = 4;
constexpr int PlatformMeshCount = PlatformTopMeshIndex + 1;
constexpr float PlatformLegTopY = -0.12480831F;
constexpr float PlatformLegSpan = 3.87519169F;
// Authoring/import scale is shared by regular enemy rendering, instancing,
// collision bounds and target UI. Gameplay archetype scale is supplied by
// AppRenderSupport::enemyVisualScale.
constexpr float EnemyImportScale = 0.82F;
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
        modelVisual == EnemyModelVisual::Splitter ||
        modelVisual == EnemyModelVisual::Splitling) {
        if (modelVisual == EnemyModelVisual::Rogue) {
            animations = &resources.enemyNinjaAnimations();
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
        return {55, 118, 154, tint.a};
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

std::optional<double>
renderer_model_detail::modelColliderRaycastDistance(
    const ModelResource& resource, Matrix transform,
    Ray ray, double maxDistance) {
    return modelColliderRaycastDistanceImpl(
        resource, transform, ray, maxDistance);
}

using namespace renderer_model_detail;

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

bool Renderer::drawFirstPersonTool(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    float iceChargeProgress, float iceRecoilProgress) {
    ModelResource* resource = nullptr;
    const bool iceWand = visual == FirstPersonToolVisual::IceWand;
    const bool fireWand = visual == FirstPersonToolVisual::FireWand;
    const bool wand = iceWand || fireWand;
    const bool bomb = visual == FirstPersonToolVisual::Bomb;
    switch (visual) {
    case FirstPersonToolVisual::None:
        break;
    case FirstPersonToolVisual::Axe:
        resource = &resources_.axeModel();
        break;
    case FirstPersonToolVisual::Pickaxe:
        resource = &resources_.pickaxeModel();
        break;
    case FirstPersonToolVisual::Club:
        resource = &resources_.clubModel();
        break;
    case FirstPersonToolVisual::IceWand:
    case FirstPersonToolVisual::FireWand:
        resource = &resources_.iceWandModel();
        break;
    case FirstPersonToolVisual::Hammer:
        resource = &resources_.hammerModel();
        break;
    case FirstPersonToolVisual::Bomb:
        break;
    }
    if (!bomb && (resource == nullptr || !resource->valid())) {
        return false;
    }

    const float progress = std::clamp(swingProgress, 0.0F, 1.0F);
    const auto smoothStep = [](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    };
    float swingPitch = 0.0F;
    float swingPush = 0.0F;
    if (progress > 0.0F && progress < 0.22F) {
        const float phase = smoothStep(progress / 0.22F);
        swingPitch = tuning.windupDegrees * phase;
    } else if (progress >= 0.22F && progress < 0.53F) {
        const float phase = smoothStep((progress - 0.22F) / 0.31F);
        swingPitch = tuning.windupDegrees +
                     (tuning.strikeDegrees - tuning.windupDegrees) * phase;
        swingPush = std::sin(phase * PI) * tuning.depthPush;
    } else if (progress >= 0.53F) {
        const float phase = smoothStep((progress - 0.53F) / 0.47F);
        swingPitch = tuning.strikeDegrees * (1.0F - phase);
    }

    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
                      std::max(tuning.movementBob, 0.0F);
    const float bobX = std::sin(movementPhase) * 0.012F * bob;
    const float bobY =
        std::abs(std::cos(movementPhase)) * 0.014F * bob;
    const float charge = smoothStep(std::clamp(iceChargeProgress, 0.0F, 1.0F));
    const float recoil = std::sin(
        std::clamp(iceRecoilProgress, 0.0F, 1.0F) * PI);
    rlPushMatrix();
    const float wandX = wand
        ? tuning.position.x + 0.035F + bobX
        : tuning.position.x + bobX;
    const float wandY = wand
        ? tuning.position.y - bobY + charge * 0.045F
        : tuning.position.y - bobY;
    const float wandZ = wand
        ? tuning.position.z + recoil * 0.075F
        : tuning.position.z + swingPush;
    rlTranslatef(wandX, wandY, wandZ);
    // Swing in camera space. Applying this before the model's local pose
    // keeps the strike aimed forward even when the tool is yawed in-hand.
    rlRotatef(swingPitch, 1.0F, 0.0F, 0.0F);
    // Blender exports the handle along local +Y. With the origin at the
    // grip, these rotations move the head without making the hand slide.
    rlRotatef(tuning.rotation.x, 1.0F, 0.0F, 0.0F);
    rlRotatef(tuning.rotation.y, 0.0F, 1.0F, 0.0F);
    rlRotatef(tuning.rotation.z, 0.0F, 0.0F, 1.0F);
    const float modelScale = bomb
        ? tuning.scale / 1.5F
        : tuning.scale;
    rlScalef(modelScale, modelScale, modelScale);
    if (bomb) {
        DrawSphereEx({}, 0.27F, 12, 12, {49, 55, 63, 255});
        DrawCylinder({0.0F, 0.29F, 0.0F}, 0.055F, 0.04F,
                     0.16F, 8, {124, 80, 39, 255});
        DrawSphere({0.0F, 0.39F, 0.0F}, 0.035F,
                   {255, 152, 38, 255});
    } else {
        Model& model = resource->get();
        DrawModel(model, {}, 1.0F,
                  fireWand ? Color{255, 193, 150, 255} : WHITE);
    }
    if (wand) {
        constexpr float CrystalHeight = 0.365F;
        const float crystalPulse = 0.42F + charge * 0.28F +
            0.035F * std::sin(static_cast<float>(GetTime()) * 5.0F);
        drawIceMagicSphere(
            {0.0F, CrystalHeight, 0.0F},
            0.052F + charge * 0.012F,
            static_cast<float>(GetTime()), crystalPulse,
            fireWand ? Color{255, 104, 24, 165}
                     : Color{142, 229, 255, 145});
        if (charge > 0.01F && settings_.particles) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int particle = 0; particle < 5; ++particle) {
                const float phase = static_cast<float>(GetTime()) *
                    (2.6F + static_cast<float>(particle) * 0.37F) +
                    static_cast<float>(particle) * 1.2566F;
                const float orbit = 0.18F * charge;
                DrawSphereEx(
                    {std::cos(phase) * orbit,
                     CrystalHeight + std::sin(phase * 1.3F) * orbit,
                     std::sin(phase) * orbit},
                    0.008F + charge * 0.005F, 4, 4,
                    fireWand ? Color{255, 128, 32, 155}
                             : Color{142, 229, 255, 135});
            }
            EndBlendMode();
        }
    }
    rlPopMatrix();
    return true;
}

void Renderer::drawIceMagicSphere(
    Vector3 position, float radius, float timeSeconds,
    float intensity, Color tint) {
    if (!resources_.iceMagicShader().valid()) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphereEx(position, radius, 10, 10, tint);
        EndBlendMode();
        return;
    }
    rlDrawRenderBatchActive();
    Shader& shader = resources_.iceMagicShader().get();
    const Vector4 shaderTint{
        tint.r / 255.0F, tint.g / 255.0F,
        tint.b / 255.0F, tint.a / 255.0F};
    SetShaderValue(shader, iceMagicTimeLocation_, &timeSeconds,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, iceMagicTintLocation_, &shaderTint,
                   SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, iceMagicIntensityLocation_, &intensity,
                   SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(shader);
    DrawSphereEx(position, radius, 10, 10, WHITE);
    EndShaderMode();
    EndBlendMode();
}

void Renderer::drawIceWandProjectile(
    const IceWandProjectile& projectile, Vector3 cameraPosition,
    float timeSeconds, float interpolationAlpha) {
    if (!projectile.active) {
        return;
    }
    const float alpha = std::clamp(interpolationAlpha, 0.0F, 1.0F);
    const Vector3 previous{
        static_cast<float>(projectile.previousPosition.x),
        static_cast<float>(projectile.previousPosition.y),
        static_cast<float>(projectile.previousPosition.z)};
    const Vector3 current{
        static_cast<float>(projectile.position.x),
        static_cast<float>(projectile.position.y),
        static_cast<float>(projectile.position.z)};
    const Vector3 position = Vector3Lerp(previous, current, alpha);
    const float radius = static_cast<float>(projectile.radius);
    const bool fire = projectile.element == WandElement::Fire;
    const Color trailOuterHead = fire
        ? Color{255, 92, 12, 170}
        : Color{52, 175, 255, 145};
    const Color trailOuterTail = fire
        ? Color{188, 28, 4, 125}
        : Color{24, 105, 235, 120};
    const Color trailInnerHead = fire
        ? Color{255, 244, 174, 245}
        : Color{223, 248, 255, 235};
    const Color trailInnerTail = fire
        ? Color{255, 146, 34, 215}
        : Color{142, 229, 255, 205};
    const auto fadedColor = [](Color color, float amount) {
        color.a = static_cast<unsigned char>(std::lround(
            static_cast<float>(color.a) *
            std::clamp(amount, 0.0F, 1.0F)));
        return color;
    };

    if (projectile.trailCount > 1U) {
        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlBegin(RL_TRIANGLES);
        const std::size_t count = std::min(
            std::min(projectile.trailCount, IceWandTrailPointCount),
            std::size_t{12});
        const auto trailPoint = [&projectile, position](std::size_t index) {
            if (index == 0U) {
                return position;
            }
            const Vec3& value = projectile.trail[index];
            return Vector3{
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z)};
        };
        const auto emitVertex = [](Vector3 vertex, Color color) {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(vertex.x, vertex.y, vertex.z);
        };
        for (std::size_t index = 0; index + 1 < count; ++index) {
            const Vector3 point = trailPoint(index);
            const Vector3 next = trailPoint(index + 1U);
            Vector3 tangent = Vector3Normalize(Vector3Subtract(point, next));
            Vector3 toCamera = Vector3Normalize(Vector3Subtract(cameraPosition, point));
            Vector3 side = Vector3Normalize(Vector3CrossProduct(tangent, toCamera));
            if (Vector3LengthSqr(side) < 0.0001F) {
                side = {1.0F, 0.0F, 0.0F};
            }
            const float fade = std::pow(
                1.0F - static_cast<float>(index) /
                    static_cast<float>(std::max<std::size_t>(count - 1, 1U)),
                1.15F);
            const float nextFade = std::pow(
                1.0F - static_cast<float>(index + 1U) /
                    static_cast<float>(std::max<std::size_t>(count - 1, 1U)),
                1.15F);
            const auto emitRibbon = [&](float width, float nextWidth,
                                        Color headColor, Color tailColor) {
                const Vector3 left = Vector3Subtract(
                    point, Vector3Scale(side, width));
                const Vector3 right = Vector3Add(
                    point, Vector3Scale(side, width));
                const Vector3 nextLeft = Vector3Subtract(
                    next, Vector3Scale(side, nextWidth));
                const Vector3 nextRight = Vector3Add(
                    next, Vector3Scale(side, nextWidth));
                emitVertex(left, headColor);
                emitVertex(right, headColor);
                emitVertex(nextRight, tailColor);
                emitVertex(left, headColor);
                emitVertex(nextRight, tailColor);
                emitVertex(nextLeft, tailColor);
            };
            emitRibbon(
                radius * (0.92F * fade + 0.05F),
                radius * (0.92F * nextFade + 0.025F),
                fadedColor(trailOuterHead, fade),
                fadedColor(trailOuterTail, nextFade));
            emitRibbon(
                radius * (0.28F * fade + 0.018F),
                radius * (0.28F * nextFade + 0.008F),
                fadedColor(trailInnerHead, fade),
                fadedColor(trailInnerTail, nextFade));
        }
        rlEnd();
        EndBlendMode();
    }

    const float pulse = 0.94F + 0.12F * std::sin(
        timeSeconds * 4.6F + static_cast<float>(projectile.id.index) * 0.41F);
    BeginBlendMode(BLEND_ALPHA);
    DrawSphereEx(position, radius * (0.94F + pulse * 0.025F), 12, 12,
                 fire ? Color{232, 61, 12, 255}
                      : Color{48, 145, 232, 255});
    EndBlendMode();
    drawIceMagicSphere(position, radius * (1.48F + pulse * 0.04F),
                       timeSeconds, 0.96F,
                       fire ? Color{255, 126, 28, 235}
                            : Color{142, 229, 255, 225});
    BeginBlendMode(BLEND_ADDITIVE);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(timeSeconds * 72.0F, 0.0F, 1.0F, 0.0F);
    DrawCircle3D({}, radius * 1.42F,
                 {1.0F, 0.0F, 0.0F}, 63.0F,
                 fire ? Color{255, 212, 91, 165}
                      : Color{191, 246, 255, 145});
    rlRotatef(117.0F, 0.45F, 0.72F, 0.53F);
    DrawCircle3D({}, radius * 1.27F,
                 {0.0F, 0.0F, 1.0F}, 47.0F,
                 fire ? Color{255, 76, 12, 135}
                      : Color{85, 207, 255, 112});
    rlPopMatrix();
    EndBlendMode();
    if (settings_.particles) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int spark = 0; spark < 4; ++spark) {
            const float phase = timeSeconds *
                (2.1F + static_cast<float>(spark) * 0.31F) +
                static_cast<float>(projectile.id.index % 17U) * 0.13F +
                static_cast<float>(spark) * 0.897F;
            const float distance = radius * (1.30F + 0.38F *
                std::sin(timeSeconds * 3.0F + static_cast<float>(spark) * 1.4F));
            const float sparkSize = radius *
                (0.075F + 0.035F * (0.5F + 0.5F * std::sin(
                    timeSeconds * 7.0F + static_cast<float>(spark))));
            DrawSphereEx(
                {position.x + std::cos(phase) * distance,
                 position.y + std::sin(phase * 1.7F) * distance,
                 position.z + std::sin(phase) * distance},
                sparkSize, 4, 4,
                spark % 3 == 0
                    ? (fire ? Color{255, 245, 181, 225}
                            : Color{223, 248, 255, 215})
                    : (fire ? Color{255, 113, 24, 185}
                            : Color{142, 229, 255, 172}));
            if (projectile.trailCount > 2U) {
                const std::size_t trailIndex = std::min<std::size_t>(
                    projectile.trailCount - 1U,
                    1U + static_cast<std::size_t>(spark) * 2U);
                const Vec3& trailPoint = projectile.trail[trailIndex];
                const Vector3 trailPosition{
                    static_cast<float>(trailPoint.x),
                    static_cast<float>(trailPoint.y),
                    static_cast<float>(trailPoint.z)};
                const Vector3 streakEnd{
                    trailPosition.x + std::cos(phase) * radius * 0.28F,
                    trailPosition.y + std::sin(phase * 1.3F) * radius * 0.18F,
                    trailPosition.z + std::sin(phase) * radius * 0.28F};
                DrawLine3D(
                    trailPosition, streakEnd,
                    fire
                        ? Color{255, 112, 18,
                              static_cast<unsigned char>(105.0F * pulse)}
                        : Color{142, 229, 255,
                              static_cast<unsigned char>(95.0F * pulse)});
            }
        }
        EndBlendMode();
    }
}

std::optional<double>
Renderer::platformFrameRaycastDistance(
    Vector3 topCenter, float scale,
    const std::array<float, 4>& supportLengths,
    Ray ray, double maxDistance) {
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

bool Renderer::drawCannon(Vector3 position, float yawRadians,
                          float pitchRadians, Color tint,
                          float scaleFactor) {
    auto& resource = resources_.cannonModel();
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

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    yawRadians * RAD2DEG,
                    {scaleFactor, scaleFactor, scaleFactor},
                    tint);
        return true;
    }

    const Matrix scale =
        MatrixScale(scaleFactor, scaleFactor, scaleFactor);
    const Matrix yaw = MatrixRotateY(yawRadians);
    const Matrix localPitch = MatrixMultiply(
        MatrixMultiply(
            MatrixTranslate(
                0.0F, -static_cast<float>(CannonPitchPivotY), 0.0F),
            MatrixRotateX(pitchRadians)),
        MatrixTranslate(
            0.0F, static_cast<float>(CannonPitchPivotY), 0.0F));
    const Matrix translation =
        MatrixTranslate(position.x, position.y, position.z);
    const Matrix baseTransform =
        MatrixMultiply(MatrixMultiply(scale, yaw), translation);
    const Matrix barrelTransform = MatrixMultiply(
        MatrixMultiply(MatrixMultiply(scale, localPitch), yaw),
        translation);

    const auto drawMesh = [&model, tint](int meshIndex,
                                         Matrix transform) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    // The authored collider is stripped, leaving the pitch-pivot barrel
    // first and the yaw-only body second.
    drawMesh(1, baseTransform);
    drawMesh(0, barrelTransform);
    for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
        drawMesh(meshIndex, baseTransform);
    }
    return true;
}

bool Renderer::drawCannonball(Vector3 position, Color tint) {
    auto& resource = resources_.cannonballModel();
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
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {1.0F, 1.0F, 1.0F}, tint);
    return true;
}

bool Renderer::drawCatapult(
    Vector3 position, float yawRadians, float armPitchRadians,
    bool loaded, Color tint, float scaleFactor) {
    auto& resource = resources_.catapultModel();
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

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    yawRadians * RAD2DEG,
                    {scaleFactor, scaleFactor, scaleFactor}, tint);
    } else {
        const Matrix scale =
            MatrixScale(scaleFactor, scaleFactor, scaleFactor);
        const Matrix yaw = MatrixRotateY(yawRadians);
        const Matrix localPitch = MatrixMultiply(
            MatrixMultiply(
                MatrixTranslate(
                    0.0F, -static_cast<float>(CatapultPitchPivotY), 0.0F),
                MatrixRotateX(armPitchRadians)),
            MatrixTranslate(
                0.0F, static_cast<float>(CatapultPitchPivotY), 0.0F));
        const Matrix translation =
            MatrixTranslate(position.x, position.y, position.z);
        const Matrix bodyTransform =
            MatrixMultiply(MatrixMultiply(scale, yaw), translation);
        const Matrix armTransform = MatrixMultiply(
            MatrixMultiply(MatrixMultiply(scale, localPitch), yaw),
            translation);
        const auto drawMesh = [&model, tint](int meshIndex,
                                             Matrix transform) {
            const int materialIndex = model.meshMaterial[meshIndex];
            Material& material = model.materials[materialIndex];
            const Color original =
                material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                ColorTint(original, tint);
            DrawMesh(model.meshes[meshIndex], material,
                     MatrixMultiply(model.transform, transform));
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        };
        // The collision mesh is stripped at import: the throwing arm is
        // first, while the remaining body only follows yaw.
        drawMesh(0, armTransform);
        drawMesh(1, bodyTransform);
        for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
            drawMesh(meshIndex, bodyTransform);
        }
    }

    if (loaded) {
        const Vec3 base{position.x, position.y, position.z};
        Vec3 socket = catapultMuzzleWorldPosition(
            base, yawRadians, armPitchRadians);
        socket.x = base.x + (socket.x - base.x) * scaleFactor;
        socket.y = base.y + (socket.y - base.y) * scaleFactor;
        socket.z = base.z + (socket.z - base.z) * scaleFactor;
        static_cast<void>(drawCatapultBall(
            {static_cast<float>(socket.x), static_cast<float>(socket.y),
             static_cast<float>(socket.z)}, tint));
    }
    return true;
}

bool Renderer::drawCatapultBall(Vector3 position, Color tint) {
    auto& resource = resources_.catapultBallModel();
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
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {1.0F, 1.0F, 1.0F}, tint);
    return true;
}

bool Renderer::drawArrow(Vector3 position, Vector3 direction, Color tint) {
    auto& resource = resources_.arrowModel();
    if (!resource.valid() ||
        Vector3LengthSqr(direction) <= 0.000001F) {
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

    const Vector3 forward = Vector3Normalize(direction);
    const float yaw = std::atan2(-forward.x, -forward.z);
    const float horizontal = std::hypot(forward.x, forward.z);
    const float pitch = std::atan2(forward.y, horizontal);

    constexpr float ArrowScale = 1.0F;
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(ArrowScale, ArrowScale, ArrowScale),
                MatrixRotateX(pitch)),
            MatrixRotateY(yaw)),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

bool Renderer::drawCrossbow(Vector3 position, float yawRadians,
                            Color tint, float scale,
                            float pitchRadians) {
    auto& resource = resources_.crossbowModel();
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
    constexpr float PitchPivotY = 0.14931101F;
    const Matrix localPitch = MatrixMultiply(
        MatrixMultiply(
            MatrixTranslate(0.0F, -PitchPivotY, 0.0F),
            MatrixRotateX(pitchRadians)),
        MatrixTranslate(0.0F, PitchPivotY, 0.0F));
    const Matrix yawTransform = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(scale, scale, scale),
            MatrixRotateY(yawRadians)),
        MatrixTranslate(position.x, position.y, position.z));
    const Matrix pitchedTransform = MatrixMultiply(
        MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale), localPitch),
            MatrixRotateY(yawRadians)),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        // After the authored collider is stripped, mesh 0 is the complete
        // upper assembly named "arrow" under pitch_pivot. Mesh 1 is the
        // yaw-only lower weapon body.
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform,
                     meshIndex == 0 ? pitchedTransform : yawTransform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

Vec3 Renderer::crossbowMuzzlePosition(
    Vec3 position, float yawRadians, float pitchRadians) const {
    constexpr double PivotY = 0.14931100606918335;
    constexpr Vec3 AuthoredMuzzle{
        0.0, 0.7464950680732727, -0.1994807869195938};
    const double relativeY = AuthoredMuzzle.y - PivotY;
    const double cosinePitch = std::cos(static_cast<double>(pitchRadians));
    const double sinePitch = std::sin(static_cast<double>(pitchRadians));
    const double pitchedY =
        PivotY + relativeY * cosinePitch - AuthoredMuzzle.z * sinePitch;
    const double pitchedZ =
        relativeY * sinePitch + AuthoredMuzzle.z * cosinePitch;
    const double cosineYaw = std::cos(static_cast<double>(yawRadians));
    const double sineYaw = std::sin(static_cast<double>(yawRadians));
    return {
        position.x + pitchedZ * sineYaw,
        position.y + pitchedY,
        position.z + pitchedZ * cosineYaw,
    };
}

bool Renderer::drawGunTurret(Vector3 position, float baseYawRadians,
                             float barrelYawRadians, Color tint,
                             float scale, Vec3 barrelRecoilOffset) {
    auto& resource = resources_.gunTurretModel();
    if (!resource.valid()) return false;
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
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    const auto transformFor = [position, scale](float yaw, Vec3 offset) {
        return MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale),
                           MatrixRotateY(yaw)),
            MatrixTranslate(
                position.x + static_cast<float>(offset.x),
                position.y + static_cast<float>(offset.y),
                position.z + static_cast<float>(offset.z)));
    };
    const Matrix baseTransform = transformFor(baseYawRadians, {});
    const Matrix barrelTransform = transformFor(
        barrelYawRadians, barrelRecoilOffset);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        // Collider is stripped while loading. Visible GLB order is base,
        // then the barrel under yaw_pivot.
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform,
                     meshIndex == 1 ? barrelTransform : baseTransform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

Vec3 Renderer::gunTurretMuzzlePosition(
    Vec3 position, float yawRadians, std::size_t muzzleIndex) {
    const auto& sockets =
        resources_.gunTurretModel().collisionAsset().sockets;
    if (sockets.empty()) {
        return {position.x, position.y + 0.7, position.z};
    }
    const Vec3 local = sockets[muzzleIndex % sockets.size()].position;
    const double cosine = std::cos(static_cast<double>(yawRadians));
    const double sine = std::sin(static_cast<double>(yawRadians));
    return {
        position.x + local.x * cosine + local.z * sine,
        position.y + local.y,
        position.z - local.x * sine + local.z * cosine,
    };
}

bool Renderer::drawTurretBullet(Vector3 position, Vector3 direction,
                                Color tint) {
    auto& resource = resources_.turretBulletModel();
    if (!resource.valid() ||
        Vector3LengthSqr(direction) <= 0.000001F) return false;
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    const Vector3 forward = Vector3Normalize(direction);
    const float yaw = std::atan2(-forward.x, -forward.z);
    const float horizontal = std::sqrt(
        forward.x * forward.x + forward.z * forward.z);
    const float pitch = std::atan2(forward.y, horizontal);
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(yaw)),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        const Texture2D originalTexture =
            material.maps[MATERIAL_MAP_DIFFUSE].texture;
        if (resources_.fallbackTexture().valid()) {
            material.maps[MATERIAL_MAP_DIFFUSE].texture =
                resources_.fallbackTexture().get();
        }
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint({255, 190, 86, 255}, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        material.maps[MATERIAL_MAP_DIFFUSE].texture = originalTexture;
    }
    return true;
}

bool Renderer::drawCore(Vector3 position, float yawRadians,
                        Color tint, float scale) {
    auto& resource = resources_.coreModel();
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
    constexpr float ModelScale = 2.0F;
    constexpr float GroundOffset = 0.005F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                yawRadians * RAD2DEG,
                {ModelScale * scale, ModelScale * scale,
                 ModelScale * scale},
                tint);
    return true;
}

bool Renderer::drawChallengeColumn(
    Vector3 position, float yawRadians, Color tint, float scale) {
    auto& resource = resources_.challengeColumnModel();
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
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(0.001F, bounds.max.y - bounds.min.y);
    const float modelScale = 3.25F / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale, modelScale * scale}, tint);
    return true;
}

bool Renderer::drawChallengeArenaPeg(
    Vector3 position, float yawRadians, Color tint, float scale) {
    auto& resource = resources_.challengeArenaPegModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(0.001F, bounds.max.y - bounds.min.y);
    constexpr float TargetHeight = 2.25F;
    const float modelScale = TargetHeight / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale, modelScale * scale}, tint);
    return true;
}

bool Renderer::drawWorldLandmark(
    std::size_t variant, Vector3 position,
    float yawRadians, Color tint, float scale) {
    auto& resource = resources_.worldLandmarkModel(variant);
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
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(
        0.001F, bounds.max.y - bounds.min.y);
    constexpr float TargetHeight = 6.76F;
    const float modelScale = TargetHeight / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale,
         modelScale * scale},
        tint);
    return true;
}

BoundingBox Renderer::worldLandmarkWorldBounds(
    std::size_t variant, Vector3 position,
    float yawRadians, float scale) {
    ModelResource& resource = resources_.worldLandmarkModel(variant);
    if (!resource.valid() || !world_transforms::finite(position) ||
        !std::isfinite(yawRadians) || !std::isfinite(scale) ||
        scale <= 0.0F) {
        return {};
    }
    const BoundingBox localBounds = resource.visualBounds();
    const float authoredHeight = std::max(
        0.001F, localBounds.max.y - localBounds.min.y);
    constexpr float TargetHeight = 6.76F;
    const float modelScale = TargetHeight / authoredHeight * scale;
    position.y -= localBounds.min.y * modelScale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    return world_transforms::transformBounds(localBounds, transform);
}

bool Renderer::drawMine(Vector3 position, float yawRadians,
                        Color tint, float scale) {
    return drawResourceProducer(
        BuildingType::CrystalMine, position, yawRadians,
        tint, scale);
}

bool Renderer::drawPlatformFrameModel(
    Vector3 topCenter, Color tint, float scale,
    const std::array<float, 4>& supportLengths) {
    auto& resource = resources_.platformModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    if (model.meshCount <= PlatformTopMeshIndex) {
        return false;
    }
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount;
             ++index) {
            model.materials[index].shader = *shader;
        }
    }
    const auto drawMesh = [&model, tint](
                              int meshIndex,
                              Matrix transform) {
        const int materialIndex =
            model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(
            model.meshes[meshIndex], material,
            MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    const auto transforms = platformMeshTransforms(
        topCenter, scale, supportLengths);
    drawMesh(
        PlatformTopMeshIndex,
        transforms[PlatformTopMeshIndex]);
    for (int legIndex = 0; legIndex < PlatformLegCount;
         ++legIndex) {
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
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount;
             ++index) {
            model.materials[index].shader = *shader;
        }
    }

    DrawModelEx(
        model, footprintCenter,
        {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {scale, scale, scale}, tint);
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
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    position.y += 0.005F;
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
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }

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
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            if (!resource.meshValid(
                    static_cast<std::size_t>(meshIndex))) continue;
            const int materialIndex = model.meshMaterial[meshIndex];
            if (materialIndex < 0 ||
                materialIndex >= model.materialCount) continue;
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                resourceRockTransforms_[variant].data(),
                static_cast<int>(
                    resourceRockTransforms_[variant].size()));
        }
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
    auto& resource = resources_.treeModel(visualVariant);
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
            visualVariant % TreeVisualVariantCount] * scale);
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {TreeModelScale * scale, TreeModelScale * scale,
         TreeModelScale * scale},
        tint);
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
        TreeVisualGroundOffsets[variant] * scale);
    const float modelScale = TreeModelScale * scale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
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
            TreeVisualGroundOffsets[variant] * scale);
        const float modelScale = TreeModelScale * scale;
        const Matrix rotation = MatrixRotateY(
            instance.yawRadians);
        resourceTreeTransforms_[variant].push_back(MatrixMultiply(
            resource.get().transform,
            MatrixMultiply(
                MatrixMultiply(
                    MatrixScale(
                        modelScale, modelScale, modelScale),
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
        Model& model = resources_.treeModel(variant).get();
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            if (!resources_.treeModel(variant).meshValid(
                    static_cast<std::size_t>(meshIndex))) {
                continue;
            }
            const int materialIndex = model.meshMaterial[meshIndex];
            if (materialIndex < 0 || materialIndex >= model.materialCount) {
                continue;
            }
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                resourceTreeTransforms_[variant].data(),
                static_cast<int>(
                    resourceTreeTransforms_[variant].size()));
        }
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, instancingLocation, &disabled,
        SHADER_UNIFORM_INT);
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
    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }

    position.y += 0.005F;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        modelYaw * RAD2DEG, {scale, scale, scale}, tint);
    return true;
}

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
    bool inkOutlineEligible, bool quantizeCrowdPose) {
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

    Shader* shader = nullptr;
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = &resources_.selectionMaskShader().get();
    } else if (
        shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (
        worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr && model.materials != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }

    const bool originalInkOutlineEligible =
        worldMaterial_.inkOutlineEligible;
    const bool worldInkChanged = worldShaderActive_ &&
        worldMaterial_.inkOutlineEligible != inkOutlineEligible;
    if (worldInkChanged) {
        WorldMaterialState material = worldMaterial_;
        material.inkOutlineEligible = inkOutlineEligible;
        setWorldMaterial(material);
    }
    const bool skinningEnabled =
        modelResource->gpuSkinningCompatible() &&
        modelResource->runtimeBoneMatricesFinite();
    const Matrix transform = MatrixMultiply(
        model.transform,
        MatrixMultiply(
            MatrixScale(
                EnemyImportScale * scale,
                EnemyImportScale * scale,
                EnemyImportScale * scale),
            MatrixMultiply(
                MatrixRotateY(yawRadians),
                MatrixTranslate(
                    position.x, position.y, position.z))));
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
    if (worldInkChanged) {
        WorldMaterialState material = worldMaterial_;
        material.inkOutlineEligible = originalInkOutlineEligible;
        setWorldMaterial(material);
    }
    return true;
}

BoundingBox Renderer::enemyWorldBounds(
    EnemyModelVisual modelVisual, Vector3 position,
    float yawRadians, float scale) const {
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
    return world_transforms::transformBounds(
        resource->visualBounds(), transform);
}

bool Renderer::drawEnemiesInstanced(
    std::span<const EnemyDrawInstance> instances) {
    if (instances.empty() || shadowPassOpen_ ||
        selectionMaskPassOpen_ || !worldShaderActive_ ||
        !resources_.worldShader().valid()) {
        return false;
    }
    const auto drawStart = PerformanceClock::now();

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
        const EnemyBatchKey key{
            instance.modelVisual, batchAnimation, batchFrame,
            tint, static_cast<int>(std::lround(instance.scale * 1000.0F)),
            batchLoop, lowDetail, instance.inkOutlineEligible};
        EnemyBatch& batch = enemyBatches_[key];
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
            proxyPosition.y += uniformScale;
            transform = MatrixMultiply(
                MatrixScale(
                    uniformScale * 0.9F,
                    uniformScale * 2.0F,
                    uniformScale * 0.9F),
                MatrixMultiply(
                    MatrixRotateY(instance.yawRadians),
                    MatrixTranslate(
                        proxyPosition.x, proxyPosition.y,
                        proxyPosition.z)));
        } else {
            transform = MatrixMultiply(
                resource->get().transform,
                MatrixMultiply(
                    MatrixMultiply(
                        MatrixScale(
                            uniformScale, uniformScale,
                            uniformScale),
                        MatrixRotateY(instance.yawRadians)),
                    MatrixTranslate(
                        instance.position.x, instance.position.y,
                        instance.position.z)));
        }
        batch.transforms.push_back(transform);
    }

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);

    std::size_t nonEmptyBatchCount = 0U;
    const bool originalInkOutlineEligible =
        worldMaterial_.inkOutlineEligible;
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
            WorldMaterialState batchMaterial = worldMaterial_;
            batchMaterial.inkOutlineEligible =
                representative.inkOutlineEligible;
            setWorldMaterial(batchMaterial);
            if (lowDetailPass) {
                setSkinningEnabled(shader, false);
                Material material = enemyCrowdLodModel_.materials[0];
                material.shader = shader;
                material.maps[MATERIAL_MAP_DIFFUSE].color =
                    crowdLodTint(
                        representative.modelVisual,
                        representative.tint);
                DrawMeshInstanced(
                    enemyCrowdLodModel_.meshes[0], material,
                    batch.transforms.data(),
                    static_cast<int>(batch.transforms.size()));
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
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                representative.tint;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                batch.transforms.data(),
                static_cast<int>(batch.transforms.size()));
        }
        }
    }

    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    WorldMaterialState restoredMaterial = worldMaterial_;
    restoredMaterial.inkOutlineEligible = originalInkOutlineEligible;
    setWorldMaterial(restoredMaterial);
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
    return true;
}

} // namespace ian
