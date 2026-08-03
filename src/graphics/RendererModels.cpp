#include "graphics/Renderer.hpp"

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
#include <tuple>

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
constexpr float TreeModelScale = 1.0F;
constexpr float RockModelScale = 2.0F;
constexpr float RockGroundOffset = 0.204F;
constexpr float DecorativeRockClusterSize = 18.0F;
constexpr std::array<std::size_t, PlatformLegCount>
    RotatedPlatformSupportIndices{1U, 0U, 3U, 2U};

std::uint32_t decorativeRockClusterHash(int x, int z) {
    std::uint32_t value =
        static_cast<std::uint32_t>(x) * 0x8da6b343U ^
        static_cast<std::uint32_t>(z) * 0xd8163841U ^
        0x27d4eb2fU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

float decorativeRockClusterDensity(float x, float z) {
    const float sampleX = x / DecorativeRockClusterSize;
    const float sampleZ = z / DecorativeRockClusterSize;
    const int cellX = static_cast<int>(std::floor(sampleX));
    const int cellZ = static_cast<int>(std::floor(sampleZ));
    float blendX = sampleX - static_cast<float>(cellX);
    float blendZ = sampleZ - static_cast<float>(cellZ);
    blendX = blendX * blendX * (3.0F - 2.0F * blendX);
    blendZ = blendZ * blendZ * (3.0F - 2.0F * blendZ);
    const auto sample = [](int sampleCellX, int sampleCellZ) {
        return static_cast<float>(
                   decorativeRockClusterHash(
                       sampleCellX, sampleCellZ) &
                   0xffffU) /
               65535.0F;
    };
    const float lower = sample(cellX, cellZ) +
        (sample(cellX + 1, cellZ) - sample(cellX, cellZ)) *
            blendX;
    const float upper = sample(cellX, cellZ + 1) +
        (sample(cellX + 1, cellZ + 1) -
         sample(cellX, cellZ + 1)) *
            blendX;
    const float cluster = lower + (upper - lower) * blendZ;
    return std::clamp(
        0.04F + (cluster - 0.40F) * 1.9F,
        0.04F, 0.78F);
}

constexpr float GroundRockSpacing = 5.0F;
constexpr float GroundBushSpacing = 5.8F;
constexpr float GroundDecorCoreClearRadius = 10.5F;
constexpr std::size_t GroundBushVariantCount = 6U;
constexpr std::array<float, GroundBushVariantCount>
    GroundBushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
    };
// Unscaled horizontal radii from the source GLBs. These are used for both
// overlap rejection and grass clearance, so visual variants stay consistent.
constexpr std::array<float, GroundBushVariantCount>
    GroundBushSourceRadii{
        0.36F, 1.55F, 0.79F, 1.14F, 0.78F, 0.60F,
    };

struct GroundDecorPlacement {
    Vector2 position{};
    std::size_t variant{};
    float scale{1.0F};
};

std::uint32_t groundDecorCellHash(int x, int z) {
    std::uint32_t value =
        static_cast<std::uint32_t>(x) * 0x8da6b343U ^
        static_cast<std::uint32_t>(z) * 0xd8163841U ^
        0x6c8e9cf5U;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

float groundDecorUnit(std::uint32_t value) {
    return static_cast<float>(value & 0xffffU) / 65535.0F;
}

std::optional<GroundDecorPlacement> groundRockPlacement(
    int cellX, int cellZ, float worldLimit) {
    const std::uint32_t hash = groundDecorCellHash(cellX, cellZ);
    const float x =
        (static_cast<float>(cellX) + 0.5F) * GroundRockSpacing +
        (groundDecorUnit(hash >> 7U) - 0.5F) *
            GroundRockSpacing * 0.78F;
    const float z =
        (static_cast<float>(cellZ) + 0.5F) * GroundRockSpacing +
        (groundDecorUnit(hash >> 15U) - 0.5F) *
            GroundRockSpacing * 0.78F;
    if (groundDecorUnit(hash) >
            decorativeRockClusterDensity(x, z) ||
        std::abs(x) > worldLimit - 0.7F ||
        std::abs(z) > worldLimit - 0.7F ||
        x * x + z * z <
            GroundDecorCoreClearRadius * GroundDecorCoreClearRadius) {
        return std::nullopt;
    }
    return GroundDecorPlacement{
        .position = {x, z},
        .variant = static_cast<std::size_t>((hash >> 4U) % 4U),
        .scale =
            0.55F + groundDecorUnit(hash ^ 0xa511e9b3U) * 0.45F,
    };
}

std::optional<GroundDecorPlacement> groundBushPlacementRaw(
    int cellX, int cellZ, float worldLimit) {
    const std::uint32_t hash =
        std::rotl(groundDecorCellHash(cellX, cellZ), 13) ^
        0xb5297a4dU;
    const float x =
        (static_cast<float>(cellX) + 0.5F) * GroundBushSpacing +
        (groundDecorUnit(hash >> 6U) - 0.5F) *
            GroundBushSpacing * 0.76F;
    const float z =
        (static_cast<float>(cellZ) + 0.5F) * GroundBushSpacing +
        (groundDecorUnit(hash >> 14U) - 0.5F) *
            GroundBushSpacing * 0.76F;
    const float clusterDensity =
        decorativeRockClusterDensity(x + 31.0F, z - 23.0F) * 0.72F;
    if (groundDecorUnit(hash) > clusterDensity ||
        std::abs(x) > worldLimit - 0.9F ||
        std::abs(z) > worldLimit - 0.9F ||
        x * x + z * z <
            GroundDecorCoreClearRadius * GroundDecorCoreClearRadius) {
        return std::nullopt;
    }
    const std::size_t variant = static_cast<std::size_t>(
        (hash >> 4U) % GroundBushVariantCount);
    return GroundDecorPlacement{
        .position = {x, z},
        .variant = variant,
        .scale = GroundBushVariantScales[variant] *
            (0.82F +
             groundDecorUnit(hash ^ 0x68e31da4U) * 0.43F),
    };
}

bool groundBushOverlapsRock(
    const GroundDecorPlacement& bush, float worldLimit) {
    const float bushRadius =
        GroundBushSourceRadii[bush.variant] * bush.scale;
    const int centerCellX = static_cast<int>(
        std::floor(bush.position.x / GroundRockSpacing));
    const int centerCellZ = static_cast<int>(
        std::floor(bush.position.y / GroundRockSpacing));
    for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto rock = groundRockPlacement(
                centerCellX + offsetX,
                centerCellZ + offsetZ,
                worldLimit);
            if (!rock) {
                continue;
            }
            const float rockRadius = 0.58F * rock->scale;
            const float deltaX =
                bush.position.x - rock->position.x;
            const float deltaZ =
                bush.position.y - rock->position.y;
            const float clearance = bushRadius + rockRadius + 0.18F;
            if (deltaX * deltaX + deltaZ * deltaZ <
                clearance * clearance) {
                return true;
            }
        }
    }
    return false;
}

