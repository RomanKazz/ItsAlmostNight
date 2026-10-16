// Standalone acceptance probe; links the existing, unchanged game loaders.
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-5; }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: probe project-root\n";
        return 2;
    }
    try {
        const std::filesystem::path root(argv[1]);
        const auto manifestPath = root / "art/models/laser_levels/manifest.json";
        std::ifstream input(manifestPath);
        auto manifest = nlohmann::json::parse(input);
        nlohmann::json reports = nlohmann::json::array();
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "Laser levels acceptance");
        require(IsWindowReady(), "graphics context");
        for (const auto& row : manifest.at("levels")) {
            const std::string filename = (root / row.at("file").get<std::string>()).string();
            const auto collision = ian::loadGlbCollisionAsset(filename);
            require(collision.valid() && collision.colliders.size() == 1, "collision asset");
            const auto& box = collision.colliders.front();
            require(box.name == "COL_BOX_BODY" && !box.walkable &&
                        box.type == ian::ModelColliderType::Box, "collision contract");
            const auto& cb = row.at("collision_bounds_glb");
            require(near(box.minimum.x, cb["min"][0]) && near(box.minimum.y, cb["min"][1]) &&
                        near(box.minimum.z, cb["min"][2]) && near(box.maximum.x, cb["max"][0]) &&
                        near(box.maximum.y, cb["max"][1]) && near(box.maximum.z, cb["max"][2]), "collision bounds");
            require(collision.sockets.size() == 1, "laser muzzle count");
            const auto& muzzle = collision.sockets.front();
            require(muzzle.name == "MUZZLE_01" && near(muzzle.position.x, 0) &&
                near(muzzle.position.y, 1.020) && near(muzzle.position.z, -.459), "laser muzzle position");
            require(near(muzzle.forward.x, 0) && near(muzzle.forward.y, 0) && near(muzzle.forward.z, -1) &&
                near(muzzle.up.x, 0) && near(muzzle.up.y, 1) && near(muzzle.up.z, 0), "laser muzzle axes");
            ian::ModelResource model;
            require(model.load(filename.c_str()) && model.valid() && model.get().meshCount == 4, "render loader and collision removal");
            int triangles = 0;
            for (int i = 0; i < 4; ++i) {
                require(model.meshValid(static_cast<std::size_t>(i)), "visible mesh invalid");
                triangles += model.get().meshes[i].triangleCount;
                const auto texture = model.get().materials[model.get().meshMaterial[i]].maps[MATERIAL_MAP_ALBEDO].texture;
                require(texture.id > 0 && texture.width == 512 && texture.height == 512, "embedded texture load");
            }
            require(triangles == row.at("visible_triangles").get<int>(), "triangle count");
            const auto& vb = row.at("visible_bounds_glb");
            const auto bounds = model.visualBounds();
            require(near(bounds.min.x, vb["min"][0]) && near(bounds.min.y, vb["min"][1]) &&
                        near(bounds.min.z, vb["min"][2]) && near(bounds.max.x, vb["max"][0]) &&
                        near(bounds.max.y, vb["max"][1]) && near(bounds.max.z, vb["max"][2]), "visual bounds");
            reports.push_back({{"level", row.at("level")}, {"model_resource_valid", true},
                {"collision_asset_valid", true}, {"colliders", 1}, {"muzzles", 1},
                {"visible_meshes", 4}, {"embedded_palette_loaded", true}, {"playtested", false}});
        }
        CloseWindow();
        manifest["integration"]["loader_verified"] = true;
        std::ofstream(manifestPath) << manifest.dump(2) << '\n';
        std::ofstream(root / "art/models/laser_levels/loader_report.json") << reports.dump(2) << '\n';
        std::cout << "LASER_LEVEL_LOADERS_VERIFIED " << reports.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
