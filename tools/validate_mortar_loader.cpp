// Mortar asset probe using the unchanged game loaders.
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a,double b) { return std::abs(a-b)<1e-5; }
}
int main(int argc,char** argv) {
    if (argc!=4) { std::cerr<<"Usage: probe asset.glb report.json manifest.json\n"; return 2; }
    try {
        const auto asset=ian::loadGlbCollisionAsset(argv[1]);
        require(asset.valid() && asset.colliders.size()==1,"invalid collision asset");
        const auto& box=asset.colliders.front();
        require(box.name=="COL_BOX_BODY" && box.type==ian::ModelColliderType::Box && !box.walkable,"collider contract");
        require(near(box.minimum.x,-.6) && near(box.minimum.y,0) && near(box.minimum.z,-.6) &&
                near(box.maximum.x,.6) && near(box.maximum.y,1.450) && near(box.maximum.z,.6),"collider bounds");
        require(asset.sockets.size()==1,"muzzle count");
        const auto& muzzle=asset.sockets.front();
        require(muzzle.name=="MUZZLE_01" && near(muzzle.position.x,0) && near(muzzle.position.y,.870+.480/std::sqrt(2.0)) &&
                near(muzzle.position.z,-.480/std::sqrt(2.0)),"muzzle position");
        require(near(muzzle.forward.x,0) && near(muzzle.forward.y,1/std::sqrt(2.0)) && near(muzzle.forward.z,-1/std::sqrt(2.0)) &&
                near(muzzle.up.x,0) && near(muzzle.up.y,1/std::sqrt(2.0)) && near(muzzle.up.z,1/std::sqrt(2.0)),"muzzle axes");
        require(asset.renderMeshIndices.size()==1 && asset.renderMeshIndices[0]==1,"collider index");
        SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64,64,"Mortar asset acceptance");require(IsWindowReady(),"graphics context");
        nlohmann::json report;
        {
            ian::ModelResource model;require(model.load(argv[1]) && model.valid(),"ModelResource load");
            require(model.collisionAsset().valid() && model.get().meshCount==3,"collision not stripped");
            for (int i=0;i<3;++i) {
                require(model.meshValid(i),"visible mesh invalid");
                const auto texture=model.get().materials[model.get().meshMaterial[i]].maps[MATERIAL_MAP_ALBEDO].texture;
                require(texture.id>0 && texture.width==512 && texture.height==512,"embedded texture not loaded");
            }
            const auto& b=model.visualBounds();
            require(near(b.min.x,-.6) && near(b.min.y,0) && near(b.min.z,-.6) &&
                    near(b.max.x,.6) && near(b.max.y,.870+.73375/std::sqrt(2.0)) && near(b.max.z,.6),"visual bounds");
            report={{"glb_collision_valid",true},{"model_resource_valid",true},{"colliders",1},
                {"collider_name",box.name},{"collider_min",{box.minimum.x,box.minimum.y,box.minimum.z}},
                {"collider_max",{box.maximum.x,box.maximum.y,box.maximum.z}},
                {"muzzles",1},{"muzzle_name",muzzle.name},
                {"muzzle_position",{muzzle.position.x,muzzle.position.y,muzzle.position.z}},
                {"muzzle_forward",{muzzle.forward.x,muzzle.forward.y,muzzle.forward.z}},
                {"muzzle_up",{muzzle.up.x,muzzle.up.y,muzzle.up.z}},
                {"removed_collision_mesh_indices",asset.renderMeshIndices},{"visible_mesh_count",3},
                {"embedded_texture_loaded",true},{"playtested",false},{"attack_connected",false}};
        }
        CloseWindow();std::ofstream(argv[2])<<report.dump(2)<<'\n';
        std::ifstream input(argv[3]);auto manifest=nlohmann::json::parse(input);
        manifest["validation"]["game_loader"]=report;manifest["integration"]["loader_checked"]=true;
        manifest["integration"]["note"]="Export, fresh import and current game loaders verified. Gameplay, mortar attack and VFX integration unconnected; no playtest performed.";
        std::ofstream(argv[3])<<manifest.dump(2)<<'\n';
        std::cout<<"MORTAR_LOADER_VERIFIED "<<report.dump()<<'\n';return 0;
    } catch (const std::exception& e) {
        if (IsWindowReady()) CloseWindow();std::cerr<<e.what()<<'\n';return 1;
    }
}
