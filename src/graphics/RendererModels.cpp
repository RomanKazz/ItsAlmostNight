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
    float shaped = std::clamp(
        (cluster - 0.34F) / 0.46F, 0.0F, 1.0F);
    shaped = shaped * shaped * (3.0F - 2.0F * shaped);
    // Large quiet gaps, soft outskirts, then a genuinely dense core.
    return std::clamp(
        0.012F + shaped * 0.10F + shaped * shaped * 0.78F,
        0.012F, 0.89F);
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

    if (modelVisual == EnemyModelVisual::Minion) {
        animations = &resources.enemyPinkBlobAnimations();
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

void drawLootItemGeometry(
    LootUpgradeEffect effect, Color color) {
    if (effect == LootUpgradeEffect::Damage) {
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCube({}, 0.32F, 0.32F, 0.32F, color);
    } else if (effect == LootUpgradeEffect::MoveSpeed) {
        DrawCube({-0.10F, 0.0F, 0.0F}, 0.12F, 0.48F, 0.18F, color);
        DrawCube({0.10F, 0.08F, 0.0F}, 0.12F, 0.38F, 0.18F, color);
    } else if (effect == LootUpgradeEffect::IronBar) {
        DrawCube({}, 0.48F, 0.18F, 0.20F, color);
    } else if (effect == LootUpgradeEffect::FuelJerrycan) {
        DrawCube({}, 0.30F, 0.46F, 0.18F, color);
        DrawCube({0.0F, 0.28F, 0.0F}, 0.14F, 0.10F, 0.14F, color);
    } else if (effect == LootUpgradeEffect::Compass) {
        DrawCylinder({}, 0.22F, 0.22F, 0.10F, 12, color);
        DrawCylinder({0.0F, 0.08F, 0.0F}, 0.04F, 0.04F, 0.22F, 6,
                     WHITE);
    } else if (effect == LootUpgradeEffect::Nail) {
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCylinder({}, 0.045F, 0.045F, 0.40F, 8, color);
        DrawCylinder({0.0F, 0.22F, 0.0F}, 0.10F, 0.10F, 0.06F, 8,
                     color);
    } else if (effect == LootUpgradeEffect::Key) {
        DrawCylinder({}, 0.14F, 0.14F, 0.06F, 12, color);
        DrawCylinder({0.0F, 0.0F, 0.0F}, 0.08F, 0.08F, 0.08F, 12,
                     {24, 28, 31, 255});
        DrawCube({0.18F, 0.0F, 0.0F}, 0.28F, 0.06F, 0.06F, color);
    } else if (effect == LootUpgradeEffect::Map) {
        DrawCube({}, 0.46F, 0.28F, 0.08F, color);
        DrawLine3D({-0.08F, -0.14F, -0.05F},
                   {-0.08F, 0.14F, -0.05F}, WHITE);
    } else if (effect == LootUpgradeEffect::Anvil) {
        DrawCube({0.0F, 0.08F, 0.0F}, 0.48F, 0.16F, 0.20F, color);
        DrawCube({0.0F, -0.12F, 0.0F}, 0.20F, 0.28F, 0.16F, color);
    } else if (effect == LootUpgradeEffect::Saw) {
        DrawCube({}, 0.50F, 0.08F, 0.10F, color);
        for (int tooth = 0; tooth < 4; ++tooth) {
            DrawCube({-0.15F + tooth * 0.10F, -0.08F, 0.0F},
                     0.06F, 0.10F, 0.10F, color);
        }
    } else if (effect == LootUpgradeEffect::Potion) {
        DrawCylinder({0.0F, -0.08F, 0.0F}, 0.14F, 0.18F, 0.30F, 10,
                     color);
        DrawCylinder({0.0F, 0.14F, 0.0F}, 0.08F, 0.08F, 0.10F, 8,
                     color);
    } else {
        DrawSphereEx({-0.11F, 0.08F, 0.0F}, 0.18F, 6, 6, color);
        DrawSphereEx({0.11F, 0.08F, 0.0F}, 0.18F, 6, 6, color);
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCube({0.0F, -0.10F, 0.0F}, 0.25F, 0.25F, 0.25F, color);
    }
}

ModelResource* lootItemModelFor(
    GraphicsResources& resources, LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Apple:
        return &resources.appleLootModel();
    case LootUpgradeEffect::Bread:
        return &resources.breadLootModel();
    case LootUpgradeEffect::IronBar:
        return &resources.ironBarLootModel();
    case LootUpgradeEffect::FuelJerrycan:
        return &resources.fuelJerrycanLootModel();
    case LootUpgradeEffect::Compass:
        return &resources.compassLootModel();
    case LootUpgradeEffect::Nail:
        return &resources.nailLootModel();
    case LootUpgradeEffect::Key:
        return &resources.keyLootModel();
    case LootUpgradeEffect::Map:
        return &resources.mapLootModel();
    case LootUpgradeEffect::Anvil:
        return &resources.anvilLootModel();
    case LootUpgradeEffect::Saw:
        return &resources.sawLootModel();
    case LootUpgradeEffect::Potion:
        return &resources.potionLootModel();
    case LootUpgradeEffect::Damage:
    case LootUpgradeEffect::MoveSpeed:
    case LootUpgradeEffect::MaximumHealth:
        return nullptr;
    }
    return nullptr;
}

