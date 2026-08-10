#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool checkMissingResources() {
    ian::ShaderResource shader;
    ian::TextureResource texture;
    ian::ModelResource model;
    ian::ModelAnimationsResource animations;
    ian::RenderTextureResource target;

    const bool rejected =
        !shader.load(nullptr, "assets/missing/not_found.fs") &&
        !texture.load("assets/missing/not_found.png") &&
        !model.load("assets/missing/not_found.glb") &&
        !animations.load("assets/missing/not_found.glb") &&
        !target.load(0, 64);
    shader.unload();
    shader.unload();
    texture.unload();
    texture.unload();
    model.unload();
    model.unload();
    animations.unload();
    animations.unload();
    target.unload();
    target.unload();
    return rejected;
}

bool hasTexturedRenderableMesh(
    const ian::ModelResource& resource) {
    const Model& model = resource.get();
    if (model.materials == nullptr || model.meshMaterial == nullptr) {
        return false;
    }
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(
                static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 ||
            materialIndex >= model.materialCount ||
            model.materials[materialIndex].maps == nullptr) {
            continue;
        }
        if (IsTextureValid(model.materials[materialIndex]
                               .maps[MATERIAL_MAP_DIFFUSE]
                               .texture)) {
            return true;
        }
    }
    return false;
}