std::optional<GroundDecorPlacement> groundBushPlacement(
    int cellX, int cellZ, float worldLimit) {
    const auto bush = groundBushPlacementRaw(
        cellX, cellZ, worldLimit);
    if (!bush || groundBushOverlapsRock(*bush, worldLimit)) {
        return std::nullopt;
    }
    return bush;
}

float groundDecorClearance(
    Vector2 position, Vector2 propPosition,
    float radius, float feather) {
    const float deltaX = position.x - propPosition.x;
    const float deltaZ = position.y - propPosition.y;
    const float distance = std::sqrt(
        deltaX * deltaX + deltaZ * deltaZ);
    const float amount = std::clamp(
        (distance - radius) / std::max(feather, 0.001F),
        0.0F, 1.0F);
    return amount * amount * (3.0F - 2.0F * amount);
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

std::optional<double> modelColliderRaycastDistance(
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
        const auto distance = type == ModelColliderType::Cylinder
                                  ? rayCylinderDistance(
                                        ray, worldBounds, maxDistance)
                                  : accept(GetRayCollisionBox(
                                        ray, worldBounds));
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

    if (modelVisual == EnemyModelVisual::Sapper) {
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

} // namespace

float Renderer::decorativePropVisibility(
    Vector2 position, float worldLimit) {
    float visibility = 1.0F;
    const int rockCellX = static_cast<int>(
        std::floor(position.x / GroundRockSpacing));
    const int rockCellZ = static_cast<int>(
        std::floor(position.y / GroundRockSpacing));
    for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto rock = groundRockPlacement(
                rockCellX + offsetX,
                rockCellZ + offsetZ,
                worldLimit);
            if (!rock) {
                continue;
            }
            visibility = std::min(
                visibility,
                groundDecorClearance(
                    position, rock->position,
                    0.58F * rock->scale + 0.08F,
                    0.32F));
        }
    }

    const int bushCellX = static_cast<int>(
        std::floor(position.x / GroundBushSpacing));
    const int bushCellZ = static_cast<int>(
        std::floor(position.y / GroundBushSpacing));
    for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto bush = groundBushPlacement(
                bushCellX + offsetX,
                bushCellZ + offsetZ,
                worldLimit);
            if (!bush) {
                continue;
            }
            visibility = std::min(
                visibility,
                groundDecorClearance(
                    position, bush->position,
                    GroundBushSourceRadii[bush->variant] *
                            bush->scale +
                        0.10F,
                    0.38F));
        }
    }
    return visibility;
}