struct LootModelFit {
    Vector3 center{};
    float scale{1.0F};
};

LootModelFit lootModelFit(const Model& model) {
    const BoundingBox bounds = GetModelBoundingBox(model);
    const Vector3 extent{
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z,
    };
    const float maximumExtent = std::max({extent.x, extent.y, extent.z});
    constexpr float TargetMaximumExtent = 0.46F;
    return {
        .center = {
            (bounds.min.x + bounds.max.x) * 0.5F,
            (bounds.min.y + bounds.max.y) * 0.5F,
            (bounds.min.z + bounds.max.z) * 0.5F,
        },
        .scale = maximumExtent > 0.0001F
            ? TargetMaximumExtent / maximumExtent
            : 1.0F,
    };
}

void drawFittedLootModel(Model& model, Color tint) {
    const LootModelFit fit = lootModelFit(model);
    rlPushMatrix();
    rlScalef(fit.scale, fit.scale, fit.scale);
    rlTranslatef(-fit.center.x, -fit.center.y, -fit.center.z);
    DrawModel(model, {}, 1.0F, tint);
    rlPopMatrix();
}

void drawFittedLootOutlineModel(Model& model, Color color) {
    const LootModelFit fit = lootModelFit(model);
    const Texture2D whiteTexture{
        .id = rlGetTextureIdDefault(),
        .width = 1,
        .height = 1,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    rlPushMatrix();
    rlScalef(fit.scale, fit.scale, fit.scale);
    rlTranslatef(-fit.center.x, -fit.center.y, -fit.center.z);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        Material& material =
            model.materials[model.meshMaterial[meshIndex]];
        const Texture2D originalTexture =
            material.maps[MATERIAL_MAP_DIFFUSE].texture;
        const Color originalColor =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTexture;
        material.maps[MATERIAL_MAP_DIFFUSE].color = color;
        DrawMesh(model.meshes[meshIndex], material, model.transform);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = originalTexture;
        material.maps[MATERIAL_MAP_DIFFUSE].color = originalColor;
    }
    rlPopMatrix();
}

