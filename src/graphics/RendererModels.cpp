#include "graphics/Renderer.hpp"
#include "graphics/GateModel.hpp"
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
// Authoring/import scale is shared by regular enemy rendering, instancing,
// collision bounds and target UI. Gameplay archetype scale is supplied by
// AppRenderSupport::enemyVisualScale.
constexpr float EnemyImportScale = 0.82F;
constexpr std::array<std::size_t, PlatformLegCount>
    RotatedPlatformSupportIndices{1U, 0U, 3U, 2U};

Matrix transformMatrix(const Transform& transform) {
    return MatrixMultiply(
        MatrixMultiply(
            MatrixScale(transform.scale.x, transform.scale.y,
                        transform.scale.z),
            QuaternionToMatrix(transform.rotation)),
        MatrixTranslate(transform.translation.x,
                        transform.translation.y,
                        transform.translation.z));
}

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

void drawPortalMatterPlane(
    Texture2D texture, Vector3 center, float yawRadians,
    float halfExtent, float textureRotationRadians, Color tint) {
    // Portal.glb opens in its local YZ plane. Match that plane after yaw;
    // using the usual local XY billboard axes turns the matter 90 degrees
    // through the stone frame.
    const Vector3 normal{
        std::cos(yawRadians), 0.0F, -std::sin(yawRadians)};
    const Vector3 right{
        std::sin(yawRadians), 0.0F, std::cos(yawRadians)};
    center = Vector3Add(center, Vector3Scale(normal, 0.035F));
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    const float cosine = std::cos(textureRotationRadians);
    const float sine = std::sin(textureRotationRadians);
    const Vector3 horizontal = Vector3Scale(
        Vector3Add(
            Vector3Scale(right, cosine),
            Vector3Scale(worldUp, sine)),
        halfExtent);
    const Vector3 vertical = Vector3Scale(
        Vector3Add(
            Vector3Scale(right, -sine),
            Vector3Scale(worldUp, cosine)),
        halfExtent);
    constexpr int SegmentCount = 48;
    rlSetTexture(texture.id);
    rlBegin(RL_TRIANGLES);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    for (int segment = 0; segment < SegmentCount; ++segment) {
        const float firstAngle =
            2.0F * PI * static_cast<float>(segment) /
            static_cast<float>(SegmentCount);
        const float secondAngle =
            2.0F * PI * static_cast<float>(segment + 1) /
            static_cast<float>(SegmentCount);
        const auto emit = [&](Vector3 vertex, Vector2 uv) {
            rlTexCoord2f(uv.x, uv.y);
            rlVertex3f(vertex.x, vertex.y, vertex.z);
        };
        emit(center, {0.5F, 0.5F});
        emit(
            Vector3Add(
                center,
                Vector3Add(
                    Vector3Scale(horizontal, std::cos(firstAngle)),
                    Vector3Scale(vertical, std::sin(firstAngle)))),
            {0.5F + 0.5F * std::cos(firstAngle),
             0.5F - 0.5F * std::sin(firstAngle)});
        emit(
            Vector3Add(
                center,
                Vector3Add(
                    Vector3Scale(horizontal, std::cos(secondAngle)),
                    Vector3Scale(vertical, std::sin(secondAngle)))),
            {0.5F + 0.5F * std::cos(secondAngle),
             0.5F - 0.5F * std::sin(secondAngle)});
    }
    rlEnd();
    rlSetTexture(0);
}

