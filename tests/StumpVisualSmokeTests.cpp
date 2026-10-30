#include "TestHarness.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/StumpRootEffect.hpp"
#include <filesystem>
#include <cstdlib>
#include <string>

int main() {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Stump roots visual validation");
    if(!IsWindowReady()) return 77;
    Renderer renderer; renderer.initialize();
    renderer.settings().frameRateLimit=0; renderer.applyFrameRateLimit();
    renderer.settings().renderWidth=1280; renderer.settings().renderHeight=720;
    renderer.settings().pixelSize=1; renderer.settings().ssao=false;
    renderer.settings().paperGrain=false; renderer.settings().shadows=false;
    ModelResource stump;
    require(stump.load("assets/models/characters/stump.glb") && stump.valid(),"Stump loads in the runtime ModelResource");
    require(stump.get().meshCount==1 && stump.get().meshes[0].triangleCount==3926,"runtime Stump mesh stays compact");
    require(IsTextureValid(stump.get().materials[0].maps[MATERIAL_MAP_ALBEDO].texture),"Stump uses its embedded palette");
    const auto bounds=stump.visualBounds();
    requireNear(bounds.max.y-bounds.min.y,1.8,.01,"GLB uses the correct Y-up runtime scale");
    const auto flat=[](float,float) { return 0.0F; };
    const auto slope=[](float x,float z) { return .08F*x+.12F*z; };
    const auto render=[&](float age,bool wall,bool hill,bool side,bool roots) {
        const Camera3D camera{side?Vector3{8,7,-3}:Vector3{0,2,-2.2F},
            {0,.35F,3.7F},{0,1,0},side?44.0F:65.0F,CAMERA_PERSPECTIVE};
        const auto ground=[&](float x,float z){return hill?slope(x,z):flat(x,z);};
        renderer.beginFrame(); renderer.beginWorldPass({110,178,213,255},camera); BeginMode3D(camera);
        WorldLighting lighting{}; lighting.cameraPosition=camera.position;
        renderer.beginWorldShader(lighting);
        WorldMaterialState material{};material.screenAoAmount=0;material.inkOutlineEligible=false;
        renderer.setWorldMaterial(material);
        rlDisableBackfaceCulling();
        rlNormal3f(0,1,0);
        for(int x=-16;x<16;++x) for(int z=-8;z<22;++z) {
            const float a=static_cast<float>(x),b=static_cast<float>(z);
            const Color green=((x+z)%2)?Color{87,133,48,255}:Color{85,130,46,255};
            DrawTriangle3D({a,ground(a,b),b},{a+1,ground(a+1,b),b},{a,ground(a,b+1),b+1},green);
            DrawTriangle3D({a+1,ground(a+1,b),b},{a+1,ground(a+1,b+1),b+1},{a,ground(a,b+1),b+1},green);
        }
        DrawModel(stump.get(),{3,ground(3,4),4},1,WHITE);
        if(wall) DrawCube({0,5,-.2F},30,12,.4F,{83,95,96,255});
        rlDrawRenderBatchActive();
        material.unlitAmount=.28F; material.inkOccluder=true;
        renderer.setWorldMaterial(material);renderer.beginColorOnlyWorldEffects();
        rlDrawRenderBatchActive();rlSetTexture(0);rlBegin(RL_TRIANGLES);
        if(roots) stump_root_effect::emitWave({0,ground(0,.5F),.5F},{0,0,1},1,age,103U,ground);
        rlEnd();rlDrawRenderBatchActive();renderer.endColorOnlyWorldEffects();
        rlEnableBackfaceCulling();renderer.endWorldShader();EndMode3D();renderer.endWorldPass();
        rlDrawRenderBatchActive();Image result=LoadImageFromScreen();renderer.endFrame();return result;
    };
    const auto difference=[](const Image& a,const Image& b) {
        Color* x=LoadImageColors(a);Color* y=LoadImageColors(b);int changed=0;
        for(int i=0;i<a.width*a.height;++i)
            if(std::abs(int(x[i].r)-int(y[i].r))+std::abs(int(x[i].g)-int(y[i].g))+std::abs(int(x[i].b)-int(y[i].b))>12) ++changed;
        UnloadImageColors(x);UnloadImageColors(y);return changed;
    };
    const char* output=std::getenv("IAN_CAPTURE_STUMP");
    if(output) std::filesystem::create_directories(output);
    Image warm=render(0,false,false,false,false); UnloadImage(warm);
    warm=render(0,false,false,false,false); UnloadImage(warm);
    Image plain=render(0,false,false,false,false);
    int frame=0;
    for(const float age:{.04F,.15F,.28F,.43F,.65F,.90F,1.25F}) {
        Image result=render(age,false,false,false,true);
        if(age==.43F) require(difference(plain,result)>1500,"root geometry is visible through the real world shader");
        if(age==1.25F) {
            if(output) { ExportImage(plain,(std::filesystem::path(output)/"plain.png").string().c_str());ExportImage(result,(std::filesystem::path(output)/"expired.png").string().c_str()); }
            require(difference(plain,result)==0,"expired roots leave no persistent geometry");
        }
        if(output) ExportImage(result,(std::filesystem::path(output)/("roots-"+std::to_string(frame++)+".png")).string().c_str());
        UnloadImage(result);
    }
    for(const bool hill:{false,true}) {
        Image a=render(.43F,false,hill,true,false),b=render(.43F,false,hill,true,true);
        require(difference(a,b)>1500,"roots remain visible over flat and sloping terrain");
        if(output) ExportImage(b,(std::filesystem::path(output)/(hill?"roots-hill.png":"roots-side.png")).string().c_str());
        UnloadImage(a);UnloadImage(b);
    }
    Image occluded=render(.43F,true,false,false,true),wall=render(.43F,true,false,false,false);
    require(difference(occluded,wall)==0,"world depth hides roots behind opaque scenery");
    UnloadImage(occluded);UnloadImage(wall);UnloadImage(plain);
    stump.unload();renderer.shutdown();CloseWindow();
    std::cout<<"Stump model and runtime root rendering passed\n";
}
