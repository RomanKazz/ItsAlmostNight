#include "graphics/CrystalMineModel.hpp"
#include "graphics/QuarryModel.hpp"
#include "graphics/CoreCrystalModel.hpp"
#include "graphics/CannonModel.hpp"
#include "graphics/CrossbowModel.hpp"
#include "graphics/CatapultModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/SpikeTrapModel.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/AtmosphereParticles.hpp"
#include "app/AppEffectDrawing.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "world/VisualBiomes.hpp"

#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <vector>

namespace {

std::size_t changedPixels(const Image& first, const Image& second) {
    Color* a = LoadImageColors(first);
    Color* b = LoadImageColors(second);
    std::size_t changed = 0;
    for (int pixel = 0; pixel < first.width * first.height; ++pixel) {
        if (std::abs(static_cast<int>(a[pixel].r) - b[pixel].r) +
            std::abs(static_cast<int>(a[pixel].g) - b[pixel].g) +
            std::abs(static_cast<int>(a[pixel].b) - b[pixel].b) > 12) ++changed;
    }
    UnloadImageColors(a);
    UnloadImageColors(b);
    return changed;
}

bool checkAdditiveEffectPass() {
    ian::ShaderResource resource;
    if (!resource.load(nullptr, "assets/shaders/vfx_impact.fs")) return false;
    const int textureLocation = GetShaderLocation(resource.get(), "useTexture");
    Image source = GenImageColor(8, 8, {60, 200, 80, 128});
    const Texture2D texture = LoadTextureFromImage(source);
    UnloadImage(source);
    const auto target = LoadRenderTexture(64, 32);
    bool valid = true;
    for (const bool tagged : {true, false}) {
        const Shader shader = tagged ? resource.get() : Shader{};
        BeginTextureMode(target);
        ClearBackground({20, 30, 40, 255});
        // Submit background geometry before switching the blend/shader state.
        DrawRectangle(0, 0, 64, 32, {20, 30, 40, 255});
        ian::app_effect_drawing::beginAdditiveEffectPass(shader, textureLocation);
        DrawRectangle(2, 2, 12, 12, {240, 80, 40, 128});
        ian::app_effect_drawing::endAdditiveEffectPass(shader);
        ian::app_effect_drawing::beginAdditiveEffectPass(shader, textureLocation,
            ian::app_effect_drawing::EffectSurface::Texture);
        DrawTexture(texture, 20, 2, WHITE);
        ian::app_effect_drawing::endAdditiveEffectPass(shader);
        DrawRectangle(40, 2, 12, 12, {80, 120, 160, 255});
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        const auto nearColor = [](Color actual, Color expected) {
            return std::abs(int(actual.r) - int(expected.r)) <= 2 &&
                   std::abs(int(actual.g) - int(expected.g)) <= 2 &&
                   std::abs(int(actual.b) - int(expected.b)) <= 2;
        };
        const Color geometry = GetImageColor(image, 6, 6);
        const Color flipbook = GetImageColor(image, 23, 5);
        valid = valid && nearColor(geometry, {140, 70, 60, 255}) &&
            nearColor(flipbook, {50, 130, 80, 255}) &&
            nearColor(GetImageColor(image, 44, 6), {80, 120, 160, 255});
        if (tagged) {
            valid = valid && std::abs(int(geometry.a) - 96) <= 1 &&
                            std::abs(int(flipbook.a) - 96) <= 1;
        }
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
    UnloadTexture(texture);
    return valid;
}

bool checkAtmosphereRendering() {
    ian::atmosphere::Field field;
    field.configure(842, 384);
    const auto biomes = ian::visual_biomes::generate(842, 384);
    const auto target = LoadRenderTexture(640, 320);
    const Camera3D camera{{0, 0, 4}, {0, 0, 0}, {0, 1, 0}, 2.4F, CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground({20, 28, 30, 255});
    BeginMode3D(camera);
    rlBegin(RL_TRIANGLES);
    for (std::size_t biome = 0; biome < 4; ++biome) {
        ian::atmosphere::Particle p;
        p.position = {biomes.centers[biome][0], 0, biomes.centers[biome][1]};
        p.opacity = .85F; p.size = .95F;
        const Color color = field.leafColor(p);
        for (int face = 0; face < 2; ++face) {
            p.position = {-1.8F + static_cast<float>(biome) * 1.2F, .6F - static_cast<float>(face) * 1.2F, 0};
            p.yaw = static_cast<float>(face) * PI; p.tilt = .17F;
            for (const auto& vertex : ian::atmosphere::leafVertices(p, color)) {
                rlColor4ub(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
                rlVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
            }
        }
    }
    rlEnd();
    EndMode3D();
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Color* pixels = LoadImageColors(image);
    bool visible = true;
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 4; ++column) {
            int painted = 0;
            for (int y = row * 160; y < (row + 1) * 160; ++y) {
                for (int x = column * 160; x < (column + 1) * 160; ++x) {
                    const Color pixel = pixels[y * 640 + x];
                    if (pixel.r + pixel.g + pixel.b > 110) ++painted;
                }
            }
            visible = visible && painted > 600;
        }
    }
    if (const char* directory = std::getenv("IAN_CAPTURE_ATMOSPHERE")) {
        std::filesystem::create_directories(directory);
        ExportImage(image, (std::filesystem::path{directory} / "leaves.png").string().c_str());
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return visible;
}

bool checkSpikeTrapLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.2F, 1.25F, -1.6F}, {0, .10F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float seconds = -1.0F, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawSpikeTrap({0, 0, 0}, 0, seconds, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_SPIKE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("spike-level-" + std::to_string(level) + "-time-" + std::to_string(seconds) +
                 (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 1; level <= 8; ++level) {
        Image idle = capture(level);
        if (level > 1) {
            const auto difference = changedPixels(previous, idle);
            if (difference <= 80) std::cerr << "spike level " << level << " differs by only " << difference << " pixels\n";
            valid = valid && difference > 80;
        }
        Image raised = capture(level, .5F);
        Image halfway = capture(level, .1F);
        Image retracted = capture(level, 1.2F);
        valid = valid && changedPixels(idle, raised) > 80 &&
            changedPixels(idle, halfway) > 80 && changedPixels(raised, halfway) > 80 &&
            changedPixels(idle, retracted) == 0;
        UnloadImage(raised);
        UnloadImage(halfway);
        UnloadImage(retracted);
        UnloadImage(previous);
        previous = idle;
    }
    Image tinted = capture(8, -1, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    for (const std::uint8_t rotation : {std::uint8_t{0}, std::uint8_t{1}}) {
        ian::BuildingInstance trap{.type = ian::BuildingType::SpikeTrap, .rotation = rotation};
        const auto center = ian::buildingWorldPosition(trap);
        const Matrix yaw = MatrixRotateY(static_cast<float>(rotation)*PI*.5F);
        Vector3 origin = Vector3Transform({.225F*1.2F, .30F*1.2F, -2}, yaw);
        origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y) + .005F, static_cast<float>(center.z)});
        const Ray ray{origin, Vector3Transform({0, 0, 1}, yaw)};
        trap.level = 1;
        const auto plain = renderer.buildingRaycastDistance(trap, {}, ray, 4, 0, 0, 1, .5F);
        trap.level = 8;
        const auto idle = renderer.buildingRaycastDistance(trap, {}, ray, 4);
        const auto active = renderer.buildingRaycastDistance(trap, {}, ray, 4, 0, 0, 1, .5F);
        const bool aligned = !plain && !idle && active;
        if (!aligned) std::cerr << "spike aiming, rotation " << static_cast<int>(rotation) << ": " << plain.value_or(-1) << ", " << idle.value_or(-1) << ", " << active.value_or(-1) << '\n';
        valid = valid && aligned;
    }
    return valid;
}

bool checkCrystalMineLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.7F, 1.5F, 2.2F}, {0, .43F, 0}, {0, 1, 0},
                          36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawResourceProducer(
            ian::BuildingType::CrystalMine, {0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_MINE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("mine-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    Image previous = capture(1);
    bool distinct = true;
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        distinct = distinct && changedPixels(previous, current) > 35;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    const float lowHeight = renderer.buildingVisualHeight(ian::BuildingType::CrystalMine, 1);
    const float highHeight = renderer.buildingVisualHeight(ian::BuildingType::CrystalMine, 8);
    bool correctAiming = highHeight > lowHeight + .25F;
    for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
        bool hitRaisedCrystals = false;
        for (int x = -20; x <= 20; ++x) {
            ian::BuildingInstance mine{.type = ian::BuildingType::CrystalMine,
                                      .rotation = rotation, .level = 1};
            const auto center = ian::buildingWorldPosition(mine);
            const Ray ray{{static_cast<float>(center.x) + static_cast<float>(x) * .02F,
                           .80F, static_cast<float>(center.z) + 2}, {0, 0, -1}};
            correctAiming = correctAiming && !renderer.buildingRaycastDistance(mine, {}, ray, 4);
            mine.level = 8;
            hitRaisedCrystals |= renderer.buildingRaycastDistance(mine, {}, ray, 4).has_value();
        }
        correctAiming = correctAiming && hitRaisedCrystals;
    }
    return distinct && correctAiming;
}

bool checkQuarryLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.25F, 1.1F, 1.8F}, {0, .32F, 0}, {0, 1, 0},
                          36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawResourceProducer(
            ian::BuildingType::Quarry, {0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_QUARRY_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("quarry-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    Image previous = capture(1);
    bool distinct = true;
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        distinct = distinct && changedPixels(previous, current) > 35;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    const float lowHeight = renderer.buildingVisualHeight(ian::BuildingType::Quarry, 1);
    const float highHeight = renderer.buildingVisualHeight(ian::BuildingType::Quarry, 8);
    bool correctAiming = highHeight > lowHeight + .20F;
    for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
        bool hitRaisedCrystals = false;
        for (int x = -20; x <= 20; ++x) {
            ian::BuildingInstance mine{.type = ian::BuildingType::Quarry,
                                      .rotation = rotation, .level = 1};
            const auto center = ian::buildingWorldPosition(mine);
            const Ray ray{{static_cast<float>(center.x) + static_cast<float>(x) * .02F,
                           .60F, static_cast<float>(center.z) + 2}, {0, 0, -1}};
            correctAiming = correctAiming && !renderer.buildingRaycastDistance(mine, {}, ray, 4);
            mine.level = 8;
            hitRaisedCrystals |= renderer.buildingRaycastDistance(mine, {}, ray, 4).has_value();
        }
        correctAiming = correctAiming && hitRaisedCrystals;
    }
    return distinct && correctAiming;
}

bool checkGateLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.6F, 1.0F, -1.9F}, {0, .26F, 0}, {0, 1, 0}, 28, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float open = 0.0F, Color tint = WHITE, bool neighbors = false) {
        const Camera3D view = neighbors
            ? Camera3D{{2, 1.6F, -3.4F}, {0, .26F, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE}
            : camera;
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(view);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = view.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawGate({0, 0, 0}, 0, open, tint, 1, level));
        if (neighbors) {
            static_cast<void>(renderer.drawWall({-1, 0, 0}, 0, 0, WHITE, 1, level));
            static_cast<void>(renderer.drawWall({1, 0, 0}, 0, 0, WHITE, 1, level));
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_GATE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("gate-level-" + std::to_string(level) + "-open-" + std::to_string(open) +
                 (neighbors ? "-neighbors" : "") + (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "gate level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    for (const float open : {-1.0F, .5F, 1.0F}) {
        Image opened = capture(8, open);
        valid = valid && changedPixels(previous, opened) > 80;
        UnloadImage(opened);
    }
    Image connected = capture(8, 0, WHITE, true);
    UnloadImage(connected);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    for (const std::uint8_t rotation : {std::uint8_t{0}, std::uint8_t{1}}) {
        ian::BuildingInstance gate{.type = ian::BuildingType::Gate, .rotation = rotation, .level = 1, .open = true};
        const auto center = ian::buildingWorldPosition(gate);
        const Matrix yaw = MatrixRotateY(static_cast<float>(rotation)*PI*.5F);
        for (const float open : {-1.0F, 0.0F, 1.0F}) {
            const Matrix pose = MatrixMultiply(ian::gate_model_detail::doorHingeTransform(open), yaw);
            Vector3 origin = Vector3Transform({-.39F, .535F, .4F}, pose);
            origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y) + .005F, static_cast<float>(center.z)});
            const Ray ray{origin, Vector3Transform({0, 0, -1}, MatrixMultiply(MatrixRotateY(ian::gate_model_detail::OpenAngle*open), yaw))};
            gate.level = 1;
            const auto plain = renderer.buildingRaycastDistance(gate, {}, ray, 1, 0, 0, open);
            gate.level = 7;
            const auto upgraded = renderer.buildingRaycastDistance(gate, {}, ray, 1, 0, 0, open);
            const bool hingeAligned = !plain && upgraded && std::abs(*upgraded - .315) < .002;
            if (!hingeAligned) std::cerr << "gate cap aiming, rotation " << static_cast<int>(rotation) << ", open " << open << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
            valid = valid && hingeAligned;
        }
    }
    return valid;
}