void applyLootItemLocalRotation(LootUpgradeEffect effect) {
    if (effect == LootUpgradeEffect::Bread) {
        rlRotatef(90.0F, 1.0F, 0.0F, 0.0F);
        rlRotatef(90.0F, 0.0F, 0.0F, 1.0F);
    }
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
    }
    return tint;
}

} // namespace

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
    // The three tiers intentionally read as blue -> gold -> red in-world:
    // common, rare (Uncommon in the data model), legendary (Rare).
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
        resource = &resources_.iceWandModel();
        break;
    case FirstPersonToolVisual::Hammer:
        resource = &resources_.hammerModel();
        break;
    }
    if (resource == nullptr || !resource->valid()) {
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
    Model& model = resource->get();
    rlPushMatrix();
    const float wandX = iceWand
        ? tuning.position.x + 0.035F + bobX
        : tuning.position.x + bobX;
    const float wandY = iceWand
        ? tuning.position.y - bobY + charge * 0.045F
        : tuning.position.y - bobY;
    const float wandZ = iceWand
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
    const float modelScale = iceWand
        ? tuning.scale * 1.0F
        : tuning.scale;
    rlScalef(modelScale, modelScale, modelScale);
    DrawModel(model, {}, 1.0F, WHITE);
    if (iceWand) {
        constexpr float CrystalHeight = 0.365F;
        const float crystalPulse = 0.42F + charge * 0.28F +
            0.035F * std::sin(static_cast<float>(GetTime()) * 5.0F);
        drawIceMagicSphere(
            {0.0F, CrystalHeight, 0.0F},
            0.052F + charge * 0.012F,
            static_cast<float>(GetTime()), crystalPulse,
            {142, 229, 255, 145});
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
                    {142, 229, 255, 135});
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
                {52, 175, 255, static_cast<unsigned char>(145.0F * fade)},
                {24, 105, 235, static_cast<unsigned char>(120.0F * nextFade)});
            emitRibbon(
                radius * (0.28F * fade + 0.018F),
                radius * (0.28F * nextFade + 0.008F),
                {223, 248, 255, static_cast<unsigned char>(235.0F * fade)},
                {142, 229, 255, static_cast<unsigned char>(205.0F * nextFade)});
        }
        rlEnd();
        EndBlendMode();
    }

    const float pulse = 0.94F + 0.12F * std::sin(
        timeSeconds * 4.6F + static_cast<float>(projectile.id.index) * 0.41F);
    BeginBlendMode(BLEND_ALPHA);
    DrawSphereEx(position, radius * (0.94F + pulse * 0.025F), 12, 12,
                 {48, 145, 232, 255});
    EndBlendMode();
    drawIceMagicSphere(position, radius * (1.48F + pulse * 0.04F),
                       timeSeconds, 0.96F,
                       {142, 229, 255, 225});
    BeginBlendMode(BLEND_ADDITIVE);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(timeSeconds * 72.0F, 0.0F, 1.0F, 0.0F);
    DrawCircle3D({}, radius * 1.42F,
                 {1.0F, 0.0F, 0.0F}, 63.0F,
                 {191, 246, 255, 145});
    rlRotatef(117.0F, 0.45F, 0.72F, 0.53F);
    DrawCircle3D({}, radius * 1.27F,
                 {0.0F, 0.0F, 1.0F}, 47.0F,
                 {85, 207, 255, 112});
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
                    ? Color{223, 248, 255, 215}
                    : Color{142, 229, 255, 172});
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
                    {142, 229, 255,
                     static_cast<unsigned char>(95.0F * pulse)});
            }
        }
        EndBlendMode();
    }
}

bool Renderer::drawLootChest(
    LootChestType type, Vector3 position, float yawRadians,
    float openingProgress, Color tint) {
    ModelResource& resource = type == LootChestType::Wooden
        ? resources_.woodenChestModel()
        : resources_.stoneChestModel();
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
        for (int index = 0; index < model.materialCount; ++index)
            model.materials[index].shader = *shader;
    }

    constexpr float ModelScale = 2.45F;
    const float progress = std::clamp(openingProgress, 0.0F, 1.0F);
    const float delayed = std::clamp((progress - 0.08F) / 0.76F, 0.0F, 1.0F);
    constexpr float Back = 1.70158F;
    constexpr float BackPlusOne = Back + 1.0F;
    const float shifted = delayed - 1.0F;
    const float eased = 1.0F + BackPlusOne * shifted * shifted * shifted +
        Back * shifted * shifted;
    const float lidAngle = std::clamp(eased, 0.0F, 1.08F) * -108.0F * DEG2RAD;
    const Matrix scale = MatrixScale(ModelScale, ModelScale, ModelScale);
    const Matrix terrainRotation = terrainAlignedRotation(
        position.x, position.z, yawRadians);
    const Matrix translation =
        MatrixTranslate(position.x, position.y, position.z);
    const Matrix baseTransform = MatrixMultiply(
        MatrixMultiply(scale, terrainRotation), translation);

    const auto drawMesh = [&model, tint](int meshIndex, Matrix transform) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    if (model.meshCount >= 2) {
        drawMesh(1, baseTransform);
        // raylib bakes glTF node transforms into every mesh on load.  The lid
        // vertices therefore already include the Blender node translation;
        // adding it again made the closed lid slide away from the chest.
        // Rotate the baked vertices around the original lid-node origin,
        // which is the authored hinge position.
        const Vector3 lidPivot = type == LootChestType::Wooden
            ? Vector3{0.0F, 0.22719747F, -0.21279876F}
            : Vector3{0.0F, 0.2F, -0.2F};
        const Matrix lidRotation = MatrixRotateX(lidAngle);
        const Matrix toLidOrigin = MatrixTranslate(
            -lidPivot.x, -lidPivot.y, -lidPivot.z);
        const Matrix fromLidOrigin = MatrixTranslate(
            lidPivot.x, lidPivot.y, lidPivot.z);
        const Matrix lidTransform = MatrixMultiply(
            MatrixMultiply(
                MatrixMultiply(
                    MatrixMultiply(
                        MatrixMultiply(toLidOrigin, lidRotation),
                        fromLidOrigin),
                    scale),
                    terrainRotation),
            translation);
        drawMesh(0, lidTransform);
        for (int index = 2; index < model.meshCount; ++index)
            drawMesh(index, baseTransform);
    } else {
        for (int index = 0; index < model.meshCount; ++index)
            drawMesh(index, baseTransform);
    }
    return true;
}

