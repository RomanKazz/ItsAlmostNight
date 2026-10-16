#pragma once

#include "graphics/GraphicsResources.hpp"
#include "graphics/HarvesterModel.hpp"
#include "graphics/Renderer.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>

inline bool checkHarvesterGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources;
    resources.initialize({});
    const auto canvas = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.4F, 1.3F, -1.9F}, {0, .43F, 0}, {0, 1, 0}, 2.1F, CAMERA_ORTHOGRAPHIC};
    const auto difference = [](Image a, Image b) {
        auto* first = LoadImageColors(a);
        auto* second = LoadImageColors(b);
        int count = 0;
        for (int i = 0; i < a.width * a.height; ++i) {
            if (std::abs(int(first[i].r) - int(second[i].r)) > 8 ||
                std::abs(int(first[i].g) - int(second[i].g)) > 8 ||
                std::abs(int(first[i].b) - int(second[i].b)) > 8) ++count;
        }
        UnloadImageColors(first);
        UnloadImageColors(second);
        return count;
    };
    bool valid = true;
    for (const auto type : {BuildingType::LumberMill, BuildingType::Quarry, BuildingType::CrystalMine}) {
        const auto modelAt = [&](int level) -> ModelResource& {
            if (type == BuildingType::LumberMill) return resources.lumberMillModel(level);
            if (type == BuildingType::Quarry) return resources.quarryModel(level);
            return resources.mineModel(level);
        };
        valid = valid && &modelAt(0) == &modelAt(1) && &modelAt(99) == &modelAt(8);
        const auto capture = [&](int level, double seconds, Color tint = WHITE) {
            renderer.beginFrame();
            BeginTextureMode(canvas);
            ClearBackground({26, 30, 40, 255});
            BeginMode3D(camera);
            WorldLighting light{};
            light.cameraPosition = camera.position;
            renderer.beginWorldShader(light);
            renderer.setWorldMaterial({});
            valid = renderer.drawResourceProducer(type, {}, .2F, tint, 1, level, seconds) && valid;
            renderer.endWorldShader();
            EndMode3D();
            EndTextureMode();
            Image image = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&image);
            return image;
        };
        Image previous{};
        for (int level = 1; level <= 8; ++level) {
            auto& resource = modelAt(level);
            valid = valid && resource.valid() && resource.get().meshCount == (type == BuildingType::CrystalMine ? 3 : 4) &&
                resource.collisionAsset().colliders.size() == 1 && resource.collisionAsset().sockets.empty();
            if (!resource.valid()) continue;
            const auto bounds = resource.visualBounds();
            valid = valid && bounds.min.x >= -.481F && bounds.max.x <= .481F &&
                bounds.min.z >= -.481F && bounds.max.z <= .481F && std::abs(bounds.min.y) < 1e-5F &&
                std::abs(renderer.buildingVisualHeight(type, level) - bounds.max.y - .005F) < 1e-4F;
            Image current = capture(level, .13);
            if (level > 1) {
                valid = difference(previous, current) > 25 && valid;
                UnloadImage(previous);
            }
            if (const char* directory = std::getenv("IAN_CAPTURE_HARVESTERS")) {
                std::filesystem::create_directories(directory);
                const auto path = std::filesystem::path(directory) /
                    ("harvester-" + std::to_string(static_cast<int>(type)) + "-level-" + std::to_string(level) + ".png");
                ExportImage(current, path.string().c_str());
            }
            previous = current;
            // Compare exact hover against the rendered pose at every rotation,
            // including animated surfaces and placement above terrain.
            for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
                BuildingInstance building{.type = type, .gridPosition = {0, 0}, .rotation = rotation,
                    .level = static_cast<std::uint8_t>(level), .baseHeight = .4};
                const auto center = buildingWorldPosition(building);
                const float yaw = static_cast<float>(rotation) * PI * .5F;
                for (const float seconds : {.13F, .47F}) {
                    const Matrix world = MatrixMultiply(resource.get().transform,
                        MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(
                            float(center.x), float(center.y) + .005F, float(center.z))));
                    for (const float x : {-.31F, -.10F, .18F}) {
                        const Ray ray{{float(center.x) + x, float(center.y) + .65F, float(center.z) - 2}, {0, 0, 1}};
                        std::optional<float> expected;
                        for (int mesh = 0; mesh < resource.get().meshCount; ++mesh) {
                            const auto hit = GetRayCollisionMesh(ray, resource.get().meshes[mesh], MatrixMultiply(
                                harvester_model::meshPose(type, mesh, seconds), world));
                            if (hit.hit && hit.distance >= 0 && (!expected || hit.distance < *expected)) expected = hit.distance;
                        }
                        const auto actual = renderer.buildingRaycastDistance(building, {}, ray, 6, yaw, 0, 1, seconds);
                        valid = valid && expected.has_value() == actual.has_value() &&
                            (!actual || std::abs(*actual - *expected) < 1e-4);
                    }
                }
            }
        }
        const auto later = capture(8, .47);
        valid = valid && (type == BuildingType::CrystalMine
            ? difference(previous, later) == 0 : difference(previous, later) > 10);
        const auto tinted = capture(8, .13, {140, 180, 240, 160});
        const auto restored = capture(8, .13);
        valid = valid && difference(previous, tinted) > 100 && difference(previous, restored) == 0;
        for (auto image : {previous, later, tinted, restored}) UnloadImage(image);
    }
    UnloadRenderTexture(canvas);
    resources.shutdown();
    return valid;
}
