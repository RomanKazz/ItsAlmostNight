#include "graphics/Renderer.hpp"
#include "graphics/WorldTransforms.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

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
    case LootUpgradeEffect::Blueprint:
        return &resources.blueprintLootModel();
    case LootUpgradeEffect::Hourglass:
        return &resources.hourglassLootModel();
    case LootUpgradeEffect::Rope:
        return &resources.ropeLootModel();
    case LootUpgradeEffect::Damage:
    case LootUpgradeEffect::MoveSpeed:
    case LootUpgradeEffect::MaximumHealth:
        return nullptr;
    }
    return nullptr;
}

constexpr float CommonLootModelScale = 0.36F;

void drawAuthoredLootModel(ModelResource& resource, Color tint) {
    Model& model = resource.get();
    rlPushMatrix();
    rlScalef(
        CommonLootModelScale,
        CommonLootModelScale,
        CommonLootModelScale);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(
            original, tint);
        DrawMesh(model.meshes[meshIndex], material, model.transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    rlPopMatrix();
}

struct LootModelFit {
    Vector3 center{};
    float scale{1.0F};
};

LootModelFit lootModelFit(const ModelResource& resource) {
    const BoundingBox bounds = resource.visualBounds();
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

void drawFittedLootModel(ModelResource& resource, Color tint) {
    Model& model = resource.get();
    const LootModelFit fit = lootModelFit(resource);
    rlPushMatrix();
    rlScalef(fit.scale, fit.scale, fit.scale);
    rlTranslatef(-fit.center.x, -fit.center.y, -fit.center.z);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(
            original, tint);
        DrawMesh(model.meshes[meshIndex], material, model.transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    rlPopMatrix();
}


} // namespace

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
    if (shader != nullptr && model.materials != nullptr) {
        for (int index = 0; index < model.materialCount; ++index)
            model.materials[index].shader = *shader;
    }

    const LootChestWorldTransform worldTransform =
        lootChestWorldTransform(
            type, position, yawRadians, openingProgress);
    if (!worldTransform.valid) return false;
    const auto drawMesh = [&resource, &model, tint](
                              int meshIndex, Matrix transform) {
        if (meshIndex < 0 || meshIndex >= model.meshCount ||
            !resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            return;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            return;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    if (worldTransform.hasLid) {
        drawMesh(1, worldTransform.baseTransform);
        drawMesh(0, worldTransform.lidTransform);
        for (int index = 2; index < model.meshCount; ++index) {
            drawMesh(index, worldTransform.baseTransform);
        }
    } else {
        for (int index = 0; index < model.meshCount; ++index) {
            drawMesh(index, worldTransform.baseTransform);
        }
    }
    return true;
}

LootChestWorldTransform Renderer::lootChestWorldTransform(
    LootChestType type, Vector3 position, float yawRadians,
    float openingProgress) {
    LootChestWorldTransform result{};
    ModelResource& resource = type == LootChestType::Wooden
        ? resources_.woodenChestModel()
        : resources_.stoneChestModel();
    if (!resource.valid()) return result;
    Model& model = resource.get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return result;
    }

    constexpr float ModelScale = 2.45F;
    const float progress = std::clamp(openingProgress, 0.0F, 1.0F);
    const float delayed = std::clamp(
        (progress - 0.08F) / 0.76F, 0.0F, 1.0F);
    constexpr float Back = 1.70158F;
    const float shifted = delayed - 1.0F;
    const float eased = 1.0F + (Back + 1.0F) * shifted * shifted * shifted +
        Back * shifted * shifted;
    const float lidAngle = std::clamp(eased, 0.0F, 1.08F) *
        -108.0F * DEG2RAD;
    const Matrix scale = MatrixScale(ModelScale, ModelScale, ModelScale);
    const Matrix terrainRotation = terrainAlignedRotation(
        position.x, position.z, yawRadians);
    const Matrix translation = MatrixTranslate(
        position.x, position.y, position.z);
    result.baseTransform = MatrixMultiply(
        MatrixMultiply(scale, terrainRotation), translation);
    result.lidTransform = result.baseTransform;
    result.hasLid = model.meshCount >= 2 &&
        resource.meshValid(0U) && resource.meshValid(1U);
    if (result.hasLid) {
        // raylib bakes glTF node translation into lid vertices. Rotate those
        // baked vertices around authored lid-node origin. A bounding-box
        // edge is not the hinge and shifts the lid while it opens.
        const Vector3 pivot = type == LootChestType::Wooden
            ? Vector3{0.0F, 0.22719747F, -0.21279876F}
            : Vector3{0.0F, 0.2F, -0.2F};
        const Matrix lidRotation = MatrixRotateX(lidAngle);
        const Matrix toPivot = MatrixTranslate(
            -pivot.x, -pivot.y, -pivot.z);
        const Matrix fromPivot = MatrixTranslate(
            pivot.x, pivot.y, pivot.z);
        result.lidTransform = MatrixMultiply(
            MatrixMultiply(
                MatrixMultiply(toPivot, lidRotation), fromPivot),
            result.baseTransform);
    }

    bool initialized = false;
    const auto addMeshBounds = [&](int meshIndex, Matrix transform) {
        if (meshIndex < 0 || meshIndex >= model.meshCount ||
            !resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            return;
        }
        const auto bounds = resource.meshBounds();
        if (static_cast<std::size_t>(meshIndex) >= bounds.size()) return;
        world_transforms::expandBounds(
            result.worldBounds,
            world_transforms::transformBounds(
                bounds[static_cast<std::size_t>(meshIndex)],
                MatrixMultiply(model.transform, transform)),
            initialized);
    };
    if (result.hasLid) {
        addMeshBounds(0, result.lidTransform);
        addMeshBounds(1, result.baseTransform);
        for (int index = 2; index < model.meshCount; ++index) {
            addMeshBounds(index, result.baseTransform);
        }
    } else {
        for (int index = 0; index < model.meshCount; ++index) {
            addMeshBounds(index, result.baseTransform);
        }
    }
    result.valid = initialized &&
        world_transforms::finite(result.worldBounds);
    return result;
}

void Renderer::drawLootItem(
    Vector3 position, LootUpgradeEffect effect,
    LootRarity rarity, float rotationRadians, Color tint,
    float scale, Vector3 surfaceNormal) {
    const Color color = ColorTint(
        ColorBrightness(lootRarityColor(rarity), 0.22F), tint);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlMultMatrixf(MatrixToFloat(world_transforms::surfaceRotation(
        surfaceNormal, rotationRadians)));
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
        if (model.materials == nullptr || model.meshMaterial == nullptr) {
            rlPopMatrix();
            return;
        }
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = shader;
        }
        // Keep the authored food colors; rarity belongs to the silhouette,
        // not to a cyan tint over the item itself.
        drawAuthoredLootModel(*resource, ColorTint(WHITE, tint));
    } else {
        drawLootItemGeometry(effect, color);
    }
    rlPopMatrix();
}

