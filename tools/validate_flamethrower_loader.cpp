// Standalone asset acceptance probe. No game source or configuration changes.
// Link with current GlbCollision.cpp and GraphicsResources.cpp plus raylib.
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool near(double actual, double expected) {
    return std::abs(actual-expected) < 1e-5;
}
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: validate_flamethrower_loader asset.glb report.json [manifest.json]\n";
        return 2;
    }
    try {
        const auto collision = ian::loadGlbCollisionAsset(argv[1]);
        require(collision.valid(), "GlbCollisionAsset::valid() failed");
        require(collision.colliders.size()==1, "expected one collider");
        const auto& box=collision.colliders.front();
        require(box.name=="COL_BOX_BODY" && box.type==ian::ModelColliderType::Box && !box.walkable,
                "box name/type/walkable mismatch");
        require(near(box.minimum.x,-.75) && near(box.minimum.y,0) && near(box.minimum.z,-.75) &&
                near(box.maximum.x,.75) && near(box.maximum.y,1.14) && near(box.maximum.z,.75),
                "collider bounds mismatch");
        require(collision.sockets.size()==1, "expected one MUZZLE socket");
        const auto& muzzle=collision.sockets.front();
        require(muzzle.name=="MUZZLE_01" && near(muzzle.position.x,0) &&
                near(muzzle.position.y,.924) && near(muzzle.position.z,-.59),
                "muzzle name/position mismatch");
        require(near(muzzle.forward.x,0) && near(muzzle.forward.y,0) && near(muzzle.forward.z,-1) &&
                near(muzzle.up.x,0) && near(muzzle.up.y,1) && near(muzzle.up.z,0),
                "muzzle direction mismatch");
        require(collision.renderMeshIndices.size()==1 && collision.renderMeshIndices.front()==1,
                "expected collider at raw render mesh index 1");
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64,64,"Flamethrower asset acceptance");
        require(IsWindowReady(), "graphics context failed");
        nlohmann::json report;
        {
            ian::ModelResource model;
            require(model.load(argv[1]) && model.valid(), "ModelResource::load() failed");
            require(model.collisionAsset().valid(), "ModelResource collision asset invalid");
            require(model.get().meshCount==3, "collision mesh was not removed from render model");
            for (int i=0;i<3;++i) require(model.meshValid(i),"invalid visible mesh");
            const auto& bounds=model.visualBounds();
            require(near(bounds.min.y,0) && near(bounds.max.y,1.14), "visual ground/height mismatch");
            const int material=model.get().meshMaterial[0];
            const auto texture=model.get().materials[material].maps[MATERIAL_MAP_ALBEDO].texture;
            require(texture.id>0 && texture.width==512 && texture.height==512,
                    "embedded palette not loaded by renderer");
            report={{"glb_collision_valid",true},{"model_resource_valid",true},
                    {"colliders",1},{"collider_name",box.name},
                    {"collider_min",{box.minimum.x,box.minimum.y,box.minimum.z}},
                    {"collider_max",{box.maximum.x,box.maximum.y,box.maximum.z}},
                    {"muzzles",1},{"muzzle_name",muzzle.name},
                    {"muzzle_position",{muzzle.position.x,muzzle.position.y,muzzle.position.z}},
                    {"muzzle_forward",{muzzle.forward.x,muzzle.forward.y,muzzle.forward.z}},
                    {"muzzle_up",{muzzle.up.x,muzzle.up.y,muzzle.up.z}},
                    {"removed_collision_mesh_indices",collision.renderMeshIndices},
                    {"visible_mesh_count",model.get().meshCount},
                    {"embedded_texture_loaded",true},{"playtested",false},{"aiming_connected",false}};
        }
        CloseWindow();
        std::ofstream(argv[2]) << report.dump(2) << '\n';
        if (argc == 4) {
            std::ifstream file(argv[3]);
            auto manifest=nlohmann::json::parse(file);
            manifest["validation"]["game_loader"]=report;
            manifest["integration"]["loader_checked"]=true;
            manifest["integration"]["note"]="Export, fresh Blender import and current GlbCollisionAsset/ModelResource loaders verified. Building gameplay, aiming and VFX remain unconnected; no playtest performed.";
            std::ofstream(argv[3]) << manifest.dump(2) << '\n';
        }
        std::cout << "FLAMETHROWER_LOADER_VERIFIED " << report.dump() << '\n';
        return 0;
    } catch (const std::exception& error) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