void Renderer::drawLootItem(
    Vector3 position, LootUpgradeEffect effect,
    LootRarity rarity, float rotationRadians, Color tint,
    float scale) {
    const Color color = ColorTint(
        ColorBrightness(lootRarityColor(rarity), 0.22F), tint);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlMultMatrixf(MatrixToFloat(terrainAlignedRotation(
        position.x, position.z, rotationRadians)));
    applyLootItemLocalRotation(effect);
    rlScalef(scale, scale, scale);
    ModelResource* resource = lootItemModelFor(resources_, effect);
    if (resource != nullptr && resource->valid()) {
        Model& model = resource->get();
        Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        if (selectionMaskPassOpen_ &&
            resources_.selectionMaskShader().valid()) {
            shader = resources_.selectionMaskShader().get();
        } else if (shadowPassOpen_ && resources_.shadowShader().valid()) {
            shader = resources_.shadowShader().get();
        } else if (worldShaderActive_ && resources_.worldShader().valid()) {
            shader = resources_.worldShader().get();
        }
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = shader;
        }
        // Keep the authored food colors; rarity belongs to the silhouette,
        // not to a cyan tint over the item itself.
        drawFittedLootModel(model, ColorTint(WHITE, tint));
    } else {
        drawLootItemGeometry(effect, color);
    }
    rlPopMatrix();
}

void Renderer::drawLootItemOutline(
    Vector3 position, LootUpgradeEffect effect,
    LootRarity rarity, float rotationRadians, float scale) {
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlSetCullFace(RL_CULL_FACE_FRONT);
    ModelResource* resource = lootItemModelFor(resources_, effect);
    if (resource != nullptr && resource->valid()) {
        Model& model = resource->get();
        const Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = shader;
        }
    }

    const Color coreColor = lootRarityColor(rarity);
    const auto drawShell = [&](float expansion, Color color) {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        rlRotatef(rotationRadians * RAD2DEG, 0.0F, 1.0F, 0.0F);
        applyLootItemLocalRotation(effect);
        const float shellScale = scale * expansion;
        rlScalef(shellScale, shellScale, shellScale);
        if (resource != nullptr && resource->valid()) {
            drawFittedLootOutlineModel(resource->get(), color);
        } else {
            drawLootItemGeometry(effect, color);
        }
        rlPopMatrix();
    };
    drawShell(1.13F, Fade(coreColor, 0.42F));
    drawShell(1.075F, coreColor);

    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_BACK);
    rlEnableDepthMask();
    EndBlendMode();
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
    const Matrix rotation = MatrixRotateY(yawRadians);
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                rotation),
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
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
        {RockModelScale * scale, RockModelScale * scale,
         RockModelScale * scale},
        tint);
    return true;
}