void Renderer::drawCoin(
    CoinType type, Vector3 position, float rotationRadians, float scale) {
    ModelResource& resource = resources_.coinModel(
        static_cast<std::size_t>(type));
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(rotationRadians * RAD2DEG, 0.0F, 1.0F, 0.0F);
    rlRotatef(8.0F, 0.0F, 0.0F, 1.0F);
    // 30% smaller than the previous 1.40 import multiplier.
    constexpr float NewCoinScale = 0.98F;
    rlScalef(scale * 0.86F * NewCoinScale,
             scale * 0.86F * NewCoinScale,
             scale * 0.86F * NewCoinScale);
    if (resource.valid()) {
        Model& model = resource.get();
        Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        if (worldShaderActive_ && resources_.worldShader().valid()) {
            shader = resources_.worldShader().get();
        }
        if (model.materials != nullptr) {
            if (resources_.coinOutlineShader().valid()) {
                const Shader outlineShader =
                    resources_.coinOutlineShader().get();
                for (int index = 0; index < model.materialCount; ++index) {
                    model.materials[index].shader = outlineShader;
                }
                // Inverted hull: draw only expanded back faces. This leaves
                // a clean outer silhouette without a screen-space pass.
                rlDrawRenderBatchActive();
                rlEnableBackfaceCulling();
                rlSetCullFace(RL_CULL_FACE_FRONT);
                drawFittedLootModel(resource, WHITE);
                rlDrawRenderBatchActive();
                rlSetCullFace(RL_CULL_FACE_BACK);
            }
            for (int index = 0; index < model.materialCount; ++index) {
                model.materials[index].shader = shader;
            }
        }
        const Color tint = type == CoinType::Bronze
            ? Color{225, 145, 84, 255}
            : type == CoinType::Silver
                ? Color{226, 238, 246, 255}
                : Color{255, 226, 92, 255};
        drawFittedLootModel(resource, tint);
    } else {
        DrawCylinder(
            {0.0F, 0.0F, 0.0F}, 0.18F, 0.18F, 0.055F,
            16, {255, 202, 55, 255});
    }
    rlPopMatrix();
}

BoundingBox Renderer::lootItemWorldBounds(
    Vector3 position, LootUpgradeEffect effect,
    float rotationRadians, float scale, Vector3 surfaceNormal) {
    ModelResource* resource = lootItemModelFor(resources_, effect);
    if (resource == nullptr || !resource->valid()) {
        const float radius = std::max(0.05F, scale * 0.32F);
        return {{position.x - radius, position.y - radius,
                 position.z - radius},
                {position.x + radius, position.y + radius,
                 position.z + radius}};
    }
    const Matrix transform = MatrixMultiply(
        resource->get().transform,
        MatrixMultiply(
            MatrixScale(
                CommonLootModelScale * scale,
                CommonLootModelScale * scale,
                CommonLootModelScale * scale),
            MatrixMultiply(
                world_transforms::surfaceRotation(
                    surfaceNormal, rotationRadians),
                MatrixTranslate(
                    position.x, position.y, position.z))));
    return world_transforms::transformBounds(
        resource->visualBounds(), transform);
}


} // namespace ian

