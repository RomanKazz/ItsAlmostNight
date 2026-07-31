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
    bool articulatedCannon = false;

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
        articulatedCannon = true;
        break;
    case BuildingType::SlowTrap:
    case BuildingType::Gate:
        break;
    }

    const auto acceptCollision =
        [maxDistance](RayCollision collision)
            -> std::optional<double> {
            if (!collision.hit ||
                collision.distance < 0.0F ||
                static_cast<double>(collision.distance) >
                    maxDistance) {
                return std::nullopt;
            }
            return static_cast<double>(
                collision.distance);
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
        std::optional<double> closest;
        for (int meshIndex = 0;
             meshIndex < model.meshCount; ++meshIndex) {
            Matrix transform = baseTransform;
            if (articulatedCannon && meshIndex == 0) {
                transform = MatrixMultiply(
                    model.transform,
                    MatrixMultiply(
                        MatrixMultiply(
                            MatrixMultiply(
                                scale,
                                MatrixRotateX(
                                    cannonPitchRadians)),
                            rotation),
                        translation));
            }
            const auto distance = acceptCollision(
                GetRayCollisionMesh(
                    ray, model.meshes[meshIndex],
                    transform));
            if (distance &&
                (!closest || *distance < *closest)) {
                closest = distance;
            }
        }
        return closest;
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
    constexpr float ModelScale = 2.0F;
    constexpr float GroundOffset = 0.204F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {ModelScale * scale, ModelScale * scale,
                 ModelScale * scale},
                tint);
    return true;
}

bool Renderer::drawTree(Vector3 position, Color tint,
                        float scale) {
    auto& resource = resources_.treeModel();
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
    constexpr float ModelScale = 2.7F;
    constexpr float GroundOffset = 0.144F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {ModelScale * scale, ModelScale * scale,
                 ModelScale * scale},
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
            enemyAnimationPose_.resize(
                static_cast<std::size_t>(model.skeleton.boneCount));
            for (int modelBone = 0;
                 modelBone < model.skeleton.boneCount; ++modelBone) {
                int sourceBone = -1;
                for (std::size_t sourceIndex = 0;
                     sourceIndex < animation.sourceBones->size();
                     ++sourceIndex) {
                    if (std::strcmp(
                            model.skeleton.bones[modelBone].name,
                            (*animation.sourceBones)[sourceIndex]) == 0) {
                        sourceBone =
                            static_cast<int>(sourceIndex);
                        break;
                    }
                }
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
                enemyAnimationPose_.resize(
                    static_cast<std::size_t>(
                        model.skeleton.boneCount));
                for (int modelBone = 0;
                     modelBone < model.skeleton.boneCount;
                     ++modelBone) {
                    int sourceBone = -1;
                    for (std::size_t sourceIndex = 0;
                         sourceIndex <
                             animation.sourceBones->size();
                         ++sourceIndex) {
                        if (std::strcmp(
                                model.skeleton.bones[modelBone].name,
                                (*animation.sourceBones)[sourceIndex]) == 0) {
                            sourceBone =
                                static_cast<int>(sourceIndex);
                            break;
                        }
                    }
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