bool Renderer::drawRocksInstanced(
    std::span<const RockDrawInstance> instances) {
    if (instances.empty() || shadowPassOpen_ ||
        selectionMaskPassOpen_ || !worldShaderActive_ ||
        !resources_.worldShader().valid() ||
        !resources_.rockModel().valid()) {
        return false;
    }

    Model& model = resources_.rockModel().get();
    resourceRockTransforms_.clear();
    resourceRockTransforms_.reserve(instances.size());
    for (const RockDrawInstance& instance : instances) {
        const float scale = instance.scale * worldRevealScaleAt(
            {instance.position.x, instance.position.z});
        if (scale <= 0.001F) {
            continue;
        }
        const float modelScale = RockModelScale * scale;
        Vector3 position = instance.position;
        position.y += RockGroundOffset;
        const Matrix rotation = MatrixIdentity();
        resourceRockTransforms_.push_back(MatrixMultiply(
            model.transform,
            MatrixMultiply(
                MatrixScale(
                    modelScale, modelScale, modelScale),
                MatrixMultiply(
                    rotation,
                    MatrixTranslate(
                        position.x, position.y, position.z)))));
    }
    if (resourceRockTransforms_.empty()) {
        return true;
    }

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (int meshIndex = 0; meshIndex < model.meshCount;
         ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material material = model.materials[materialIndex];
        material.shader = shader;
        material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        DrawMeshInstanced(
            model.meshes[meshIndex], material,
            resourceRockTransforms_.data(),
            static_cast<int>(resourceRockTransforms_.size()));
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
    return true;
}

void Renderer::drawDecorativeRocks(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    ensureGrassClearAreaIndex(clearAreas);
    constexpr std::size_t VariantCount = 4U;
    constexpr float Spacing = 4.45F;
    const float DrawRadius =
        settings_.quality == GraphicsQuality::Low
            ? 25.0F
            : settings_.quality == GraphicsQuality::Medium
                ? 30.0F
                : 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    constexpr float CacheCellSize = 8.0F;
    constexpr float CachePadding = 10.0F;
    const int cameraCellX = static_cast<int>(std::floor(
        cameraPosition.x / CacheCellSize));
    const int cameraCellZ = static_cast<int>(std::floor(
        cameraPosition.z / CacheCellSize));
    const bool revealAnimating = worldRevealElapsed_ < 1.7F;
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
    const bool useInstancing =
        worldShaderActive_ && !shadowPassOpen_ &&
        !selectionMaskPassOpen_ && resources_.worldShader().valid();
    const bool cacheMatches =
        useInstancing && !revealAnimating &&
        decorativeInstanceCacheValid_ &&
        decorativeCacheCameraCellX_ == cameraCellX &&
        decorativeCacheCameraCellZ_ == cameraCellZ &&
        decorativeCacheQuality_ == settings_.quality &&
        decorativeCacheWorldLimit_ == worldLimit &&
        decorativeCacheClearAreaHash_ == grassClearAreaContentHash_ &&
        decorativeCacheTerrain_ == terrainHeightfield_ &&
        decorativeCacheRevealOrigin_.x == worldRevealOrigin_.x &&
        decorativeCacheRevealOrigin_.y == worldRevealOrigin_.y;
    if (useInstancing) {
        for (auto& transforms : decorativeRockTransforms_) {
            transforms.clear();
            transforms.reserve(96U);
        }
        for (auto& transforms : decorativeBushTransforms_) {
            transforms.clear();
            transforms.reserve(96U);
        }
        if (!cacheMatches) {
            for (auto& candidates : decorativeRockCandidates_) {
                candidates.clear();
            }
            for (auto& candidates : decorativeBushCandidates_) {
                candidates.clear();
            }
        }
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

    const float traversalRadius =
        useInstancing && !revealAnimating
            ? DrawRadius + CachePadding
            : DrawRadius;
    const int minimumX = static_cast<int>(std::floor(
        (cameraPosition.x - traversalRadius) / Spacing));
    const int maximumX = static_cast<int>(std::ceil(
        (cameraPosition.x + traversalRadius) / Spacing));
    const int minimumZ = static_cast<int>(std::floor(
        (cameraPosition.z - traversalRadius) / Spacing));
    const int maximumZ = static_cast<int>(std::ceil(
        (cameraPosition.z + traversalRadius) / Spacing));
    const float BushDrawRadius =
        settings_.quality == GraphicsQuality::Low
            ? 27.0F
            : settings_.quality == GraphicsQuality::Medium
                ? 32.0F
                : 36.0F;
    if (!cacheMatches) {
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
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_ != nullptr &&
                terrainHeightfield_->waterSignedDistance(x, z) < 0.7) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                traversalRadius * traversalRadius) {
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
            const float clusterStrength =
                decorativeRockClusterDensity(x, z);
            const float revealScale =
                worldRevealScaleAt({x, z});
            if (revealScale <= 0.001F) {
                continue;
            }
            const float scale =
                baseScale * (0.88F + clusterStrength * 0.28F) *
                visibility * revealScale;
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x63d83595U) * PI * 2.0F;
            const Vector3 position{
                x,
                terrainHeight - (1.0F - visibility) * 0.08F,
                z,
            };
            if (useInstancing) {
                Model& model = resource.get();
                const Matrix transform = MatrixMultiply(
                    model.transform,
                    MatrixMultiply(
                        MatrixScale(scale, scale, scale),
                        MatrixMultiply(
                            terrainAlignedRotation(
                                position.x, position.z, rotation),
                            MatrixTranslate(
                                position.x, position.y,
                                position.z))));
                decorativeRockCandidates_[variant].push_back({
                    .transform = transform,
                    .position = {position.x, position.z},
                    .scale = scale,
                    .groundHeight = terrainHeight,
                });
            } else {
                drawTerrainAlignedModel(
                    resource.get(), position, rotation,
                    {scale, scale, scale}, WHITE);
            }
        }
    }

    constexpr float BushSpacing = 4.9F;
    constexpr std::array<float, BushVariantCount> BushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
    };
    const float bushTraversalRadius =
        useInstancing && !revealAnimating
            ? BushDrawRadius + CachePadding
            : BushDrawRadius;
    const int minimumBushX = static_cast<int>(std::floor(
        (cameraPosition.x - bushTraversalRadius) / BushSpacing));
    const int maximumBushX = static_cast<int>(std::ceil(
        (cameraPosition.x + bushTraversalRadius) / BushSpacing));
    const int minimumBushZ = static_cast<int>(std::floor(
        (cameraPosition.z - bushTraversalRadius) / BushSpacing));
    const int maximumBushZ = static_cast<int>(std::ceil(
        (cameraPosition.z + bushTraversalRadius) / BushSpacing));
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
            const float sharedCluster =
                decorativeRockClusterDensity(x, z);
            const float secondaryCluster =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F);
            const float clusterDensity = std::clamp(
                sharedCluster * 0.74F + secondaryCluster * 0.34F,
                0.0F, 0.88F);
            if (unitFloat(hash) > clusterDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_ != nullptr &&
                terrainHeightfield_->waterSignedDistance(x, z) < 1.0) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                bushTraversalRadius * bushTraversalRadius) {
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
            const float revealScale = worldRevealScaleAt({x, z});
            const float scale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F) *
                (0.90F + sharedCluster * 0.22F) *
                visibility * revealScale;
            if (scale <= 0.001F) {
                continue;
            }
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x1b56c4e9U) * PI * 2.0F;
            const Vector3 position{
                x,
                terrainHeight - (1.0F - visibility) * 0.10F,
                z,
            };
            if (useInstancing) {
                Model& model = resource.get();
                const Matrix transform = MatrixMultiply(
                    model.transform,
                    MatrixMultiply(
                        MatrixScale(scale, scale, scale),
                        MatrixMultiply(
                            terrainAlignedRotation(
                                position.x, position.z, rotation),
                            MatrixTranslate(
                                position.x, position.y,
                                position.z))));
                decorativeBushCandidates_[variant].push_back({
                    .transform = transform,
                    .position = {position.x, position.z},
                    .scale = scale,
                    .groundHeight = terrainHeight,
                });
            } else {
                drawTerrainAlignedModel(
                    resource.get(), position, rotation,
                    {scale, scale, scale}, WHITE);
            }
        }
    }
    }

    if (!useInstancing) {
        return;
    }
    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        for (const CachedInstance& instance :
             decorativeRockCandidates_[variant]) {
            const float offsetX =
                instance.position.x - cameraPosition.x;
            const float offsetZ =
                instance.position.y - cameraPosition.z;
            if (offsetX * offsetX + offsetZ * offsetZ <=
                DrawRadius * DrawRadius) {
                decorativeRockTransforms_[variant].push_back(
                    instance.transform);
            }
        }
    }
    for (std::size_t variant = 0; variant < BushVariantCount;
         ++variant) {
        for (const CachedInstance& instance :
             decorativeBushCandidates_[variant]) {
            const float offsetX =
                instance.position.x - cameraPosition.x;
            const float offsetZ =
                instance.position.y - cameraPosition.z;
            if (offsetX * offsetX + offsetZ * offsetZ <=
                BushDrawRadius * BushDrawRadius) {
                decorativeBushTransforms_[variant].push_back(
                    instance.transform);
            }
        }
    }
    if (!revealAnimating) {
        decorativeInstanceCacheValid_ = true;
        decorativeCacheCameraCellX_ = cameraCellX;
        decorativeCacheCameraCellZ_ = cameraCellZ;
        decorativeCacheQuality_ = settings_.quality;
        decorativeCacheWorldLimit_ = worldLimit;
        decorativeCacheClearAreaHash_ = grassClearAreaContentHash_;
        decorativeCacheTerrain_ = terrainHeightfield_;
        decorativeCacheRevealOrigin_ = worldRevealOrigin_;
    } else {
        decorativeInstanceCacheValid_ = false;
    }
    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    const auto drawInstanced = [&shader](
                                   Model& model,
                                   const std::vector<Matrix>& transforms) {
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex = model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms.data(),
                static_cast<int>(transforms.size()));
        }
    };
    for (std::size_t variant = 0; variant < VariantCount; ++variant) {
        if (decorativeRockTransforms_[variant].empty() ||
            !resources_.decorativeRockModel(variant).valid()) {
            continue;
        }
        drawInstanced(
            resources_.decorativeRockModel(variant).get(),
            decorativeRockTransforms_[variant]);
    }
    for (std::size_t variant = 0; variant < BushVariantCount; ++variant) {
        if (decorativeBushTransforms_[variant].empty() ||
            !resources_.decorativeBushModel(variant).valid()) {
            continue;
        }
        drawInstanced(
            resources_.decorativeBushModel(variant).get(),
            decorativeBushTransforms_[variant]);
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
}