bool checkCatapultLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, 3.4F}, {0, .53F, .20F}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, bool loaded = false, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCatapult({0, 0, 0}, 0, pitch, loaded, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CATAPULT_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("catapult-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (loaded ? "-loaded" : "") + (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "catapult level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, -.35F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    Image loaded = capture(8, 0, true);
    valid = valid && changedPixels(previous, loaded) > 80;
    UnloadImage(loaded);
    Image tinted = capture(8, 0, false, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    ian::BuildingInstance catapult{.type = ian::BuildingType::Catapult, .level = 1};
    const auto center = ian::buildingWorldPosition(catapult);
    for (const float pitch : {0.0F, -.35F}) {
        constexpr float Yaw = PI * .5F;
        const Matrix pose = MatrixMultiply(ian::catapult_model_detail::pitchTransform(pitch), MatrixRotateY(Yaw));
        Vector3 origin = Vector3Transform({0, .925F, 3}, pose);
        origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
        const Ray ray{origin, Vector3Transform({0, 0, -1}, MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(Yaw)))};
        catapult.level = 1;
        const auto plain = renderer.buildingRaycastDistance(catapult, {}, ray, 8, Yaw, pitch);
        catapult.level = 7;
        const auto upgraded = renderer.buildingRaycastDistance(catapult, {}, ray, 8, Yaw, pitch);
        const bool bucketAligned = upgraded && (!plain || *upgraded < *plain - .02);
        if (!bucketAligned) std::cerr << "catapult raised rim aiming at pitch " << pitch << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
        valid = valid && bucketAligned;
    }
    return valid;
}

bool checkCrossbowLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, -3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, Color tint = WHITE, bool rear = false) {
        const Camera3D view = rear
            ? Camera3D{{2.4F, 2.2F, 3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE}
            : camera;
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(view);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = view.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCrossbow({0, 0, 0}, 0, tint, 1, pitch, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CROSSBOW_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("crossbow-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (tint.a == 255 ? "" : "-tinted") + (rear ? "-rear" : "") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "crossbow level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, .30F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    Image rear = capture(8, 0, WHITE, true);
    for (const float pitch : {-.3F, .3F}) {
        Image rearPitched = capture(8, pitch, WHITE, true);
        valid = valid && changedPixels(rear, rearPitched) > 80;
        UnloadImage(rearPitched);
    }
    UnloadImage(rear);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    ian::BuildingInstance crossbow{.type = ian::BuildingType::Turret, .level = 1};
    const auto center = ian::buildingWorldPosition(crossbow);
    for (const float pitch : {0.0F, .30F}) {
        constexpr float Yaw = PI * .5F;
        const Matrix pose = MatrixMultiply(ian::crossbow_model_detail::pitchTransform(pitch), MatrixRotateY(Yaw));
        Vector3 origin = Vector3Transform({0, 1.030F, -3}, pose);
        origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
        const Ray ray{origin, Vector3Transform({0, 0, 1}, MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(Yaw)))};
        crossbow.level = 1;
        const auto plain = renderer.buildingRaycastDistance(crossbow, {}, ray, 8, Yaw, pitch);
        crossbow.level = 7;
        const auto upgraded = renderer.buildingRaycastDistance(crossbow, {}, ray, 8, Yaw, pitch);
        const bool bowAligned = upgraded && (!plain || *upgraded < *plain - .02);
        if (!bowAligned) std::cerr << "crossbow raised bow aiming at pitch " << pitch << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
        valid = valid && bowAligned;
    }
    return valid;
}

bool checkCannonLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, -3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCannon({0, 0, 0}, 0, pitch, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CANNON_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("cannon-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (tint.a == 255 ? "" : "-tinted") +
                 ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "cannon level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, .30F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    UnloadImage(previous);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    // A raised, yawed muzzle collar must follow the pitched barrel for aiming.
    ian::BuildingInstance cannon{.type = ian::BuildingType::Cannon, .level = 1};
    const auto center = ian::buildingWorldPosition(cannon);
    constexpr float Pitch = .30F;
    constexpr float Yaw = PI * .5F;
    const Matrix pose = MatrixMultiply(ian::cannon_model_detail::pitchTransform(Pitch), MatrixRotateY(Yaw));
    Vector3 origin = Vector3Transform({0, .980118F, -3}, pose);
    origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
    const Ray ray{origin, Vector3Transform({0, 0, 1}, MatrixMultiply(MatrixRotateX(Pitch), MatrixRotateY(Yaw)))};
    const auto plain = renderer.buildingRaycastDistance(cannon, {}, ray, 8, Yaw, Pitch);
    cannon.level = 6;
    const auto upgraded = renderer.buildingRaycastDistance(cannon, {}, ray, 8, Yaw, Pitch);
    const bool collarAligned = upgraded && (!plain || *upgraded < *plain - .02);
    if (!collarAligned) std::cerr << "cannon pitched collar aiming: " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
    return valid && collarAligned;
}

bool checkWallLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.6F, 1.0F, -1.9F}, {0, .26F, 0}, {0, 1, 0}, 28, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, std::uint8_t connections = 0U) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawWall({0, 0, 0}, connections, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_WALL_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("wall-level-" + std::to_string(level) + "-mask-" +
                 std::to_string(connections) + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "wall level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    // All junctions and their rotations must use the matching upgrade mesh.
    for (std::uint8_t mask = 1; mask < 16; ++mask) {
        Image plain = capture(1, mask);
        Image upgraded = capture(8, mask);
        valid = valid && changedPixels(plain, upgraded) > 80;
        UnloadImage(plain);
        UnloadImage(upgraded);
    }
    UnloadRenderTexture(target);
    ian::BuildingInstance wall{.type = ian::BuildingType::Wall, .level = 1};
    const ian::Vec3 center = ian::buildingWorldPosition(wall);
    const Ray capRay{{static_cast<float>(center.x) + .436F, .545F,
        static_cast<float>(center.z) - 2}, {0, 0, 1}};
    const auto plainCap = renderer.buildingRaycastDistance(wall, {}, capRay, 4);
    wall.level = 7;
    const auto upgradedCap = renderer.buildingRaycastDistance(wall, {}, capRay, 4);
    if (plainCap || !upgradedCap)
        std::cerr << "wall cap aiming: " << plainCap.value_or(-1) << " -> " << upgradedCap.value_or(-1) << '\n';
    return valid && !plainCap && upgradedCap;
}

bool checkGunTurretLevelRendering(ian::Renderer& renderer) {
    const bool correctHeights =
        renderer.buildingVisualHeight(ian::BuildingType::GunTurret, 1) < 1.10F &&
        renderer.buildingVisualHeight(ian::BuildingType::GunTurret, 7) > 1.15F &&
        renderer.buildingVisualHeight(ian::BuildingType::Core, 8) >
            renderer.buildingVisualHeight(ian::BuildingType::Core, 1) + 0.30F;
    const auto target = LoadRenderTexture(384, 384);
    const Camera3D camera{{2.4F, 1.9F, -3.2F}, {0, 0.62F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawGunTurret({0, 0, 0}, 0, 0, WHITE, 1, {}, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_GUN_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("turret-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    bool distinct = correctHeights;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "turret level " << level << " differs by only " << difference << " pixels\n";
        distinct = distinct && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    Image recovered = capture(1);
    distinct = distinct && changedPixels(original, recovered) == 0;
    UnloadImage(original);
    UnloadImage(recovered);
    UnloadRenderTexture(target);

    ian::BuildingInstance turret{.type = ian::BuildingType::GunTurret, .level = 1};
    const ian::Vec3 center = ian::buildingWorldPosition(turret);
    const Ray topRay{{static_cast<float>(center.x), 1.16F,
        static_cast<float>(center.z) - 4}, {0, 0, 1}};
    const auto oldTop = renderer.buildingRaycastDistance(turret, {}, topRay, 8);
    turret.level = 7;
    const auto armoredTop = renderer.buildingRaycastDistance(turret, {}, topRay, 8);
    const Ray collarRay{{static_cast<float>(center.x) - 3, 0.805445F,
        static_cast<float>(center.z) + 0.140388F}, {1, 0, 0}};
    turret.level = 1;
    const auto oldCollar = renderer.buildingRaycastDistance(turret, {}, collarRay, 8, PI * 0.5F);
    turret.level = 6;
    const auto rotatedCollar = renderer.buildingRaycastDistance(turret, {}, collarRay, 8, PI * 0.5F);
    Ray hollowRay = collarRay;
    hollowRay.position.y = 0.685445F;
    turret.level = 1;
    const auto oldHollow = renderer.buildingRaycastDistance(turret, {}, hollowRay, 8, PI * 0.5F);
    turret.level = 6;
    const auto hollow = renderer.buildingRaycastDistance(turret, {}, hollowRay, 8, PI * 0.5F);
    // A hollow muzzle must not be selected as a solid box in front of the housing.
    const bool hollowPreserved = oldHollow && hollow && std::abs(*oldHollow - *hollow) < 0.001;
    if (oldTop || !armoredTop || !oldCollar || !rotatedCollar ||
        *rotatedCollar >= *oldCollar - 0.02 || !hollowPreserved)
        std::cerr << "turret aiming: top " << oldTop.value_or(-1) << " -> " << armoredTop.value_or(-1)
                  << ", collar " << oldCollar.value_or(-1) << " -> " << rotatedCollar.value_or(-1) << '\n';
    return distinct && !oldTop && armoredTop && oldCollar && rotatedCollar &&
        *rotatedCollar < *oldCollar - 0.02 && hollowPreserved;
}

bool checkCoreOrbitRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(384, 384);
    const Camera3D camera{{0, 1.5F, 5}, {0, 1.0F, 0}, {0, 1, 0}, 36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level = 1) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCore({0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image before = capture();
    WaitTime(.35);
    Image after = capture();
    bool changed = changedPixels(before, after) > 200;
    if (const char* directory = std::getenv("IAN_CAPTURE_CORE_ORBIT")) {
        std::filesystem::create_directories(directory);
        ExportImage(before, (std::filesystem::path{directory} / "core-before.png").string().c_str());
        ExportImage(after, (std::filesystem::path{directory} / "core-after.png").string().c_str());
    }
    UnloadImage(before);
    UnloadImage(after);
    Image previous = capture(1);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        changed = changed && changedPixels(previous, current) > 120;
        if (const char* directory = std::getenv("IAN_CAPTURE_CORE_LEVELS")) {
            std::filesystem::create_directories(directory);
            if (level == 2) ExportImage(previous,
                (std::filesystem::path{directory} / "core-level-1.png").string().c_str());
            ExportImage(current, (std::filesystem::path{directory} /
                ("core-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    return changed;
}

bool checkSequentialMountainRendering(ian::Renderer& renderer) {
    ian::TerrainHeightfield terrain;
    terrain.generate(42);
    renderer.rebuildTerrain(terrain);
    renderer.setWorldReveal({0, 0}, 1000);
    const auto target = LoadRenderTexture(480, 270);
    const Camera3D camera{{0, static_cast<float>(terrain.getHeight(0, 0)) + 2, 0},
                          {0, 8, -100}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
    const auto capture = [&](float lower, float upper) {
        BeginTextureMode(target);
        ClearBackground({95, 160, 215, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.terrainAmount = 1;
        material.distantFadeAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawTerrain({66, 112, 67, 255}, camera.position, false, upper, lower);
        material.terrainAmount = 0;
        material.treeAmount = 1;
        material.coniferAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawBoundaryForest(camera);
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image flat = capture(0, 0);
    Image lower = capture(1, 0);
    Image both = capture(1, 1);
    const bool valid = changedPixels(flat, lower) > 1000 && changedPixels(lower, both) > 1000;
    if (const char* directory = std::getenv("IAN_CAPTURE_CORE_ORBIT")) {
        ExportImage(flat, (std::filesystem::path{directory} / "mountains-start.png").string().c_str());
        ExportImage(lower, (std::filesystem::path{directory} / "mountains-lower.png").string().c_str());
        ExportImage(both, (std::filesystem::path{directory} / "mountains-both.png").string().c_str());
    }
    UnloadImage(flat);
    UnloadImage(lower);
    UnloadImage(both);
    UnloadRenderTexture(target);
    // Renderer borrows this terrain; detach it before the local field expires.
    renderer.shutdown();
    renderer.initialize();
    return valid;
}

bool checkVisualBiomeRendering(ian::Renderer& renderer) {
    auto config = ian::WorldConfig::defaults();
    config.terrainResolution = 129;
    config.terrainAmplitude = 0;
    config.terrainBoundaryRiseHeight = 0;
    config.terrainSurfaceNoiseAmplitude = 0;
    config.pondMinimumCount = config.pondMaximumCount = 0;
    ian::TerrainHeightfield terrain{config};
    terrain.generate(42);
    renderer.rebuildTerrain(terrain);
    renderer.setWorldReveal({0, 0}, 1000);
    const auto biomes = ian::visual_biomes::generate(terrain.seed(), config.terrainWorldSize);
    const auto target = LoadRenderTexture(384, 288);
    std::array<Vector3, 4> groundColors{};
    std::array<Color, 4> propColors{};
    for (std::size_t region = 0; region < 4; ++region) {
        const float x = biomes.centers[region][0], z = biomes.centers[region][1];
        const Camera3D camera{{x, 4.5F, z + 12}, {x, 1, z - 1}, {0, 1, 0}, 55, CAMERA_PERSPECTIVE};
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        lighting.sunColor = {1, 1, 1};
        lighting.cloudShadowStrength = 0;
        lighting.fogStart = 500;
        lighting.fogEnd = 700;
        BeginTextureMode(target);
        ClearBackground({100, 170, 220, 255});
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.terrainAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawTerrain(WHITE, camera.position);
        material.terrainAmount = 0;
        material.treeAmount = material.vegetationAmount = 1;
        renderer.setWorldMaterial(material);
        static_cast<void>(renderer.drawTree({x - 2.8F, 0, z - 1}, WHITE, 1, 0));
        static_cast<void>(renderer.drawTree({x + 2.8F, 0, z - 2}, WHITE, 1, 3));
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawRock({x, 0, z - 4}, WHITE, .8F));
        renderer.drawDecorativeRocks(camera, 192);
        renderer.drawGrassInstances(camera.position, 192, 0, lighting);
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image scene = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&scene);
        Vector3 average{};
        for (int py = 212; py < 274; ++py) {
            for (int px = 155; px < 229; ++px) {
                const auto color = GetImageColor(scene, px, py);
                average.x += color.r; average.y += color.g; average.z += color.b;
            }
        }
        groundColors[region] = Vector3Scale(average, 1.0F / (62 * 74));
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES")) {
            std::filesystem::create_directories(directory);
            constexpr std::array names{"forest", "pink-grove", "autumn", "cold"};
            ExportImage(scene, (std::filesystem::path{directory} / (std::string{names[region]} + ".png")).string().c_str());
        }
        UnloadImage(scene);
        // The same ordinary prop must retain its authored color in every zone.
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        material = {};
        material.unlitAmount = 1;
        renderer.setWorldMaterial(material);
        DrawCube({x, 1, z - 1}, 3, 3, 3, {100, 130, 190, 255});
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image prop = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&prop);
        propColors[region] = GetImageColor(prop, 192, 144);
        UnloadImage(prop);
    }
    // A large crown straddling a transition must match the same tree wholly
    // inside the region selected by its trunk, in both rendering paths.
    Vector2 border{};
    std::size_t borderRegion = 0;
    bool foundBorder = false;
    for (int z = -150; z < 150 && !foundBorder; ++z) {
        for (int x = -150; x < 150 && !foundBorder; ++x) {
            const auto blend = ian::visual_biomes::weights(biomes, float(x), float(z));
            const auto region = static_cast<std::size_t>(
                std::max_element(blend.begin(), blend.end()) - blend.begin());
            if (blend[region] > .55F && blend[region] < .80F) {
                border = {float(x), float(z)};
                borderRegion = region;
                foundBorder = true;
            }
        }
    }
    const auto captureTree = [&](Vector2 position, bool instanced) {
        const Camera3D camera{{position.x, 8, position.y + 28},
            {position.x, 5, position.y}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        lighting.sunColor = {1, 1, 1};
        lighting.cloudShadowStrength = 0;
        lighting.fogStart = 500;
        lighting.fogEnd = 700;
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.treeAmount = material.vegetationAmount = 1;
        renderer.setWorldMaterial(material);
        if (instanced) {
            const std::array instances{ian::TreeDrawInstance{
                .position = {position.x, 0, position.y}, .scale = 3}};
            static_cast<void>(renderer.drawTreesInstanced(instances));
        } else {
            static_cast<void>(renderer.drawTree({position.x, 0, position.y}, WHITE, 3, 0));
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    bool discreteTrees = foundBorder;
    for (const bool instanced : {false, true}) {
        Image edgeTree = captureTree(border, instanced);
        Image centerTree = captureTree(
            {biomes.centers[borderRegion][0], biomes.centers[borderRegion][1]}, instanced);
        const auto changes = changedPixels(edgeTree, centerTree);
        discreteTrees = discreteTrees && changes < 50;
        if (changes >= 50) std::cerr << "boundary crown changes " << changes << " instanced " << instanced << '\n';
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES"))
            ExportImage(edgeTree, (std::filesystem::path{directory} /
                (instanced ? "boundary-tree-instanced.png" : "boundary-tree.png")).string().c_str());
        UnloadImage(edgeTree);
        UnloadImage(centerTree);
    }
    // Probe the actual grass shader on a bright blade under daytime exposure:
    // blue vegetation must keep color in its highlights rather than turn white.
    ian::ShaderResource grassShader;
    bool blueGrass = grassShader.load("assets/shaders/grass_instanced.vs", "assets/shaders/grass_instanced.fs");
    if (blueGrass) {
        Shader& shader = grassShader.get();
        const auto uniform = [&](const char* name, const void* value, int type, int count = 1) {
            SetShaderValueV(shader, GetShaderLocation(shader, name), value, type, count);
        };
        uniform("worldBiomeCenters[0]", biomes.centers.data(), SHADER_UNIFORM_VEC2, 4);
        uniform("worldBiomeGround[0]", biomes.ground.data(), SHADER_UNIFORM_VEC3, 4);
        uniform("worldBiomeShape", biomes.shape.data(), SHADER_UNIFORM_VEC4);
        const Vector4 white{1, 1, 1, 1};
        const Vector3 daylight{1, 1, 1}, sun{-1, -1, -1};
        const float exposure = 1.01F, saturation = 1.03F, fogStart = 500, fogEnd = 700;
        uniform("grassTint", &white, SHADER_UNIFORM_VEC4);
        uniform("dayNightTint", &daylight, SHADER_UNIFORM_VEC3);
        uniform("sunDirection", &sun, SHADER_UNIFORM_VEC3);
        uniform("exposure", &exposure, SHADER_UNIFORM_FLOAT);
        uniform("saturation", &saturation, SHADER_UNIFORM_FLOAT);
        uniform("fogStart", &fogStart, SHADER_UNIFORM_FLOAT);
        uniform("fogEnd", &fogEnd, SHADER_UNIFORM_FLOAT);
        const auto cold = biomes.centers[3];
        const Camera3D camera{{cold[0], 1, cold[1] + 4}, {cold[0], 0, cold[1]},
            {0, 1, 0}, 40, CAMERA_PERSPECTIVE};
        uniform("cameraPosition", &camera.position, SHADER_UNIFORM_VEC3);
        Mesh blade = GenMeshCube(1, 1, .08F);
        Material material = LoadMaterialDefault();
        material.shader = shader;
        const Matrix transform = MatrixTranslate(cold[0], 0, cold[1]);
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        // The shader's alpha is an ink tag; the game writes RGB opaquely.
        rlDisableColorBlend();
        DrawMeshInstanced(blade, material, &transform, 1);
        rlEnableColorBlend();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        int coloredPixels = 0;
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                const Color color = GetImageColor(image, x, y);
                if (color.b < 60) continue;
                ++coloredPixels;
                blueGrass = blueGrass && color.b > color.r + 50 && color.r < 150 && color.b < 235;
            }
        }
        blueGrass = blueGrass && coloredPixels > 500;
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES"))
            ExportImage(image, (std::filesystem::path{directory} / "cold-grass.png").string().c_str());
        UnloadImage(image);
        UnloadMesh(blade);
        material.shader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
        UnloadMaterial(material);
        grassShader.unload();
    }
    const bool distinct = groundColors[0].y > groundColors[0].x &&
        groundColors[1].x > groundColors[1].y + 12 &&
        groundColors[2].x > groundColors[2].z + 12 &&
        groundColors[3].z > groundColors[3].x + 8;
    bool unchangedProps = true;
    for (std::size_t i = 1; i < 4; ++i)
        unchangedProps = unchangedProps && propColors[i].r == propColors[0].r &&
            propColors[i].g == propColors[0].g && propColors[i].b == propColors[0].b;
    if (!distinct) {
        for (const auto color : groundColors) std::cerr << "biome RGB " << color.x << ' ' << color.y << ' ' << color.z << '\n';
    }
    UnloadRenderTexture(target);
    renderer.shutdown();
    renderer.initialize();
    return distinct && unchangedProps && discreteTrees && blueGrass;
}

bool checkCoreCrystalOrbit(ian::ModelResource& resource) {
    if (!resource.valid() || resource.get().meshCount != 7) return false;
    const Model& model = resource.get();
    for (int part = 0; part < model.meshCount; ++part) {
        const Mesh& mesh = model.meshes[part];
        const Matrix pose = ian::core_crystal_model_detail::meshTransform(part, 2.0);
        const bool satellite = part >= 1 && part <= 4;
        const BoundingBox bounds = GetMeshBoundingBox(mesh);
        const Vector3 center{(bounds.min.x + bounds.max.x) * .5F,
                             (bounds.min.y + bounds.max.y) * .5F,
                             (bounds.min.z + bounds.max.z) * .5F};
        if (satellite) {
            // Verify the loader baked the original node offsets: rotating an
            // unbaked mesh would only spin it about itself.
            const Vector3 orbitCenter = Vector3Transform(center, pose);
            if (std::abs(std::hypot(center.x, center.z) - .25F) > .001F ||
                std::abs(std::hypot(orbitCenter.x, orbitCenter.z) - .29F) > .001F ||
                Vector3Distance(center, orbitCenter) < .15F ||
                orbitCenter.y - center.y < .0349F || orbitCenter.y - center.y > .0651F) return false;
        }
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const Vector3 point{mesh.vertices[vertex * 3], mesh.vertices[vertex * 3 + 1],
                                mesh.vertices[vertex * 3 + 2]};
            const Vector3 animated = Vector3Transform(point, pose);
            if (!satellite && Vector3Distance(point, animated) > .00001F) return false;
            const Vector3 spread = Vector3Transform(point,
                ian::core_crystal_model_detail::meshTransform(part, 0));
            const float lift = Vector3Transform(center, pose).y - center.y;
            if (std::abs(animated.y - point.y - lift) > .00001F ||
                std::abs(std::hypot(spread.x, spread.z) - std::hypot(animated.x, animated.z)) > .00001F ||
                std::abs(Vector3Distance(point, center) -
                    Vector3Distance(animated, Vector3Transform(center, pose))) > .00001F)
                return false;
        }
    }
    const Vector3 first = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(1, 0));
    const Vector3 second = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(2, 0));
    if (std::abs(first.y - second.y) < .01F) return false;
    for (int part = 1; part <= 4; ++part) {
        const Vector3 start = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(part, 0));
        const Vector3 loop = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(part, 6));
        if (std::abs(start.y - loop.y) > .00001F) return false;
        for (int step = 0; step < 120; ++step) {
            const auto before = ian::core_crystal_model_detail::meshTransform(part, step / 20.0);
            const auto after = ian::core_crystal_model_detail::meshTransform(part, (step + 1) / 20.0);
            const float y = Vector3Transform({}, before).y;
            if (y < .0349F || y > .0651F ||
                std::abs(y - Vector3Transform({}, after).y) > .0009F) return false;
        }
    }
    // Bounds used by aiming and feedback must contain every upgraded part,
    // including rotating satellite rings, without changing the build footprint.
    for (int level = 1; level <= 8; ++level) {
        const BoundingBox bounds = ian::core_crystal_model_detail::levelBounds(resource.visualBounds(), level);
        for (int frame = 0; frame < 24; ++frame) {
            bool contained = true;
            ian::core_crystal_model_detail::forEachLevelPart(level, frame * 0.25,
                [&](int meshIndex, Matrix pose, Color, bool) {
                    const Mesh& mesh = model.meshes[meshIndex];
                    for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
                        const Vector3 point = Vector3Transform({mesh.vertices[vertex * 3],
                            mesh.vertices[vertex * 3 + 1], mesh.vertices[vertex * 3 + 2]}, pose);
                        constexpr float tolerance = 0.0001F;
                        contained = contained && point.x >= bounds.min.x - tolerance &&
                            point.x <= bounds.max.x + tolerance && point.y >= bounds.min.y - tolerance &&
                            point.y <= bounds.max.y + tolerance && point.z >= bounds.min.z - tolerance &&
                            point.z <= bounds.max.z + tolerance;
                    }
                });
            if (!contained) return false;
        }
    }
    return true;
}

bool checkCrystalMineStaticShards(ian::ModelResource& resource) {
    if (!resource.valid()) return false;
    Model& model = resource.get();
    if (model.meshCount != ian::crystal_mine_model_detail::MeshCount ||
        model.meshes[0].triangleCount != 232 ||
        model.meshes[1].triangleCount != 204) return false;
    for (int level = 1; level <= 8; ++level) {
        int emitted = 0;
        bool expected = true;
        ian::crystal_mine_model_detail::forEachLevelPart(level,
            [&](int meshIndex, Matrix pose) {
                expected = expected && meshIndex < model.meshCount &&
                    model.meshes[meshIndex].triangleCount > 0 &&
                    (meshIndex == 1 || meshIndex == 10 || meshIndex == level + 1);
                const Vector3 posed = Vector3Transform({.23F, .45F, .12F}, pose);
                expected = expected && (meshIndex == 1
                    ? posed.y > .45F && posed.y < 1.0F
                    : meshIndex == 10 || Vector3Distance(posed, {.23F, .45F, .12F}) < .00001F);
                ++emitted;
            });
        if (!expected || emitted != 2 + (level >= 3) + (level >= 5) + (level >= 7)) return false;
    }
    return true;
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
        if (!checkCoreCrystalOrbit(resources.coreModel())) {
            std::cerr << "core satellites fail to orbit the fixed central crystal/pedestal\n";
            result = 1;
        }
        if (!checkCrystalMineStaticShards(resources.mineModel())) {
            std::cerr << "crystal mine static shards or level meshes failed\n";
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
        if (std::getenv("IAN_PROFILE_FIRST_SHOT")) {
            const auto target = LoadRenderTexture(64, 64);
            const Camera3D camera{{0, 0, 5}, {0, 0, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
            for (int frame = 0; frame < 8; ++frame) {
                const double start = GetTime();
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginMode3D(camera);
                renderer.beginWorldShader({});
                ian::WorldMaterialState material{};
                material.unlitAmount = 1;
                material.screenAoAmount = 0;
                material.inkOutlineEligible = false;
                renderer.setWorldMaterial(material);
                rlDisableDepthMask();
                rlDisableBackfaceCulling();
                BeginBlendMode(BLEND_ALPHA);
                DrawTriangle3D({-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, {255, 180, 30, 180});
                rlDrawRenderBatchActive();
                EndBlendMode();
                BeginBlendMode(BLEND_ADDITIVE);
                DrawTriangle3D({-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, {255, 180, 30, 180});
                rlDrawRenderBatchActive();
                EndBlendMode();
                rlEnableDepthMask();
                rlEnableBackfaceCulling();
                renderer.endWorldShader();
                EndMode3D();
                EndTextureMode();
                Image image = LoadImageFromTexture(target.texture);
                UnloadImage(image);
                std::cerr << "first-shot render " << frame << " ms " << (GetTime() - start) * 1000 << '\n';
            }
            UnloadRenderTexture(target);
        }
        if (!checkAdditiveEffectPass()) {
            std::cerr << "additive effects lose coverage, foreground tags or render-state restoration\n";
            result = 1;
        }
        if (!checkAtmosphereRendering()) {
            std::cerr << "biome leaves disappear when viewed from either side\n";
            result = 1;
        }
        if (!checkCoreOrbitRendering(renderer)) {
            std::cerr << "core orbit or distinct level appearances failed\n";
            result = 1;
        }
        if (!checkWallLevelRendering(renderer)) {
            std::cerr << "wall levels, junctions, cap aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkGunTurretLevelRendering(renderer)) {
            std::cerr << "gun turret levels, material restoration or rotated armor aiming failed\n";
            result = 1;
        }
        if (!checkCannonLevelRendering(renderer)) {
            std::cerr << "cannon levels, pitched collar aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkCrossbowLevelRendering(renderer)) {
            std::cerr << "crossbow levels, pitched bow aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkCatapultLevelRendering(renderer)) {
            std::cerr << "catapult level rendering check failed\n";
            result = 1;
        }
        if (!checkSpikeTrapLevelRendering(renderer)) {
            std::cerr << "spike trap level rendering failed\n";
            result = 1;
        }
        if (!checkCrystalMineLevelRendering(renderer)) {
            std::cerr << "crystal mine levels fail to render distinctly\n";
            result = 1;
        }
        if (!checkQuarryLevelRendering(renderer)) {
            std::cerr << "quarry levels, visual height or raised stone aiming failed\n";
            result = 1;
        }
        if (!checkGateLevelRendering(renderer)) {
            std::cerr << "gate levels, moving reinforcement aiming or tint restoration failed\n";
            result = 1;
        }
        if (!checkSequentialMountainRendering(renderer)) {
            std::cerr << "lower/upper mountain rendering stages fail\n";
            result = 1;
        }
        if (!checkVisualBiomeRendering(renderer)) {
            std::cerr << "biome palettes fail to recolor the ground or leak onto ordinary props\n";
            result = 1;
        }
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
