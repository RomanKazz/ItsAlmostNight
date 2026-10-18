#include "TestHarness.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/EnemyMagicEffect.hpp"
#include <filesystem>
#include <cstdlib>
using namespace ian;
int main() {
    SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(64,64,"Bee graphics checks");
    if(!IsWindowReady())return 77;
    {
        Renderer renderer;renderer.initialize();
        const auto target=LoadRenderTexture(512,512);
        Camera3D camera{{2.4F,2.2F,5},{0,1.9F,0},{0,1,0},3.2F,CAMERA_ORTHOGRAPHIC};
        const auto render=[&](EnemyModelVisual visual,float time,bool stinger=false,bool night=false) {
            BeginTextureMode(target);ClearBackground(BLANK);BeginMode3D(camera);
            WorldLighting lighting{};lighting.cameraPosition=camera.position;
            if(night){lighting.sunIntensity=.03F;lighting.ambientIntensity=.18F;}
            renderer.beginWorldShader(lighting);
            require(renderer.drawEnemy(visual,EnemyAnimationVisual::Walk,time,{0,.8F,0},.35F,WHITE,.70F),
                "both bee models and native flying animation load through the game renderer");
            if(stinger) {
                WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;material.inkOutlineEligible=false;
                for(bool head:{false,true}) {
                    material.inkOccluder=head;renderer.setWorldMaterial(material);renderer.beginColorOnlyWorldEffects();
                    fire_wand_effect::beginBatch(head);
                    EnemyProjectile shot{};shot.stinger=true;shot.position={.9,1.6,1};shot.previousPosition=shot.position;shot.velocity={7,0,0};
                    enemy_magic_effect::projectile(shot,camera.position,time,.5F,head,true);
                    fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
                }
            }
            renderer.endWorldShader();EndMode3D();EndTextureMode();
            auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
        };
        Image normal=render(EnemyModelVisual::Bee,.13F),next=render(EnemyModelVisual::Bee,.35F),
            elite=render(EnemyModelVisual::BeeElite,.13F),needle=render(EnemyModelVisual::Bee,.13F,true),
            night=render(EnemyModelVisual::Bee,.13F,true,true);
        int visible=0,motion=0,evolution=0,shot=0;
        for(int y=0;y<512;++y)for(int x=0;x<512;++x) {
            auto a=GetImageColor(normal,x,y),b=GetImageColor(next,x,y),c=GetImageColor(elite,x,y),d=GetImageColor(needle,x,y);
            visible+=a.a>20;motion+=ColorToInt(a)!=ColorToInt(b);evolution+=ColorToInt(a)!=ColorToInt(c);shot+=ColorToInt(a)!=ColorToInt(d);
        }
        require(visible>5000 && motion>500 && evolution>500 && shot>100,"bee silhouettes, flapping wings, evolved model and physical stinger are visible");
        if(const char* output=std::getenv("IAN_CAPTURE_BEES")) {
            std::filesystem::create_directories(output);
            for(auto entry:{std::pair{"normal",normal},std::pair{"elite",elite},std::pair{"stinger",needle},std::pair{"night",night}})
                ExportImage(entry.second,(std::filesystem::path(output)/(std::string(entry.first)+".png")).string().c_str());
        }
        if(const char* icon=std::getenv("IAN_EXPORT_BEE_ICON"))ExportImage(normal,icon);
        for(auto image:{normal,next,elite,needle,night})UnloadImage(image);
        UnloadRenderTexture(target);
    }
    CloseWindow();
}
