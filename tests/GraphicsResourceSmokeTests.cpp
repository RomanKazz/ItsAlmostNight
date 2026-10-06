#include "graphics/CrystalMineModel.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"

#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool checkCrystalMineOrbit(ian::ModelResource& resource) {
    if (!resource.valid()) return false;
    Model& model = resource.get();
    if (model.meshCount != 2 || model.meshes[0].triangleCount != 232 ||
        model.meshes[1].triangleCount != 204) return false;
    const Vector3 center{0.07254255F, 0.0F, -0.00784531F};
    const Matrix staticPose = ian::crystal_mine_model_detail::meshTransform(0, 2.0);
    const Matrix orbit = ian::crystal_mine_model_detail::meshTransform(1, 2.0);
    if (Vector3Distance(Vector3Transform(center, orbit), center) > 0.00001F) return false;
    float largestMotion = 0.0F;
    for (int part = 0; part < model.meshCount; ++part) {
        const Mesh& mesh = model.meshes[part];
        // Only indexed vertices belong to this part: the GLB primitives share
        // attribute buffers, so unused vertices must not affect the test.
        for (int index = 0; index < mesh.triangleCount * 3; ++index) {
            const int vertex = mesh.indices ? mesh.indices[index] : index;
            const Vector3 point{mesh.vertices[vertex * 3], mesh.vertices[vertex * 3 + 1],
                                mesh.vertices[vertex * 3 + 2]};
            const Vector3 animated = Vector3Transform(point, part == 0 ? staticPose : orbit);
            if (part == 0 && Vector3Distance(point, animated) > 0.00001F) return false;
            if (part == 1) {
                if (std::abs(point.y - animated.y) > 0.00001F ||
                    std::abs(Vector3Distance(point, center) - Vector3Distance(animated, center)) > 0.00001F)
                    return false;
                largestMotion = std::max(largestMotion, Vector3Distance(point, animated));
            }
        }
    }
    return largestMotion > 0.20F;
}


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

