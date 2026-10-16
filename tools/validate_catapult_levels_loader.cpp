// Validate the replacement overlays through the current game loaders.
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/CatapultModel.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
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
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root(argv[1]);
        const auto path = root / "assets/models/buildings/catapult.glb";
        const auto collision = ian::loadGlbCollisionAsset(path.string());
        require(collision.valid() && collision.colliders.size() == 1 && collision.sockets.size() == 1,
                "original collider/muzzle contract");
        const auto& muzzle = collision.sockets.front();
        require(near(muzzle.position.x, 0) && near(muzzle.position.y, .8762649297714233) &&
                    near(muzzle.position.z, .6370007395744324), "original muzzle position");
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "Catapult upgrade acceptance");
        require(IsWindowReady(), "graphics context");
        {
            ian::ModelResource model;
            require(model.load(path.string().c_str()) && model.valid() && model.get().meshCount == 16,
                    "ModelResource and collider removal");
            for (int i = 0; i < 16; ++i) {
                require(model.meshValid(static_cast<std::size_t>(i)), "valid render mesh");
                const auto texture = model.get().materials[model.get().meshMaterial[i]].maps[MATERIAL_MAP_ALBEDO].texture;
                require(texture.id > 0 && texture.width == 512 && texture.height == 512, "original colormap loaded");
                if (i >= 2) {
                    const auto bounds = model.meshBounds()[static_cast<std::size_t>(i)];
                    const auto expected = ian::catapult_model_detail::partBounds(i);
                    require(near(bounds.min.x, expected.min.x) && near(bounds.min.y, expected.min.y) &&
                        near(bounds.min.z, expected.min.z) && near(bounds.max.x, expected.max.x) &&
                        near(bounds.max.y, expected.max.y) && near(bounds.max.z, expected.max.z), "generated raycast bounds");
                }
            }
        }
        CloseWindow();
        const nlohmann::json report{{"glb_collision_valid", true}, {"model_resource_valid", true},
            {"original_muzzle_preserved", true}, {"colliders", 1}, {"muzzles", 1},
            {"render_mesh_count", 16}, {"levels", 8}, {"overlay_meshes", 14},
            {"embedded_palette_loaded_on_all_meshes", true}, {"generated_bounds_verified", true},
            {"playtested", false}};
        const auto manifestPath = root / "art/models/catapult_levels/manifest.json";
        std::ifstream input(manifestPath);
        auto manifest = nlohmann::json::parse(input);
        manifest["integration"]["loader_verified"] = true;
        std::ofstream(manifestPath) << manifest.dump(2) << '\n';
        std::ofstream(root / "art/models/catapult_levels/loader_report.json") << report.dump(2) << '\n';
        std::cout << "CATAPULT_LEVEL_LOADERS_VERIFIED 8\n";
        return 0;
    } catch (const std::exception& error) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