bool animationMovesSkeleton(
    ian::ModelResource& resource,
    const ian::ModelAnimationsResource& animations,
    const char* clipName) {
    Model& model = resource.get();
    const ModelAnimation* animation = animations.find(clipName);
    if (animation == nullptr || animation->keyframeCount < 2 ||
        model.skeleton.boneCount <= 0 ||
        model.skeleton.boneCount > ian::MaximumGpuSkinningBones ||
        model.boneMatrices == nullptr ||
        !resource.gpuSkinningCompatible()) {
        return false;
    }

    UpdateModelAnimation(model, *animation, 0);
    const std::size_t valueCount =
        static_cast<std::size_t>(model.skeleton.boneCount) * 16U;
    std::vector<float> initial(valueCount);
    const float* values = &model.boneMatrices[0].m0;
    std::copy(values, values + valueCount, initial.begin());

    for (int frame = 1; frame < animation->keyframeCount; ++frame) {
        UpdateModelAnimation(
            model, *animation, static_cast<float>(frame));
        values = &model.boneMatrices[0].m0;
        for (std::size_t index = 0; index < valueCount; ++index) {
            if (std::abs(values[index] - initial[index]) > 0.0001F) {
                UpdateModelAnimation(model, *animation, 0);
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main() {
    std::cerr << "graphics smoke: create context\n";
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "IAN graphics resource smoke");
    if (!IsWindowReady()) {
        std::cerr << "graphics context unavailable; skipping\n";
        return 77;
    }

    int result = 0;
    {
        std::cerr << "graphics smoke: missing resources\n";
        if (!checkMissingResources()) {
            std::cerr << "missing resource fallback failed\n";
            result = 1;
        }

        ian::GraphicsSettings settings;
        settings.shadowMapSize = 64;
        settings.pixelSize = 2;
        ian::GraphicsResources resources;
        std::cerr << "graphics smoke: initialize 1\n";
        resources.initialize(settings);
        if (!resources.sceneTargetValid() ||
            !resources.selectionMaskValid() ||
            !resources.viewModelTargetValid() ||
            !resources.enemyMinionModel().valid() ||
            !resources.enemyRogueModel().valid() ||
            !resources.enemyWarriorModel().valid() ||
            !resources.enemySplitterModel().valid() ||
            !resources.enemySplitlingModel().valid() ||
            !resources.spikeTrapModel().valid() ||
            resources.enemyPinkBlobAnimations().find("Idle") == nullptr ||
            resources.enemyPinkBlobAnimations().find("Walk") == nullptr ||
            resources.enemyPinkBlobAnimations().find("Death") == nullptr ||
            resources.enemyNinjaAnimations().find("Idle") == nullptr ||
            resources.enemyNinjaAnimations().find("Walk") == nullptr ||
            resources.enemyNinjaAnimations().find("Bite_Front") == nullptr ||
            resources.enemyNinjaAnimations().find("HitRecieve") == nullptr ||
            resources.enemyNinjaAnimations().find("Death") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Idle") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Walk") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Bite_Front") == nullptr ||
            resources.enemyMushroomKingAnimations().find("HitRecieve") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Death") == nullptr ||
            resources.enemySplitterAnimations().find("Idle") == nullptr ||
            resources.enemySplitterAnimations().find("Walk") == nullptr ||
            resources.enemySplitterAnimations().find("Bite_Front") == nullptr ||
            resources.enemySplitterAnimations().find("Death") == nullptr ||
            resources.enemySplitlingAnimations().find("Idle") == nullptr ||
            resources.enemySplitlingAnimations().find("Walk") == nullptr ||
            resources.enemySplitlingAnimations().find("Bite_Front") == nullptr ||
            resources.enemySplitlingAnimations().find("Death") == nullptr ||
            !animationMovesSkeleton(
                resources.enemyRogueModel(),
                resources.enemyNinjaAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemyWarriorModel(),
                resources.enemyMushroomKingAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemySplitterModel(),
                resources.enemySplitterAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemySplitlingModel(),
                resources.enemySplitlingAnimations(), "Walk") ||
            !resources.clubModel().valid() ||
            !resources.hammerModel().valid() ||
            !resources.iceWandModel().valid() ||
            !resources.iceMagicShader().valid() ||
            !resources.coinOutlineShader().valid() ||
            !resources.woodenChestModel().valid() ||
            !resources.stoneChestModel().valid() ||
            !resources.coinModel().valid() ||
            !resources.ironBarLootModel().valid() ||
            !resources.fuelJerrycanLootModel().valid() ||
            !resources.compassLootModel().valid() ||
            !resources.nailLootModel().valid() ||
            !resources.keyLootModel().valid() ||
            !resources.mapLootModel().valid() ||
            !resources.anvilLootModel().valid() ||
            !resources.sawLootModel().valid() ||
            !resources.potionLootModel().valid()) {
            std::cerr << "required graphics resource failed to load\n";
            result = 1;
        }
        if (!hasTexturedRenderableMesh(
                resources.potionLootModel()) ||
            !hasTexturedRenderableMesh(resources.coinModel()) ||
            !hasTexturedRenderableMesh(resources.enemyRogueModel()) ||
            !hasTexturedRenderableMesh(resources.enemyWarriorModel())) {
            std::cerr << "item model has no valid albedo texture\n";
            result = 1;
        }
        resources.shutdown();
        resources.shutdown();
        std::cerr << "graphics smoke: initialize 2\n";
        resources.initialize(settings);
        resources.shutdown();

        ian::Renderer renderer;
        renderer.initialize();
        BeginDrawing();
        ClearBackground(BLACK);
        const Camera3D camera{
            .position = {0.0F, 2.0F, 4.0F},
            .target = {0.0F, 0.5F, 0.0F},
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 60.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        BeginMode3D(camera);
        const bool woodenDrawn = renderer.drawLootChest(
            ian::LootChestType::Wooden, {-1.0F, 0.0F, 0.0F},
            0.0F, 0.72F);
        const bool stoneDrawn = renderer.drawLootChest(
            ian::LootChestType::Stone, {1.0F, 0.0F, 0.0F},
            0.0F, 0.72F);
        renderer.drawLootItem(
            {-0.45F, 1.2F, 0.0F},
            ian::LootUpgradeEffect::Apple,
            ian::LootRarity::Common, 0.5F);
        renderer.drawLootItem(
            {0.45F, 1.2F, 0.0F},
            ian::LootUpgradeEffect::Bread,
            ian::LootRarity::Common, -0.5F);
        renderer.drawCoin({0.0F, 0.55F, 0.0F}, 0.7F, 1.0F);
        EndMode3D();
        EndDrawing();
        renderer.shutdown();
        if (!woodenDrawn || !stoneDrawn) {
            std::cerr << "chest renderer failed\n";
            result = 1;
        }
    }
    std::cerr << "graphics smoke: close context\n";
    CloseWindow();
    return result;
}