bool Renderer::drawFirstPersonTool(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning) {
    ModelResource& resource =
        visual == FirstPersonToolVisual::Axe
            ? resources_.axeModel()
            : resources_.pickaxeModel();
    if (!resource.valid()) {
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
    Model& model = resource.get();
    rlPushMatrix();
    rlTranslatef(tuning.position.x + bobX,
                 tuning.position.y - bobY,
                 tuning.position.z + swingPush);
    // Swing in camera space. Applying this before the model's local pose
    // keeps the strike aimed forward even when the tool is yawed in-hand.
    rlRotatef(swingPitch, 1.0F, 0.0F, 0.0F);
    // Blender exports the handle along local +Y. With the origin at the
    // grip, these rotations move the head without making the hand slide.
    rlRotatef(tuning.rotation.x, 1.0F, 0.0F, 0.0F);
    rlRotatef(tuning.rotation.y, 0.0F, 1.0F, 0.0F);
    rlRotatef(tuning.rotation.z, 0.0F, 0.0F, 1.0F);
    rlScalef(tuning.scale, tuning.scale, tuning.scale);
    DrawModel(model, {}, 1.0F, WHITE);
    rlPopMatrix();
    return true;
}

std::optional<double> Renderer::buildingRaycastDistance(
    const BuildingInstance& building,
    std::span<const BuildingInstance> buildings, Ray ray,
    double maxDistance, float defensiveYaw,
    float cannonPitchRadians) {
    const Vec3 worldCenter =
        buildingWorldPosition(building);
    Vector3 position{
        static_cast<float>(worldCenter.x),
        static_cast<float>(worldCenter.y),
        static_cast<float>(worldCenter.z)};
    ModelResource* resource = nullptr;
    float modelScale = 1.0F;
    float yaw = 0.0F;
    float groundOffset = 0.0F;
    static_cast<void>(cannonPitchRadians);

    constexpr float QuarterTurn = PI * 0.5F;
    switch (building.type) {
    case BuildingType::Core:
        resource = &resources_.coreModel();
        modelScale = 2.0F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::Wall: {
        constexpr std::uint8_t AllConnections =
            WallConnectionNorth | WallConnectionEast |
            WallConnectionSouth | WallConnectionWest;
        const std::uint8_t connectionMask =
            wallConnectionMask(
                buildings, building.gridPosition,
                building.baseHeight) &
            AllConnections;
        const int connectionCount =
            std::popcount(connectionMask);
        if (connectionCount == 0) {
            resource = &resources_.wallIsolatedModel();
            yaw = static_cast<float>(
                      wallFallbackRotation(
                          buildings, building)) *
                  QuarterTurn;
        } else if (connectionCount == 1) {
            resource = &resources_.wallEndModel();
            if ((connectionMask &
                 WallConnectionNorth) != 0U) {
                yaw = PI * 0.5F;
            } else if (
                (connectionMask &
                 WallConnectionWest) != 0U) {
                yaw = PI;
            } else if (
                (connectionMask &
                 WallConnectionSouth) != 0U) {
                yaw = -PI * 0.5F;
            }
        } else if (connectionCount == 2) {
            const bool northSouth =
                connectionMask ==
                (WallConnectionNorth |
                 WallConnectionSouth);
            const bool eastWest =
                connectionMask ==
                (WallConnectionEast |
                 WallConnectionWest);
            if (northSouth || eastWest) {
                resource =
                    &resources_.wallIsolatedModel();
                yaw = northSouth ? PI * 0.5F : 0.0F;
            } else {
                resource = &resources_.wallCornerModel();
                if (connectionMask ==
                    (WallConnectionNorth |
                     WallConnectionEast)) {
                    yaw = -PI * 0.5F;
                } else if (
                    connectionMask ==
                    (WallConnectionEast |
                     WallConnectionSouth)) {
                    yaw = PI;
                } else if (
                    connectionMask ==
                    (WallConnectionSouth |
                     WallConnectionWest)) {
                    yaw = PI * 0.5F;
                }
            }
        } else if (connectionCount == 3) {
            resource = &resources_.wallTModel();
            const std::uint8_t missing =
                AllConnections ^ connectionMask;
            if (missing == WallConnectionEast) {
                yaw = PI * 0.5F;
            } else if (missing ==
                       WallConnectionNorth) {
                yaw = PI;
            } else if (missing ==
                       WallConnectionWest) {
                yaw = -PI * 0.5F;
            }
        } else {
            resource = &resources_.wallCrossModel();
        }
        groundOffset = 0.005F;
        break;
    }
    case BuildingType::Turret:
        resource = &resources_.crossbowModel();
        modelScale = 2.5F;
        yaw = defensiveYaw + PI;
        groundOffset = 0.13F;
        break;
    case BuildingType::GoldMine:
        resource = &resources_.mineModel();
        modelScale = 2.1F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::LumberMill:
        resource = &resources_.lumberMillModel();
        modelScale = 1.65F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::Quarry:
        resource = &resources_.quarryModel();
        modelScale = 1.3F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::Cannon:
        resource = &resources_.cannonModel();
        modelScale = 3.0F;
        yaw = defensiveYaw + PI;
        groundOffset = 0.155F;
        break;
    case BuildingType::SlowTrap:
    case BuildingType::Gate:
        break;
    }

    const auto acceptCollision =
        [maxDistance](RayCollision collision)
            -> std::optional<double> {
            if (!collision.hit || collision.distance < 0.0F ||
                static_cast<double>(collision.distance) > maxDistance) {
                return std::nullopt;
            }
            return static_cast<double>(collision.distance);
        };
    if (resource != nullptr && resource->valid()) {
        Model& model = resource->get();
        position.y += groundOffset;
        const Matrix scale = MatrixScale(
            modelScale, modelScale, modelScale);
        const Matrix rotation = MatrixRotateY(yaw);
        const Matrix translation = MatrixTranslate(
            position.x, position.y, position.z);
        const Matrix baseTransform = MatrixMultiply(
            model.transform,
            MatrixMultiply(
                MatrixMultiply(scale, rotation),
                translation));
        return modelColliderRaycastDistance(
            *resource, baseTransform, ray, maxDistance);
    }

    const auto collideBox =
        [&ray, &acceptCollision](
            Vector3 center, Vector3 size) {
            const Vector3 half{
                size.x * 0.5F, size.y * 0.5F,
                size.z * 0.5F};
            return acceptCollision(GetRayCollisionBox(
                ray,
                {{center.x - half.x,
                  center.y - half.y,
                  center.z - half.z},
                 {center.x + half.x,
                  center.y + half.y,
                  center.z + half.z}}));
        };
    std::optional<double> closest;
    const auto addBox =
        [&closest, &collideBox](
            Vector3 center, Vector3 size) {
            const auto distance = collideBox(center, size);
            if (distance &&
                (!closest || *distance < *closest)) {
                closest = distance;
            }
        };
    if (building.type == BuildingType::SlowTrap) {
        addBox(
            {position.x, 0.08F, position.z},
            {1.0F, 0.16F, 1.0F});
    } else if (building.type == BuildingType::Gate) {
        if ((building.rotation % 2U) == 0U) {
            addBox(
                {position.x - 0.38F, 1.0F, position.z},
                {0.22F, 2.0F, 1.0F});
            addBox(
                {position.x + 0.38F, 1.0F, position.z},
                {0.22F, 2.0F, 1.0F});
            if (!building.open) {
                addBox(
                    {position.x, 1.0F, position.z},
                    {0.55F, 1.7F, 0.18F});
            }
        } else {
            addBox(
                {position.x, 1.0F,
                 position.z - 0.38F},
                {1.0F, 2.0F, 0.22F});
            addBox(
                {position.x, 1.0F,
                 position.z + 0.38F},
                {1.0F, 2.0F, 0.22F});
            if (!building.open) {
                addBox(
                    {position.x, 1.0F, position.z},
                    {0.18F, 1.7F, 0.55F});
            }
        }
    } else {
        Vector3 size{1.6F, 1.8F, 1.6F};
        if (building.type == BuildingType::Core) {
            size = {1.8F, 2.6F, 1.8F};
        } else if (
            building.type == BuildingType::Wall) {
            size = {0.84F, 2.05F, 0.84F};
        } else if (
            building.type == BuildingType::GoldMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            size = {1.8F, 1.6F, 1.8F};
        }
        addBox(
            {position.x, size.y * 0.5F, position.z},
            size);
    }
    return closest;
}

std::optional<double> Renderer::resourceRaycastDistance(
    ResourceType type, Vector3 position,
    Ray ray, double maxDistance,
    std::size_t visualVariant,
    float visualScale, float yawRadians) {
    ModelResource& resource = type == ResourceType::Wood
                                  ? resources_.treeModel(visualVariant)
                                  : resources_.rockModel();
    if (!resource.valid() || maxDistance <= 0.0) {
        return std::nullopt;
    }
    const float modelScale = type == ResourceType::Wood
                                 ? TreeModelScale * visualScale
                                 : RockModelScale;
    position.y += type == ResourceType::Wood
                      ? static_cast<float>(TreeVisualGroundOffsets[
                            visualVariant % TreeVisualVariantCount] *
                            visualScale)
                      : RockGroundOffset;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    return modelColliderRaycastDistance(
        resource, transform, ray, maxDistance);
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

    constexpr float ModelScale = 3.0F;
    constexpr float GroundOffset = 0.155F;
    constexpr float ModelForwardOffset = PI;
    position.y += GroundOffset;

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    (yawRadians + ModelForwardOffset) * RAD2DEG,
                    {ModelScale * scaleFactor,
                     ModelScale * scaleFactor,
                     ModelScale * scaleFactor},
                    tint);
        return true;
    }

    const Matrix scale = MatrixScale(
        ModelScale * scaleFactor, ModelScale * scaleFactor,
        ModelScale * scaleFactor);
    const Matrix yaw =
        MatrixRotateY(yawRadians + ModelForwardOffset);
    const Matrix pitch = MatrixRotateX(pitchRadians);
    const Matrix translation =
        MatrixTranslate(position.x, position.y, position.z);
    const Matrix baseTransform =
        MatrixMultiply(MatrixMultiply(scale, yaw), translation);
    const Matrix barrelTransform = MatrixMultiply(
        MatrixMultiply(MatrixMultiply(scale, pitch), yaw), translation);

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

    // glTF order: barrel.002 (0), weapon-cannon/base (1).
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
                {1.5F, 1.5F, 1.5F}, tint);
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
    const Vector3 referenceUp =
        std::abs(Vector3DotProduct(forward, {0.0F, 1.0F, 0.0F})) > 0.98F
            ? Vector3{1.0F, 0.0F, 0.0F}
            : Vector3{0.0F, 1.0F, 0.0F};
    const Vector3 right =
        Vector3Normalize(Vector3CrossProduct(referenceUp, forward));
    const Vector3 up = Vector3CrossProduct(forward, right);
    Matrix rotation = MatrixIdentity();
    rotation.m0 = right.x;
    rotation.m1 = right.y;
    rotation.m2 = right.z;
    rotation.m4 = up.x;
    rotation.m5 = up.y;
    rotation.m6 = up.z;
    rotation.m8 = forward.x;
    rotation.m9 = forward.y;
    rotation.m10 = forward.z;

    constexpr float ArrowScale = 1.2F;
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(ArrowScale, ArrowScale, ArrowScale), rotation),
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
                            Color tint, float scale) {
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
    constexpr float ModelScale = 2.5F;
    constexpr float GroundOffset = 0.13F;
    constexpr float ModelForwardOffset = PI;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                (yawRadians + ModelForwardOffset) * RAD2DEG,
                {ModelScale * scale, ModelScale * scale,
                 ModelScale * scale},
                tint);
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