void drawPortalGlowRing(
    Vector3 center, float yawRadians, float matterRadius,
    Color tint) {
    const Vector3 normal{
        std::cos(yawRadians), 0.0F, -std::sin(yawRadians)};
    const Vector3 right{
        std::sin(yawRadians), 0.0F, std::cos(yawRadians)};
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    center = Vector3Add(center, Vector3Scale(normal, 0.052F));
    constexpr int SegmentCount = 48;
    constexpr std::array<float, 3> RadiusFactors{
        0.90F, 1.01F, 1.15F};
    const std::array<unsigned char, 3> Alphas{
        0U, tint.a, 0U};
    const auto point = [&](float radius, float angle) {
        return Vector3Add(
            center,
            Vector3Add(
                Vector3Scale(right, std::cos(angle) * radius),
                Vector3Scale(worldUp, std::sin(angle) * radius)));
    };
    const auto emit = [&](Vector3 vertex, unsigned char alpha) {
        rlColor4ub(tint.r, tint.g, tint.b, alpha);
        rlVertex3f(vertex.x, vertex.y, vertex.z);
    };
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlNormal3f(normal.x, normal.y, normal.z);
    for (int segment = 0; segment < SegmentCount; ++segment) {
        const float firstAngle =
            2.0F * PI * static_cast<float>(segment) /
            static_cast<float>(SegmentCount);
        const float secondAngle =
            2.0F * PI * static_cast<float>(segment + 1) /
            static_cast<float>(SegmentCount);
        for (std::size_t band = 0; band < 2U; ++band) {
            const float innerRadius =
                matterRadius * RadiusFactors[band];
            const float outerRadius =
                matterRadius * RadiusFactors[band + 1U];
            const Vector3 innerFirst = point(innerRadius, firstAngle);
            const Vector3 innerSecond = point(innerRadius, secondAngle);
            const Vector3 outerFirst = point(outerRadius, firstAngle);
            const Vector3 outerSecond = point(outerRadius, secondAngle);
            emit(innerFirst, Alphas[band]);
            emit(outerFirst, Alphas[band + 1U]);
            emit(outerSecond, Alphas[band + 1U]);
            emit(innerFirst, Alphas[band]);
            emit(outerSecond, Alphas[band + 1U]);
            emit(innerSecond, Alphas[band]);
        }
    }
    rlEnd();
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

bool Renderer::hasAlternateHarvestSwingAnimation() const {
    return resources_.pickaxeAlternateAnimation().valid();
}

bool Renderer::drawFirstPersonTool(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    float iceChargeProgress, float iceRecoilProgress,
    bool alternateHarvestSwing) {
    ModelResource* resource = nullptr;
    const bool iceWand = visual == FirstPersonToolVisual::IceWand;
    const bool fireWand = visual == FirstPersonToolVisual::FireWand;
    const bool wand = iceWand || fireWand;
    const bool bomb = visual == FirstPersonToolVisual::Bomb;
    const bool axe = visual == FirstPersonToolVisual::Axe;
    const bool pickaxe = visual == FirstPersonToolVisual::Pickaxe;
    const bool club = visual == FirstPersonToolVisual::Club;
    const bool axeLike = axe || club;
    const bool authoredHarvestSwing = axeLike || pickaxe;
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
    if (!authoredHarvestSwing && progress > 0.0F && progress < 0.22F) {
        const float phase = smoothStep(progress / 0.22F);
        swingPitch = tuning.windupDegrees * phase;
    } else if (!authoredHarvestSwing && progress >= 0.22F &&
               progress < 0.53F) {
        const float phase = smoothStep((progress - 0.22F) / 0.31F);
        swingPitch = tuning.windupDegrees +
                     (tuning.strikeDegrees - tuning.windupDegrees) * phase;
        swingPush = std::sin(phase * PI) * tuning.depthPush;
    } else if (!authoredHarvestSwing && progress >= 0.53F) {
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
    constexpr Vector3 LegacyPickaxePosition{
        0.55401313F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyAxePosition{
        0.39473686F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyPickaxeRotation{22.0F, -60.0F, 7.0F};
    constexpr float LegacyPickaxeScale = 0.74F;
    const Vector3 authoredPosition = axeLike
        ? LegacyAxePosition : LegacyPickaxePosition;
    const float wandX = authoredHarvestSwing
        ? tuning.position.x - authoredPosition.x + bobX
        : (wand ? tuning.position.x + 0.035F + bobX
                : tuning.position.x + bobX);
    const float wandY = authoredHarvestSwing
        ? tuning.position.y - authoredPosition.y - bobY
        : (wand ? tuning.position.y - bobY + charge * 0.045F
                : tuning.position.y - bobY);
    const float wandZ = authoredHarvestSwing
        ? tuning.position.z - authoredPosition.z
        : (wand ? tuning.position.z + recoil * 0.075F
                : tuning.position.z + swingPush);
    rlTranslatef(wandX, wandY, wandZ);
    // Swing in camera space. Applying this before the model's local pose
    // keeps the strike aimed forward even when the tool is yawed in-hand.
    if (!authoredHarvestSwing) {
        rlRotatef(swingPitch, 1.0F, 0.0F, 0.0F);
    }
    // Blender exports the handle along local +Y. With the origin at the
    // grip, these rotations move the head without making the hand slide.
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.x - LegacyPickaxeRotation.x
                  : tuning.rotation.x,
              1.0F, 0.0F, 0.0F);
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.y - LegacyPickaxeRotation.y
                  : tuning.rotation.y,
              0.0F, 1.0F, 0.0F);
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.z - LegacyPickaxeRotation.z
                  : tuning.rotation.z,
              0.0F, 0.0F, 1.0F);
    const float modelScale = bomb
        ? tuning.scale / 1.5F
        : (authoredHarvestSwing
               ? tuning.scale / LegacyPickaxeScale
               : tuning.scale);
    rlScalef(modelScale, modelScale, modelScale);
    Shader* viewModelShader = resources_.viewModelShader().valid()
        ? &resources_.viewModelShader().get()
        : nullptr;
    if (bomb) {
        if (viewModelShader != nullptr) {
            BeginShaderMode(*viewModelShader);
        }
        DrawSphereEx({}, 0.27F, 12, 12, {49, 55, 63, 255});
        DrawCylinder({0.0F, 0.29F, 0.0F}, 0.055F, 0.04F,
                     0.16F, 8, {124, 80, 39, 255});
        DrawSphere({0.0F, 0.39F, 0.0F}, 0.035F,
                   {255, 152, 38, 255});
        if (viewModelShader != nullptr) {
            EndShaderMode();
        }
    } else {
        Model& model = resource->get();
        if (viewModelShader != nullptr) {
            for (int index = 0; index < model.materialCount; ++index) {
                model.materials[index].shader = *viewModelShader;
            }
        }
        const Matrix originalTransform = model.transform;
        const GlbNodeAnimation& animation =
            alternateHarvestSwing &&
                    resources_.pickaxeAlternateAnimation().valid()
                ? resources_.pickaxeAlternateAnimation()
                : resources_.pickaxeAnimation();
        if (authoredHarvestSwing && animation.valid()) {
            const Matrix animated = transformMatrix(
                animation.sample(progress));
            if (pickaxe) {
                const Matrix base = transformMatrix(
                    animation.baseTransform());
                model.transform = MatrixMultiply(
                    MatrixInvert(base), animated);
            } else {
                // The pickaxe mesh has this Blender child-object scale baked
                // into its vertices. Apply it to the standalone axe mesh so
                // both tools share the authored pivot and camera space.
                constexpr float AuthoredChildScale = 5.307597F;
                model.transform = MatrixMultiply(
                    MatrixScale(AuthoredChildScale,
                                AuthoredChildScale,
                                AuthoredChildScale),
                    animated);
            }
        }
        DrawModel(model, {}, 1.0F,
                  fireWand ? Color{255, 193, 150, 255} : WHITE);
        model.transform = originalTransform;
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

void Renderer::drawFirstPersonSwingTrail(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    const Camera3D& viewModelCamera,
    bool alternateHarvestSwing) {
    const bool axe = visual == FirstPersonToolVisual::Axe;
    const bool club = visual == FirstPersonToolVisual::Club;
    const bool axeLike = axe || club;
    if (!axeLike && visual != FirstPersonToolVisual::Pickaxe) return;
    const GlbNodeAnimation& animation =
        alternateHarvestSwing &&
                resources_.pickaxeAlternateAnimation().valid()
            ? resources_.pickaxeAlternateAnimation()
            : resources_.pickaxeAnimation();
    if (!animation.valid()) return;

    const float progress = std::clamp(swingProgress, 0.0F, 1.0F);
    const float trailProgress = progress -
        (alternateHarvestSwing ? 10.0F / 60.0F : 0.0F);
    if (trailProgress < 0.14F || trailProgress > 0.60F) return;
    const auto smoothStep = [](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    };
    const float visibility =
        smoothStep((trailProgress - 0.14F) / 0.09F) *
        (1.0F - smoothStep((trailProgress - 0.39F) / 0.21F));
    if (visibility <= 0.001F) return;

    // Match the outer transform used by drawFirstPersonTool. The sampled
    // pivot moves the blade; its recent positions form the translucent arc.
    const Vector3 legacyPosition = axeLike
        ? Vector3{0.39473686F, -0.349843F, -0.7427701F}
        : Vector3{0.55401313F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyRotation{22.0F, -60.0F, 7.0F};
    constexpr float ChildScale = 5.307597F;
    constexpr float LegacyScale = 0.74F;
    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
        std::max(tuning.movementBob, 0.0F);
    const Vector3 offset{
        tuning.position.x - legacyPosition.x +
            std::sin(movementPhase) * 0.012F * bob,
        tuning.position.y - legacyPosition.y -
            std::abs(std::cos(movementPhase)) * 0.014F * bob,
        tuning.position.z - legacyPosition.z,
    };
    const float scale = tuning.scale / LegacyScale;
    Matrix outer = MatrixScale(scale, scale, scale);
    outer = MatrixMultiply(outer, MatrixRotateZ(
        (tuning.rotation.z - LegacyRotation.z) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateY(
        (tuning.rotation.y - LegacyRotation.y) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateX(
        (tuning.rotation.x - LegacyRotation.x) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixTranslate(
        offset.x, offset.y, offset.z));

    const Vector3 bladeEdge = axeLike
        ? Vector3{-0.21F, 0.35F, 0.0F}
        : Vector3{-0.36F, 0.52F, 0.0F};
    const Vector3 bladeInsideFullWidth = axeLike
        ? Vector3{-0.02F, 0.24F, 0.0F}
        : Vector3{-0.13F, 0.37F, 0.0F};
    const Vector3 bladeInside = Vector3Lerp(
        bladeEdge, bladeInsideFullWidth, 0.5F);
    const auto projectedBlade = [&](float sampleProgress,
                                    Vector3 localPoint) {
        const Matrix pivot = transformMatrix(
            animation.sample(sampleProgress));
        const Vector3 point = Vector3Transform(
            Vector3Scale(localPoint, ChildScale), pivot);
        return GetWorldToScreen(
            Vector3Transform(point, outer), viewModelCamera);
    };
    const auto colorFor = [visibility](float age) {
        const float fade = std::pow(
            std::clamp(1.0F - age, 0.0F, 1.0F), 1.25F);
        const float shade = 1.0F - std::clamp(age, 0.0F, 1.0F);
        return Color{
            static_cast<unsigned char>(std::lround(246.0F - 80.0F * shade)),
            static_cast<unsigned char>(std::lround(237.0F - 78.0F * shade)),
            static_cast<unsigned char>(std::lround(221.0F - 72.0F * shade)),
            static_cast<unsigned char>(std::lround(
                std::clamp(visibility * fade * 92.0F, 0.0F, 255.0F))),
        };
    };
    const auto emit = [](Vector2 point, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point.x, point.y);
    };

    constexpr int Segments = 18;
    constexpr float History = 0.22F;
    constexpr float HeadLag = 0.012F;
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int segment = 0; segment < Segments; ++segment) {
        const float age0 = static_cast<float>(segment) / Segments;
        const float age1 = static_cast<float>(segment + 1) / Segments;
        const float time0 = progress - HeadLag - age0 * History;
        const float time1 = progress - HeadLag - age1 * History;
        if (time1 < 0.0F) break;
        const Vector2 edge0 = projectedBlade(time0, bladeEdge);
        const Vector2 inside0 = projectedBlade(time0, bladeInside);
        const Vector2 edge1 = projectedBlade(time1, bladeEdge);
        const Vector2 inside1 = projectedBlade(time1, bladeInside);
        const Color color0 = colorFor(age0);
        const Color color1 = colorFor(age1);
        emit(edge0, color0);
        emit(inside0, color0);
        emit(inside1, color1);
        emit(edge0, color0);
        emit(inside1, color1);
        emit(edge1, color1);
    }
    rlEnd();
    rlEnableBackfaceCulling();
    EndBlendMode();
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

bool Renderer::drawEnemyPortal(
    Vector3 position, float yawRadians,
    float matterRotationRadians, const WorldLighting& lighting,
    float scale) {
    ModelResource& portal = resources_.enemyPortalModel();
    TextureResource& matter = resources_.portalMatterTexture();
    if (!portal.valid() || !matter.valid() || scale <= 0.001F) {
        return false;
    }

    constexpr float PortalCenterHeight = 1.74F;
    constexpr float PortalGroundLift = -0.24F;
    constexpr float MatterRadius = 2.13F;
    Model& frame = portal.get();
    beginWorldShader(lighting);
    WorldMaterialState stoneMaterial{};
    stoneMaterial.bakedAo = 0.78F;
    stoneMaterial.screenAoAmount = 0.0F;
    setWorldMaterial(stoneMaterial);
    if (resources_.worldShader().valid()) {
        for (int index = 0; index < frame.materialCount; ++index) {
            frame.materials[index].shader = resources_.worldShader().get();
        }
    }
    Vector3 framePosition = position;
    framePosition.y += PortalGroundLift * scale;
    DrawModelEx(
        frame, framePosition,
        {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {scale, scale, scale}, WHITE);
    endWorldShader();

    const Vector3 matterCenter{
        position.x,
        position.y + PortalCenterHeight * scale,
        position.z,
    };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ALPHA);
    const bool masksBackgroundInk =
        resources_.portalMatterShader().valid();
    if (masksBackgroundInk) {
        BeginShaderMode(resources_.portalMatterShader().get());
    }
    drawPortalMatterPlane(
        matter.get(), matterCenter, yawRadians,
        MatterRadius * scale, matterRotationRadians,
        WHITE);
    if (masksBackgroundInk) {
        EndShaderMode();
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    drawPortalGlowRing(
        matterCenter, yawRadians, MatterRadius * scale,
        {194, 72, 255, 72});
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
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
