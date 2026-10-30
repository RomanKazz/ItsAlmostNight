// Probe all full harvester tiers using the unchanged game loaders.
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using Json=nlohmann::json;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"Usage: probe manifest.json loader_report.json\n";return 2;}
 try{
  std::ifstream input(argv[1]);auto manifest=Json::parse(input);Json report=Json::array();
  SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(64,64,"Harvester levels loader acceptance");require(IsWindowReady(),"graphics context");
  for(const auto& row:manifest.at("levels")){
   const auto path=row.at("glb").get<std::string>();const auto asset=ian::loadGlbCollisionAsset(path);
   require(asset.valid()&&asset.colliders.size()==1,"invalid collision asset");const auto& b=asset.colliders.front();
   const bool stone=row.at("asset")=="stone_harvester";
   require(b.name=="COL_BOX_BODY"&&b.type==ian::ModelColliderType::Box&&!b.walkable,"collider contract");
   require(near(b.minimum.x,-.48)&&near(b.minimum.y,0)&&near(b.minimum.z,-.48)&&near(b.maximum.x,.48)&&near(b.maximum.y,stone?.92:.90)&&near(b.maximum.z,.48),"collider bounds");
   require(asset.sockets.empty(),"no weapon muzzles expected");require(asset.renderMeshIndices.size()==1&&asset.renderMeshIndices[0]==1,"collision render index");
   ian::ModelResource model;require(model.load(path.c_str())&&model.valid(),"ModelResource load");
   const int count=row.at("visible_meshes").get<int>();require(model.get().meshCount==count,"collision stripping");
   for(int i=0;i<count;++i){require(model.meshValid(i),"invalid visible mesh");const auto tex=model.get().materials[model.get().meshMaterial[i]].maps[MATERIAL_MAP_ALBEDO].texture;require(tex.id>0&&tex.width==512&&tex.height==512,"embedded texture");}
   const auto& v=model.visualBounds();const auto& expected=row.at("visible_bounds_glb");
   require(near(v.min.x,expected["min"][0])&&near(v.min.y,expected["min"][1])&&near(v.min.z,expected["min"][2])&&near(v.max.x,expected["max"][0])&&near(v.max.y,expected["max"][1])&&near(v.max.z,expected["max"][2]),"visual bounds");
   report.push_back({{"asset",row["asset"]},{"level",row["level"]},{"glb_collision_valid",true},{"model_resource_valid",true},{"collider_removed",true},{"embedded_texture_loaded",true},{"visible_meshes",count}});
  }
  CloseWindow();std::ofstream(argv[2])<<report.dump(2)<<'\n';manifest["integration"]["loader_checked"]=true;
  manifest["validation"]["loader_report"]="art/models/harvester_levels/loader_report.json";
  std::ofstream(argv[1])<<manifest.dump(2)<<'\n';std::cout<<"HARVESTER_LEVEL_LOADERS_VERIFIED "<<report.size()<<'\n';return 0;
 }catch(const std::exception& e){if(IsWindowReady())CloseWindow();std::cerr<<e.what()<<'\n';return 1;}
}