// A world surface must replace the normal attachment even with alpha blending
// enabled. Encoding object IDs in its alpha previously leaked background ink.
bool checkOpaqueNormalAttachment(ian::GraphicsResources& resources) {
    Shader& shader = resources.worldShader().get();
    const float eligible = 1.0F;
    SetShaderValue(shader, GetShaderLocation(shader, "inkOutlineEligible"),
                   &eligible, SHADER_UNIFORM_FLOAT);
    const Camera3D camera{{0.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 0.0F},
                          {0.0F, 1.0F, 0.0F}, 60.0F, CAMERA_PERSPECTIVE};
    std::array<Color, 2> samples{};
    const std::array<Color, 2> backgrounds{RED, BLUE};
    for (std::size_t i = 0; i < backgrounds.size(); ++i) {
        BeginTextureMode(resources.sceneTarget());
        rlActiveDrawBuffers(2);
        ClearBackground(backgrounds[i]);
        BeginMode3D(camera);
        BeginShaderMode(shader);
        DrawCube({0.0F, 0.0F, 0.0F}, 2.0F, 2.0F, 2.0F, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(resources.sceneNormalTexture());
        samples[i] = GetImageColor(image, image.width/2, image.height/2);
        UnloadImage(image);
    }
    return samples[0].a == 255 && samples[1].a == 255 &&
        samples[0].r == samples[1].r && samples[0].g == samples[1].g &&
        samples[0].b == samples[1].b && samples[0].b >= 127;
}

bool checkPlatformVisibilityMasks() {
    const std::array<ian::PlatformFrameInstance, 2> frames{{
        {.id = ian::EntityId{1U, 0U},
         .anchor = {0, 0, 0}},
        {.id = ian::EntityId{2U, 0U},
         .anchor = {2, 0, 0}},
    }};
    const auto edges = ian::platformEdgeVisibilityMasks(frames);
    if (edges.size() != frames.size() ||
        !edges[0][0] || edges[0][1] ||
        edges[1][0] || !edges[1][1] ||
        !edges[0][2] || !edges[0][3] ||
        !edges[1][2] || !edges[1][3]) {
        return false;
    }

    auto sharedFrames = frames;
    sharedFrames[0].supportIds[1] = 7U;
    sharedFrames[1].supportIds[0] = 7U;
    sharedFrames[0].supports[1] = {
        .top = {2.0, 3.0, 0.0},
        .bottom = {2.0, 0.0, 0.0},
        .length = 3.0,
    };
    sharedFrames[1].supports[0] = {
        .top = {2.0, 4.0, 0.0},
        .bottom = {2.0, 1.0, 0.0},
        .length = 3.0,
    };
    const std::array<ian::SharedSupport, 1> supports{{
        {.id = 7U,
         .referenceCount = 2U,
         .active = true},
    }};
    const auto posts = ian::platformSupportVisibilityMasks(
        sharedFrames, supports);
    const auto lengths = ian::platformSupportVisualLengths(
        sharedFrames, supports);
    return posts.size() == sharedFrames.size() &&
        lengths.size() == sharedFrames.size() &&
        posts[0][1] && posts[1][0] &&
        std::abs(lengths[0][1] - 3.0) < 1e-9 &&
        std::abs(lengths[1][0] - 4.0) < 1e-9;
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

bool pickaxePivotAnimationMoves(
    const ian::GlbNodeAnimation& animation) {
    if (!animation.valid()) return false;
    const Transform& base = animation.baseTransform();
    const Transform middle = animation.sample(0.5F);
    const float translationDistance = std::sqrt(
        (base.translation.x - middle.translation.x) *
            (base.translation.x - middle.translation.x) +
        (base.translation.y - middle.translation.y) *
            (base.translation.y - middle.translation.y) +
        (base.translation.z - middle.translation.z) *
            (base.translation.z - middle.translation.z));
    const float rotationDot =
        base.rotation.x * middle.rotation.x +
        base.rotation.y * middle.rotation.y +
        base.rotation.z * middle.rotation.z +
        base.rotation.w * middle.rotation.w;
    return translationDistance > 0.01F ||
        std::abs(rotationDot) < 0.999F;
}

} // namespace

int main() {
    std::cerr << "graphics smoke: create context\n";
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    const bool captureOutlines = std::getenv("IAN_CAPTURE_OUTLINES") != nullptr;
    InitWindow(captureOutlines ? 960 : 64,
               captureOutlines ? 640 : 64,
               "IAN graphics resource smoke");
    if (!IsWindowReady()) {
        std::cerr << "graphics context unavailable; skipping\n";
        return 77;
    }

    int result = 0;
    {
        if (!checkPlatformVisibilityMasks()) {
            std::cerr << "platform visibility masks failed\n";
            result = 1;
        }
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
        if (!checkCrystalMineOrbit(resources.mineModel())) {
            std::cerr << "crystal mine orbit or fixed core/base failed\n";
            result = 1;
        }
        if (!resources.sceneTargetValid() ||
            !resources.sceneScreenSpaceBuffers() ||
            !IsTextureValid(resources.sceneTarget().depth) ||
            !IsTextureValid(resources.sceneNormalTexture()) ||
            !resources.ssaoTargetValid() ||
            !resources.ssaoShader().valid() ||
            !resources.selectionMaskValid() ||
            !resources.viewModelTargetValid() ||
            !resources.viewModelCompositeShader().valid() ||
            !resources.enemyMinionModel().valid() ||
            !resources.enemyRogueModel().valid() ||
            !resources.enemyWarriorModel().valid() ||
            !resources.enemySplitterModel().valid() ||
            !resources.enemySplitlingModel().valid() ||
            !resources.spikeTrapModel().valid() ||
            !resources.grassModelB().valid() ||
            !resources.grassModelC().valid() ||
            !resources.grassModelD().valid() ||
            !resources.gateModel().valid() ||
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
            !resources.pickaxeModel().valid() ||
            !pickaxePivotAnimationMoves(resources.pickaxeAnimation()) ||
            !pickaxePivotAnimationMoves(
                resources.pickaxeAlternateAnimation()) ||
            !resources.clubModel().valid() ||
            !resources.hammerModel().valid() ||
            !resources.iceWandModel().valid() ||
            !resources.iceMagicShader().valid() ||
            !resources.coinOutlineShader().valid() ||
            !resources.heartOutlineShader().valid() ||
            !resources.portalMatterTexture().valid() ||
            !resources.portalMatterShader().valid() ||
            !resources.enemyPortalModel().valid() ||
            !resources.skyboxTexture(0).valid() ||
            !resources.skyboxTexture(1).valid() ||
            !resources.skyboxTexture(2).valid() ||
            !resources.woodenChestModel().valid() ||
            !resources.stoneChestModel().valid() ||
            !resources.coinModel(0).valid() ||
            !resources.coinModel(1).valid() ||
            !resources.coinModel(2).valid() ||
            !resources.destructiblePropModel(0).valid() ||
            !resources.destructiblePropModel(1).valid() ||
            !resources.destructiblePropModel(2).valid() ||
            !resources.decorativeBushModel(6).valid() ||
            !resources.decorativeBushModel(7).valid() ||
            !resources.decorativeBushModel(8).valid() ||
            !resources.appleLootModel().valid() ||
            !resources.breadLootModel().valid() ||
            !resources.ironBarLootModel().valid() ||
            !resources.fuelJerrycanLootModel().valid() ||
            !resources.compassLootModel().valid() ||
            !resources.nailLootModel().valid() ||
            !resources.keyLootModel().valid() ||
            !resources.mapLootModel().valid() ||
            !resources.anvilLootModel().valid() ||
            !resources.sawLootModel().valid() ||
            !resources.potionLootModel().valid() ||
            !resources.blueprintLootModel().valid() ||
            !resources.hourglassLootModel().valid() ||
            !resources.ropeLootModel().valid() ||
            !resources.heartLootModel().valid()) {
            std::cerr << "required graphics resource failed to load\n";
            result = 1;
        }
        if (!hasTexturedRenderableMesh(resources.appleLootModel()) ||
            !hasTexturedRenderableMesh(resources.enemyPortalModel()) ||
            !hasTexturedRenderableMesh(resources.breadLootModel()) ||
            !hasTexturedRenderableMesh(resources.potionLootModel()) ||
            !hasTexturedRenderableMesh(resources.blueprintLootModel()) ||
            !hasTexturedRenderableMesh(resources.hourglassLootModel()) ||
            !hasTexturedRenderableMesh(resources.ropeLootModel()) ||
            !hasTexturedRenderableMesh(resources.heartLootModel()) ||
            !hasTexturedRenderableMesh(resources.coinModel(0)) ||
            !hasTexturedRenderableMesh(resources.coinModel(1)) ||
            !hasTexturedRenderableMesh(resources.coinModel(2)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(0)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(1)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(2)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(6)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(7)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(8)) ||
            !hasTexturedRenderableMesh(resources.gateModel()) ||
            !hasTexturedRenderableMesh(resources.enemyRogueModel()) ||
            !hasTexturedRenderableMesh(resources.enemyWarriorModel())) {
            std::cerr << "item model has no valid albedo texture\n";
            result = 1;
        }
        settings.ssao = false;
        resources.updateFramebuffer(settings);
        if (!resources.sceneTargetValid() ||
            resources.sceneScreenSpaceBuffers() ||
            resources.ssaoTargetValid()) {
            std::cerr << "SSAO disable transition failed\n";
            result = 1;
        }
        settings.postProcessing = false;
        resources.updateFramebuffer(settings);
        if (resources.sceneTargetValid() ||
            resources.ssaoTargetValid()) {
            std::cerr << "post-process disable transition failed\n";
            result = 1;
        }
        settings.pixelSize = 1;
        settings.renderWidth = 1280;
        settings.renderHeight = 720;
        resources.updateFramebuffer(settings);
        const int expectedScaledWidth = static_cast<int>(std::lround(
            static_cast<float>(GetScreenWidth()) * (2.0F / 3.0F)));
        const int expectedScaledHeight = static_cast<int>(std::lround(
            static_cast<float>(GetScreenHeight()) * (2.0F / 3.0F)));
        if (!resources.sceneTargetValid() ||
            resources.sceneWidth() != expectedScaledWidth ||
            resources.sceneHeight() != expectedScaledHeight) {
            std::cerr << "render-resolution scale transition failed\n";
            result = 1;
        }
        settings.renderWidth = 1920;
        settings.renderHeight = 1080;
        resources.updateFramebuffer(settings);
        if (resources.sceneTargetValid()) {
            std::cerr << "native render-resolution transition failed\n";
            result = 1;
        }
        settings.postProcessing = true;
        settings.ssao = true;
        resources.updateFramebuffer(settings);
        if (!resources.sceneTargetValid() ||
            !resources.sceneScreenSpaceBuffers() ||
            !IsTextureValid(resources.sceneTarget().depth) ||
            !IsTextureValid(resources.sceneNormalTexture()) ||
            !resources.ssaoTargetValid()) {
            std::cerr << "SSAO enable transition failed\n";
            result = 1;
        }
        if (!checkOpaqueNormalAttachment(resources)) {
            std::cerr << "normal attachment leaks background through opaque geometry\n";
            result = 1;
        }
        resources.shutdown();
        resources.shutdown();
        std::cerr << "graphics smoke: initialize 2\n";
        resources.initialize(settings);
        resources.shutdown();

        ian::Renderer renderer;
        renderer.initialize();
        const ian::BuildingInstance core{
            .id = {1U, 0U},
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
        };
        const std::array coreBuildings{core};
        const auto coreCenterHit = renderer.buildingRaycastDistance(
            core, coreBuildings,
            {{0.0F, 1.0F, 3.0F}, {0.0F, 0.0F, -1.0F}},
            5.0);
        const auto coreEmptySpaceHit = renderer.buildingRaycastDistance(
            core, coreBuildings,
            {{0.0F, 2.0F, 3.0F}, {0.0F, 0.0F, -1.0F}},
            5.0);
        if (!coreCenterHit || coreEmptySpaceHit) {
            std::cerr << "core aim extends beyond visible model\n";
            result = 1;
        }
        const bool rampSurfaceRaycast =
            renderer.rampRaycastDistance(
                {0.0F, 0.0F, 0.0F}, 0.0F, 1.0F,
                {{0.0F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
                10.0).has_value();
        const bool rampEmptyVolumeRejected =
            !renderer.rampRaycastDistance(
                {0.0F, 0.0F, 0.0F}, 0.0F, 1.0F,
                {{-3.0F, 0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},
                6.0).has_value();
        const Camera3D smokeCamera{
            .position = {0.0F, 1.5F, 4.0F},
            .target = {0.0F, 1.0F, 0.0F},
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 60.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        renderer.beginFrame();
        renderer.beginWorldPass(BLACK, smokeCamera);
        renderer.drawSky(ian::SkyState{});
        BeginMode3D(smokeCamera);
        ian::WorldLighting smokeLighting{};
        smokeLighting.cameraPosition = smokeCamera.position;
        renderer.beginWorldShader(smokeLighting);
        ian::WorldMaterialState treeMaterial{};
        treeMaterial.treeAmount = 1.0F;
        treeMaterial.vegetationAmount = 1.0F;
        renderer.setWorldMaterial(treeMaterial);
        const std::array<ian::TreeDrawInstance, 2> smokeTrees{{
            {.position = {-0.8F, 0.0F, 0.0F},
             .yawRadians = 0.2F,
             .scale = 0.6F,
             .visualVariant = 0U},
            {.position = {0.8F, 0.0F, 0.0F},
             .yawRadians = -0.2F,
             .scale = 0.6F,
             .visualVariant = 0U},
        }};
        const bool instancedTreesDrawn =
            renderer.drawTreesInstanced(smokeTrees);
        renderer.endWorldShader();
        if (captureOutlines) {
            // Reproduce a transparent world particle crossing outlined
            // geometry. It changes scene color/alpha but intentionally does
            // not replace the normal attachment behind it.
            rlDrawRenderBatchActive();
            rlSetBlendFactorsSeparate(
                RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                RL_ZERO, RL_ZERO,
                RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            rlDisableDepthMask();
            DrawSphereEx(
                {0.32F, 2.22F, 1.05F}, 0.22F, 6, 6,
                {255, 182, 55, 150});
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
        }
        EndMode3D();
        renderer.endWorldPass();
        if (captureOutlines) {
            rlDrawRenderBatchActive();
            Image frame = LoadImageFromScreen();
            ExportImage(frame, "/tmp/ian-outline-regression.png");
            UnloadImage(frame);
        }
        EndDrawing();
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
        renderer.drawLootItem(
            {-0.45F, 1.7F, 0.0F},
            ian::LootUpgradeEffect::Magnet,
            ian::LootRarity::Common, 0.2F);
        renderer.drawLootItem(
            {0.45F, 1.7F, 0.0F},
            ian::LootUpgradeEffect::HealthAid,
            ian::LootRarity::Common, -0.2F);
        renderer.drawCoin(
            ian::CoinType::HighValue, {0.0F, 0.55F, 0.0F}, 0.7F, 1.0F);
        renderer.drawHeart({0.55F, 0.55F, 0.0F}, 0.7F, 1.0F);
        const bool barrelDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::Barrel, {-1.3F, 0.0F, -1.0F}, 0.2F);
        const bool crateDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::Crate, {0.0F, 0.0F, -1.0F}, -0.2F);
        const bool itemCrateDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::ItemCrate, {1.3F, 0.0F, -1.0F}, 0.0F);
        const bool portalDrawn = renderer.drawEnemyPortal(
            {0.0F, 0.0F, -2.0F}, 0.0F, 0.4F,
            smokeLighting, 0.35F);
        EndMode3D();
        EndDrawing();
        renderer.shutdown();
        if (!rampSurfaceRaycast || !rampEmptyVolumeRejected ||
            !instancedTreesDrawn || !woodenDrawn || !stoneDrawn ||
            !barrelDrawn ||
            !crateDrawn || !itemCrateDrawn || !portalDrawn) {
            std::cerr << "world prop renderer failed\n";
            result = 1;
        }
    }
    std::cerr << "graphics smoke: close context\n";
    CloseWindow();
    return result;
}