bool Renderer::drawMine(Vector3 position, float yawRadians,
                        Color tint, float scale) {
    return drawResourceProducer(
        BuildingType::GoldMine, position, yawRadians,
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
    if (type == BuildingType::GoldMine) {
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
        {modelScale * scale, modelScale * scale,
         modelScale * scale},
        tint);
    return true;
}

bool Renderer::drawRock(Vector3 position, Color tint,
                        float scale) {
    auto& resource = resources_.rockModel();
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
    position.y += RockGroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {RockModelScale * scale, RockModelScale * scale,
                 RockModelScale * scale},
                tint);
    return true;
}

void Renderer::drawDecorativeRocks(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    constexpr std::size_t VariantCount = 4U;
    constexpr float Spacing = 5.0F;
    constexpr float DrawRadius = 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    const int minimumX = static_cast<int>(
        std::floor((cameraPosition.x - DrawRadius) / Spacing));
    const int maximumX = static_cast<int>(
        std::ceil((cameraPosition.x + DrawRadius) / Spacing));
    const int minimumZ = static_cast<int>(
        std::floor((cameraPosition.z - DrawRadius) / Spacing));
    const int maximumZ = static_cast<int>(
        std::ceil((cameraPosition.z + DrawRadius) / Spacing));
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0x6c8e9cf5U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) / 65535.0F;
    };

    Shader* decorativeShader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        decorativeShader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ &&
               resources_.worldShader().valid()) {
        decorativeShader = &resources_.worldShader().get();
    }
    for (std::size_t variant = 0; variant < VariantCount; ++variant) {
        ModelResource& resource =
            resources_.decorativeRockModel(variant);
        if (!resource.valid()) {
            continue;
        }
        if (decorativeShader != nullptr) {
            Model& model = resource.get();
            for (int material = 0; material < model.materialCount;
                 ++material) {
                model.materials[material].shader = *decorativeShader;
            }
        }
    }
    constexpr std::size_t BushVariantCount = 6U;
    for (std::size_t variant = 0;
         variant < BushVariantCount; ++variant) {
        ModelResource& resource =
            resources_.decorativeBushModel(variant);
        if (!resource.valid()) {
            continue;
        }
        if (decorativeShader != nullptr) {
            Model& model = resource.get();
            for (int material = 0; material < model.materialCount;
                 ++material) {
                model.materials[material].shader = *decorativeShader;
            }
        }
    }

    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            const std::uint32_t hash = hashCell(cellX, cellZ);
            const float jitterX =
                (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.78F;
            const float jitterZ =
                (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.78F;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * Spacing +
                jitterX;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * Spacing +
                jitterZ;
            if (unitFloat(hash) >
                decorativeRockClusterDensity(x, z)) {
                continue;
            }
            if (std::abs(x) > worldLimit - 0.7F ||
                std::abs(z) > worldLimit - 0.7F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                DrawRadius * DrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 1.8F, 0.65F);
            if (visibility <= 0.01F) {
                continue;
            }
            const std::size_t variant =
                static_cast<std::size_t>((hash >> 4U) % VariantCount);
            ModelResource& resource =
                resources_.decorativeRockModel(variant);
            if (!resource.valid()) {
                continue;
            }
            const float baseScale =
                0.55F + unitFloat(hash ^ 0xa511e9b3U) * 0.45F;
            const float revealScale =
                worldRevealScaleAt({x, z});
            if (revealScale <= 0.001F) {
                continue;
            }
            const float scale =
                baseScale * visibility * revealScale;
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x63d83595U) * PI * 2.0F;
            DrawModelEx(
                resource.get(),
                {x,
                 terrainHeight -
                     (1.0F - visibility) * 0.08F,
                 z},
                {0.0F, 1.0F, 0.0F}, rotation * RAD2DEG,
                {scale, scale, scale}, WHITE);
        }
    }

    constexpr float BushSpacing = 5.8F;
    constexpr float BushDrawRadius = 36.0F;
    constexpr std::array<float, BushVariantCount> BushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
    };
    const int minimumBushX = static_cast<int>(std::floor(
        (cameraPosition.x - BushDrawRadius) / BushSpacing));
    const int maximumBushX = static_cast<int>(std::ceil(
        (cameraPosition.x + BushDrawRadius) / BushSpacing));
    const int minimumBushZ = static_cast<int>(std::floor(
        (cameraPosition.z - BushDrawRadius) / BushSpacing));
    const int maximumBushZ = static_cast<int>(std::ceil(
        (cameraPosition.z + BushDrawRadius) / BushSpacing));
    for (int cellZ = minimumBushZ;
         cellZ <= maximumBushZ; ++cellZ) {
        for (int cellX = minimumBushX;
             cellX <= maximumBushX; ++cellX) {
            const std::uint32_t hash =
                std::rotl(hashCell(cellX, cellZ), 13) ^ 0xb5297a4dU;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 6U) - 0.5F) * BushSpacing * 0.76F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 14U) - 0.5F) * BushSpacing * 0.76F;
            const float clusterDensity =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F) *
                0.72F;
            if (unitFloat(hash) > clusterDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                BushDrawRadius * BushDrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 2.0F, 0.72F);
            if (visibility <= 0.01F) {
                continue;
            }
            const std::size_t variant = static_cast<std::size_t>(
                (hash >> 4U) % BushVariantCount);
            ModelResource& resource =
                resources_.decorativeBushModel(variant);
            if (!resource.valid()) {
                continue;
            }
            const float baseScale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F);
            if (groundBushOverlapsRock(
                    {{x, z}, variant, baseScale}, worldLimit)) {
                continue;
            }
            const float revealScale = worldRevealScaleAt({x, z});
            const float scale =
                baseScale * visibility * revealScale;
            if (scale <= 0.001F) {
                continue;
            }
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x1b56c4e9U) * 360.0F;
            DrawModelEx(
                resource.get(),
                {x,
                 terrainHeight -
                     (1.0F - visibility) * 0.10F,
                 z},
                {0.0F, 1.0F, 0.0F}, rotation,
                {scale, scale, scale}, WHITE);
        }
    }
}

