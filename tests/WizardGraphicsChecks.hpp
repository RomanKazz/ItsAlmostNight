#pragma once

#include "graphics/EnemyMagicEffect.hpp"
#include "graphics/Renderer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkWizardMagic(ian::Renderer& renderer) {
    using namespace ian;
    const auto target=LoadRenderTexture(768,480);
    const Camera3D camera{{0,1.5F,9},{0,1,0},{0,1,0},3.6F,CAMERA_ORTHOGRAPHIC};
    EnemyProjectile shot{};
    shot.position={1.25,1.1,0}; shot.previousPosition=shot.position;
    shot.velocity={7.2,0,0}; shot.radius=.24; shot.lifetimeRemaining=2.7;
    bool loaded=true;
    const auto render=[&](int mode,float time,bool night=false) {
        BeginTextureMode(target); ClearBackground({20,33,51,255}); BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;
        if(night) {lighting.sunIntensity=.03F;lighting.ambientIntensity=.18F;}
        renderer.beginWorldShader(lighting);
        if(mode==3 || mode==4) DrawCube({0,1,2},9,6,.1F,GREEN);
        if(mode==8) loaded &= renderer.drawEnemy(EnemyModelVisual::Mage,EnemyAnimationVisual::Walk,
            time,{-1,0,0},.35F,WHITE,.94F);
        if(mode!=0 && mode!=3 && mode!=8) {
            WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
            material.inkOutlineEligible=false;
            for(bool head:{false,true}) {
                material.inkOccluder=head;renderer.setWorldMaterial(material);
                renderer.beginColorOnlyWorldEffects();fire_wand_effect::beginBatch(head);
                auto projectile=shot; if(mode==7) projectile.active=false;
                enemy_magic_effect::projectile(projectile,camera.position,time,.5F,head,mode!=6);
                fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
            }
            if(mode==5 || mode==9) {
                material.inkOccluder=true;renderer.setWorldMaterial(material);
                renderer.beginColorOnlyWorldEffects();fire_wand_effect::beginBatch();
                enemy_magic_effect::burst({-.75F,1,0},camera.position,time,1,mode==5,true,17);
                fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
            }
        }
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    const auto difference=[](Image a,Image b) {
        int result=0; Color* first=LoadImageColors(a);Color* second=LoadImageColors(b);
        for(int i=0;i<a.width*a.height;++i) if(std::abs(int(first[i].r)-int(second[i].r))+
            std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b))>6) ++result;
        UnloadImageColors(first);UnloadImageColors(second);return result;
    };
    Image empty=render(0,.1F),flight=render(1,.1F),next=render(1,.38F),night=render(1,.1F,true),
        cover=render(3,.1F),hidden=render(4,.1F),impact=render(5,.12F),impactNext=render(5,.4F),
        low=render(6,.1F),inactive=render(7,.1F),wizard=render(8,.2F),cast=render(9,.08F);
    const int visible=difference(empty,flight),motion=difference(flight,next),
        lighting=difference(flight,night),blocked=difference(cover,hidden);
    const bool valid=loaded && visible>500 && motion>80 && lighting==0 && blocked==0 &&
        difference(impact,impactNext)>200 && difference(empty,low)>300 &&
        difference(empty,inactive)==0 && difference(empty,wizard)>1000 && difference(flight,cast)>100;
    if(!valid) std::cerr<<"Wizard visibility/motion/night/depth: "<<visible<<' '<<motion<<' '<<lighting<<' '<<blocked<<'\n';
    if(const char* directory=std::getenv("IAN_CAPTURE_WIZARD")) {
        std::filesystem::create_directories(directory);
        for(auto view:{std::pair{"wizard",wizard},std::pair{"flight",flight},std::pair{"impact",impact},
                       std::pair{"impact-late",impactNext},std::pair{"cast",cast}})
            ExportImage(view.second,(std::filesystem::path(directory)/(std::string(view.first)+".png")).string().c_str());
    }
    for(Image image:{empty,flight,next,night,cover,hidden,impact,impactNext,low,inactive,wizard,cast}) UnloadImage(image);
    UnloadRenderTexture(target);return valid;
}