void Renderer::drawDecorativeRockAo(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    ensureGrassClearAreaIndex(clearAreas);
    if (!blobShadowBatchOpen_ ||
        terrainHeightfield_ == nullptr) {
        return;
    }
    constexpr float Spacing = 4.45F;
    constexpr float DrawRadius = 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    constexpr std::size_t BushVariantCount = 6U;
    constexpr std::array<float, BushVariantCount> BushAoRadii{
        0.42F, 0.78F, 0.55F, 0.68F, 0.60F, 0.50F,
    };
    constexpr float BushDrawRadius = 36.0F;
    const int cameraCellX = static_cast<int>(std::floor(
        cameraPosition.x / 8.0F));
    const int cameraCellZ = static_cast<int>(std::floor(
        cameraPosition.z / 8.0F));
    const bool cachedCandidates =
        decorativeInstanceCacheValid_ &&
        decorativeCacheCameraCellX_ == cameraCellX &&
        decorativeCacheCameraCellZ_ == cameraCellZ &&
        decorativeCacheQuality_ == settings_.quality &&
        decorativeCacheWorldLimit_ == worldLimit &&
        decorativeCacheClearAreaHash_ == grassClearAreaContentHash_ &&
        decorativeCacheTerrain_ == terrainHeightfield_ &&
        decorativeCacheRevealOrigin_.x == worldRevealOrigin_.x &&
        decorativeCacheRevealOrigin_.y == worldRevealOrigin_.y &&
        worldRevealElapsed_ >= 1.7F;
    if (cachedCandidates) {
        for (const auto& candidates : decorativeRockCandidates_) {
            for (const CachedInstance& instance : candidates) {
                const float offsetX =
                    instance.position.x - cameraPosition.x;
                const float offsetZ =
                    instance.position.y - cameraPosition.z;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    DrawRadius * DrawRadius) {
                    continue;
                }
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.018F,
                     instance.position.y},
                    instance.scale * 0.62F,
                    instance.scale * 0.52F, 0.16F);
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.02F,
                     instance.position.y},
                    instance.scale * 0.34F,
                    instance.scale * 0.28F, 0.27F);
            }
        }
        for (std::size_t variant = 0;
             variant < BushVariantCount; ++variant) {
            for (const CachedInstance& instance :
                 decorativeBushCandidates_[variant]) {
                const float offsetX =
                    instance.position.x - cameraPosition.x;
                const float offsetZ =
                    instance.position.y - cameraPosition.z;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    BushDrawRadius * BushDrawRadius) {
                    continue;
                }
                const float radius =
                    BushAoRadii[variant] * instance.scale;
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.018F,
                     instance.position.y},
                    radius, radius * 0.82F, 0.14F);
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.02F,
                     instance.position.y},
                    radius * 0.52F, radius * 0.42F, 0.22F);
            }
        }
        return;
    }
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
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_->waterSignedDistance(x, z) < 0.7) {
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
                (0.88F + decorativeRockClusterDensity(x, z) * 0.28F) *
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

    constexpr float BushSpacing = 4.9F;
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
            const float sharedCluster =
                decorativeRockClusterDensity(x, z);
            const float secondaryCluster =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F);
            const float clusterDensity = std::clamp(
                sharedCluster * 0.74F + secondaryCluster * 0.34F,
                0.0F, 0.88F);
            if (unitFloat(hash) > clusterDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_->waterSignedDistance(x, z) < 1.0) {
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
            const float scale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F) *
                (0.90F + sharedCluster * 0.22F) *
                visibility * worldRevealScaleAt({x, z});
            if (scale <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                terrainHeightfield_->getHeight(x, z));
            const float radius = BushAoRadii[variant] * scale;
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
    constexpr float Spacing = 8.0F;
    constexpr std::size_t MaximumInstancesPerVariant = 4096U;
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
            variant.reserve(1024U);
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
                    6.0F + sizeRoll * sizeRoll * 9.0F +
                    slopeProgress * 1.5F;
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