void Renderer::drawDecorativeRockAo(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    if (!blobShadowBatchOpen_ ||
        terrainHeightfield_ == nullptr) {
        return;
    }
    constexpr float Spacing = 5.0F;
    constexpr float DrawRadius = 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    const int minimumX = static_cast<int>(
        std::floor((cameraPosition.x - DrawRadius) / Spacing));
    const int maximumX = static_cast<int>(
        std::ceil((cameraPosition.x + DrawRadius) / Spacing));
    const int minimumZ = static_cast<int>(
        std::floor((cameraPosition.z - DrawRadius) / Spacing));
    const int maximumZ = static_cast<int>(
        std::ceil((cameraPosition.z + DrawRadius) / Spacing));
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0x6c8e9cf5U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) /
               65535.0F;
    };

    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            const std::uint32_t hash = hashCell(cellX, cellZ);
            const float x =
                (static_cast<float>(cellX) + 0.5F) * Spacing +
                (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.78F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * Spacing +
                (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.78F;
            if (unitFloat(hash) >
                decorativeRockClusterDensity(x, z)) {
                continue;
            }
            if (std::abs(x) > worldLimit - 0.7F ||
                std::abs(z) > worldLimit - 0.7F ||
                x * x + z * z <
                    CoreClearRadius * CoreClearRadius) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                DrawRadius * DrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 1.8F, 0.65F);
            if (visibility <= 0.01F) {
                continue;
            }
            const float scale =
                (0.55F +
                 unitFloat(hash ^ 0xa511e9b3U) * 0.45F) *
                visibility *
                worldRevealScaleAt({x, z});
            if (scale <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                terrainHeightfield_->getHeight(x, z));
            drawBlobShadow(
                {x, groundY + 0.018F, z},
                scale * 0.62F, scale * 0.52F, 0.16F);
            drawBlobShadow(
                {x, groundY + 0.02F, z},
                scale * 0.34F, scale * 0.28F, 0.27F);
        }
    }

    constexpr std::size_t BushVariantCount = 6U;
    constexpr float BushSpacing = 5.8F;
    constexpr float BushDrawRadius = 36.0F;
    constexpr std::array<float, BushVariantCount> BushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
    };
    const int minimumBushX = static_cast<int>(std::floor(
        (cameraPosition.x - BushDrawRadius) / BushSpacing));
    const int maximumBushX = static_cast<int>(std::ceil(
        (cameraPosition.x + BushDrawRadius) / BushSpacing));
    const int minimumBushZ = static_cast<int>(std::floor(
        (cameraPosition.z - BushDrawRadius) / BushSpacing));
    const int maximumBushZ = static_cast<int>(std::ceil(
        (cameraPosition.z + BushDrawRadius) / BushSpacing));
    for (int cellZ = minimumBushZ;
         cellZ <= maximumBushZ; ++cellZ) {
        for (int cellX = minimumBushX;
             cellX <= maximumBushX; ++cellX) {
            const std::uint32_t hash =
                std::rotl(hashCell(cellX, cellZ), 13) ^ 0xb5297a4dU;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 6U) - 0.5F) * BushSpacing * 0.76F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 14U) - 0.5F) * BushSpacing * 0.76F;
            const float clusterDensity =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F) *
                0.72F;
            if (unitFloat(hash) > clusterDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                BushDrawRadius * BushDrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 2.0F, 0.72F);
            if (visibility <= 0.01F) {
                continue;
            }
            const std::size_t variant = static_cast<std::size_t>(
                (hash >> 4U) % BushVariantCount);
            const float baseScale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F);
            if (groundBushOverlapsRock(
                    {{x, z}, variant, baseScale}, worldLimit)) {
                continue;
            }
            const float scale =
                baseScale * visibility * worldRevealScaleAt({x, z});
            if (scale <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                terrainHeightfield_->getHeight(x, z));
            const float radius =
                GroundBushSourceRadii[variant] * scale;
            drawBlobShadow(
                {x, groundY + 0.018F, z},
                radius, radius * 0.82F, 0.14F);
            drawBlobShadow(
                {x, groundY + 0.02F, z},
                radius * 0.52F, radius * 0.42F, 0.22F);
        }
    }
}

void Renderer::drawBoundaryForest() {
    if (!worldShaderActive_ ||
        terrainHeightfield_ == nullptr ||
        !resources_.worldShader().valid()) {
        return;
    }
    constexpr std::size_t VariantCount = 2U;
    constexpr float Spacing = 4.0F;
    constexpr std::size_t MaximumInstancesPerVariant = 8192U;
    const auto& terrain = *terrainHeightfield_;
    const auto& config = terrain.config();
    if (config.terrainBoundaryRiseWidth <= 0.0F ||
        config.terrainBoundaryRiseHeight <= 0.0F) {
        return;
    }
    const float terrainLimit = static_cast<float>(
        config.terrainWorldSize * 0.5);
    const float forestInnerLimit = static_cast<float>(
        config.terrainWorldSize * 0.5 -
        config.terrainBoundaryRiseWidth + 0.5);
    const float forestOuterLimit = terrainLimit - 0.5F;
    const int minimumX = static_cast<int>(std::floor(
        -terrainLimit / Spacing));
    const int maximumX = static_cast<int>(std::ceil(
        terrainLimit / Spacing));
    const int minimumZ = static_cast<int>(std::floor(
        -terrainLimit / Spacing));
    const int maximumZ = static_cast<int>(std::ceil(
        terrainLimit / Spacing));
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0xa511e9b3U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) /
               65535.0F;
    };

    if (!boundaryForestCached_) {
        for (auto& variant : boundaryForestTransforms_) {
            variant.clear();
            variant.reserve(3072U);
        }
        for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
            for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                const std::uint32_t hash = hashCell(cellX, cellZ);
                const float x =
                    (static_cast<float>(cellX) + 0.5F) * Spacing +
                    (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.34F;
                const float z =
                    (static_cast<float>(cellZ) + 0.5F) * Spacing +
                    (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.34F;
                const float edgeDistance =
                    std::max(std::abs(x), std::abs(z));
                if (edgeDistance < forestInnerLimit ||
                    edgeDistance > forestOuterLimit) {
                    continue;
                }
                const std::size_t variant =
                    static_cast<std::size_t>(
                        (hash >> 4U) % VariantCount);
                if (boundaryForestTransforms_[variant].size() >=
                    MaximumInstancesPerVariant) {
                    continue;
                }
                ModelResource& resource =
                    resources_.boundaryTreeModel(variant);
                if (!resource.valid()) {
                    continue;
                }
                const float slopeProgress = std::clamp(
                    (edgeDistance - forestInnerLimit) /
                        std::max(
                            forestOuterLimit - forestInnerLimit,
                            0.001F),
                    0.0F, 1.0F);
                const float sizeRoll =
                    unitFloat(hash ^ 0x63d83595U);
                const float scale =
                    3.15F + sizeRoll * sizeRoll * 5.85F +
                    slopeProgress * 0.75F;
                const float yaw =
                    unitFloat(hash ^ 0x9e3779b9U) * PI * 2.0F;
                const float height = static_cast<float>(
                    terrain.getHeight(x, z));
                boundaryForestTransforms_[variant].push_back(
                    MatrixMultiply(
                        resource.get().transform,
                        MatrixMultiply(
                            MatrixMultiply(
                                MatrixScale(scale, scale, scale),
                                MatrixRotateY(yaw)),
                            MatrixTranslate(x, height, z))));
            }
        }
        boundaryForestCached_ = true;
    }

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        ModelResource& resource =
            resources_.boundaryTreeModel(variant);
        if (!resource.valid() ||
            boundaryForestTransforms_[variant].empty()) {
            continue;
        }
        Model& model = resource.get();
        const Matrix* transforms =
            boundaryForestTransforms_[variant].data();
        int transformCount = static_cast<int>(
            boundaryForestTransforms_[variant].size());
        if (worldRevealElapsed_ < 1.7F) {
            auto& revealTransforms =
                boundaryForestRevealTransforms_[variant];
            revealTransforms.clear();
            revealTransforms.reserve(
                boundaryForestTransforms_[variant].size());
            for (const Matrix transform :
                 boundaryForestTransforms_[variant]) {
                const float revealScale =
                    worldRevealScaleAt(
                        {transform.m12, transform.m14});
                if (revealScale <= 0.001F) {
                    continue;
                }
                revealTransforms.push_back(
                    MatrixMultiply(
                        MatrixScale(
                            revealScale,
                            revealScale,
                            revealScale),
                        transform));
            }
            transforms = revealTransforms.data();
            transformCount = static_cast<int>(
                revealTransforms.size());
            if (transformCount == 0) {
                continue;
            }
        }
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms, transformCount);
        }
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
}