bool Renderer::drawTreesInstanced(
    std::span<const TreeDrawInstance> instances) {
    if (instances.empty() || shadowPassOpen_ ||
        selectionMaskPassOpen_ || !worldShaderActive_ ||
        !resources_.worldShader().valid()) {
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

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
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
            const int materialIndex = model.meshMaterial[meshIndex];
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
        shader, worldInstancingEnabledLocation_, &disabled,
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
    constexpr float ModelScale = 0.82F;
    for (const EnemyDrawInstance& instance : instances) {
        const bool lowDetail = instance.lowDetail && lodAvailable;
        ModelResource* resource = lowDetail
            ? nullptr
            : modelFor(instance.modelVisual);
        if (!lowDetail &&
            (resource == nullptr || !resource->valid())) {
            return false;
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
            batchLoop, lowDetail};
        EnemyBatch& batch = enemyBatches_[key];
        if (batch.transforms.empty()) {
            batch.representative = instance;
            batch.representative.animationSeconds =
                static_cast<float>(frame) /
                SourceAnimationFps;
            activeEnemyBatches_.push_back(&batch);
        }
        const float uniformScale = ModelScale * instance.scale;
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
    for (int detailPass = 0; detailPass < 2; ++detailPass) {
        const bool lowDetailPass = detailPass == 0;
        setSkinningEnabled(shader, !lowDetailPass);
        for (EnemyBatch* batchPointer : activeEnemyBatches_) {
            EnemyBatch& batch = *batchPointer;
            if (batch.representative.lowDetail != lowDetailPass) {
                continue;
            }
            ++nonEmptyBatchCount;
            const EnemyDrawInstance& representative =
                batch.representative;
            if (lowDetailPass) {
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
        const EnemyAnimationSource animation =
            animationFor(representative.modelVisual,
                         representative.animationVisual);
        const ModelAnimation* clip =
            animation.animations->find(animation.clipName);
        auto poseIt = enemyBonePoseCache_.end();
        if (clip != nullptr && clip->keyframeCount > 0 &&
            model.skeleton.boneCount > 0) {
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

        const Matrix* boneMatrices = model.boneMatrices;
        if (poseIt != enemyBonePoseCache_.end()) {
            boneMatrices = poseIt->second.data();
        }
        if (boneMatrices != nullptr &&
            model.skeleton.boneCount > 0 &&
            shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0) {
            rlEnableShader(shader.id);
            rlSetUniformMatrices(
                shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                boneMatrices, model.skeleton.boneCount);
            rlDisableShader();
        }

        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
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