bool Renderer::drawTree(Vector3 position, Color tint,
                        float scale, std::size_t visualVariant,
                        float yawRadians) {
    auto& resource = resources_.treeModel(visualVariant);
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
    position.y += static_cast<float>(
        TreeVisualGroundOffsets[
            visualVariant % TreeVisualVariantCount] *
        scale);
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                yawRadians * RAD2DEG,
                {TreeModelScale * scale, TreeModelScale * scale,
                 TreeModelScale * scale},
                tint);
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
    float yawRadians, Color tint, float scale, bool loop) {
    ModelResource* modelResource =
        enemyModelFor(resources_, modelVisual);
    if (modelResource == nullptr || !modelResource->valid()) {
        return false;
    }

    const EnemyAnimationSource animation =
        enemyAnimationFor(resources_, modelVisual, animationVisual);

    Model& model = modelResource->get();
    const ModelAnimation* clip =
        animation.animations->find(animation.clipName);
    if (clip != nullptr && clip->keyframeCount > 0 &&
        model.skeleton.boneCount > 0) {
        const float frameValue =
            std::max(0.0F, animationSeconds) * 30.0F;
        int frame = static_cast<int>(frameValue);
        frame = loop
                    ? frame % clip->keyframeCount
                    : std::min(frame, clip->keyframeCount - 1);
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
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
        setSkinningEnabled(*shader, true);
    }

    constexpr float ModelScale = 0.82F;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {ModelScale * scale, ModelScale * scale,
         ModelScale * scale},
        tint);
    if (shader != nullptr) {
        setSkinningEnabled(*shader, false);
    }
    return true;
}

bool Renderer::drawEnemiesInstanced(
    std::span<const EnemyDrawInstance> instances) {
    if (instances.empty() || shadowPassOpen_ ||
        selectionMaskPassOpen_ || !worldShaderActive_ ||
        !resources_.worldShader().valid()) {
        return false;
    }

    struct BatchKey {
        EnemyModelVisual model{};
        EnemyAnimationVisual animation{};
        int frame{};
        std::uint32_t tint{};
        int scale{};
        bool loop{};

        [[nodiscard]] auto values() const {
            return std::tie(model, animation, frame, tint, scale, loop);
        }
        [[nodiscard]] bool operator<(const BatchKey& other) const {
            return values() < other.values();
        }
    };
    struct Batch {
        EnemyDrawInstance representative{};
        std::vector<Matrix> transforms{};
    };

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
    std::map<BatchKey, Batch> batches;
    constexpr float ModelScale = 0.82F;
    for (const EnemyDrawInstance& instance : instances) {
        ModelResource* resource = modelFor(instance.modelVisual);
        if (resource == nullptr || !resource->valid()) {
            return false;
        }
        const EnemyAnimationSource animation =
            animationFor(instance.modelVisual,
                         instance.animationVisual);
        const ModelAnimation* clip =
            animation.animations->find(animation.clipName);
        int frame = 0;
        if (clip != nullptr && clip->keyframeCount > 0) {
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
        const BatchKey key{
            instance.modelVisual, instance.animationVisual, frame,
            tint, static_cast<int>(std::lround(instance.scale * 1000.0F)),
            instance.loop};
        Batch& batch = batches[key];
        if (batch.transforms.empty()) {
            batch.representative = instance;
            batch.representative.animationSeconds =
                static_cast<float>(frame) /
                SourceAnimationFps;
        }
        const float uniformScale = ModelScale * instance.scale;
        const Matrix transform = MatrixMultiply(
            resource->get().transform,
            MatrixMultiply(
                MatrixMultiply(
                    MatrixScale(
                        uniformScale, uniformScale, uniformScale),
                    MatrixRotateY(instance.yawRadians)),
                MatrixTranslate(
                    instance.position.x, instance.position.y,
                    instance.position.z)));
        batch.transforms.push_back(transform);
    }

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, true);

    for (auto& [key, batch] : batches) {
        (void)key;
        const EnemyDrawInstance& representative =
            batch.representative;
        ModelResource* resource =
            modelFor(representative.modelVisual);
        Model& model = resource->get();
        const EnemyAnimationSource animation =
            animationFor(representative.modelVisual,
                         representative.animationVisual);
        const ModelAnimation* clip =
            animation.animations->find(animation.clipName);
        if (clip != nullptr && clip->keyframeCount > 0 &&
            model.skeleton.boneCount > 0) {
            const int frame = std::min(
                static_cast<int>(
                    representative.animationSeconds * 30.0F),
                clip->keyframeCount - 1);
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
        }

        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                representative.tint;
            if (model.boneMatrices != nullptr &&
                model.skeleton.boneCount > 0 &&
                shader.locs[
                    SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0) {
                rlEnableShader(shader.id);
                rlSetUniformMatrices(
                    shader.locs[
                        SHADER_LOC_MATRIX_BONETRANSFORMS],
                    model.boneMatrices,
                    model.skeleton.boneCount);
                rlDisableShader();
            }
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                batch.transforms.data(),
                static_cast<int>(batch.transforms.size()));
        }
    }

    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    return true;
}

} // namespace ian
