#pragma once

#include "graphics/Renderer.hpp"
#include "graphics/RoyalBossEffect.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkRoyalBossGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto target=LoadRenderTexture(1024,768);
    const Camera3D camera{{7,6,12},{0,2,0},{0,1,0},8.0F,CAMERA_ORTHOGRAPHIC};
    bool loaded=true;
    const auto capture=[&](int mode,float time,bool night=false) {
        BeginTextureMode(target);ClearBackground({24,37,50,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;
        if(night) {light.sunIntensity=.03F;light.ambientIntensity=.18F;}
        renderer.beginWorldShader(light);
        if(mode==4 || mode==5) DrawCube({2,3,4},20,12,1,GREEN);
        if(mode==1) {
            DrawPlane({0,0,0},{20,20},{63,87,65,255});
            loaded &= renderer.drawEnemy(EnemyModelVisual::Boss,EnemyAnimationVisual::Walk,time,
                {0,.7F,0},.35F,WHITE,1.35F);
            loaded &= renderer.drawEnemy(EnemyModelVisual::BossSummon,EnemyAnimationVisual::Walk,time,
                {-3,1.25F,0},.35F,WHITE,.52F);
            loaded &= renderer.drawEnemy(EnemyModelVisual::BossSummon,EnemyAnimationVisual::Walk,time,
                {3,1.25F,0},.35F,WHITE,.52F);
        }
        WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
        material.inkOutlineEligible=false;material.inkOccluder=false;
        renderer.setWorldMaterial(material);renderer.beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch(false);
        if(mode==1 || mode==2 || mode==5 || mode==7) {
            royal_boss_effect::aura({0,3,0},camera.position,time,3,.8F,true,mode!=7);
            royal_boss_effect::marker({0,0,0},2.7F,.7F,time,mode!=7);
        }
        if(mode==3) royal_boss_effect::impact({0,0,0},camera.position,time,1.5F,true);
        fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
        if(mode==6) {
            EnemyProjectile shot{};shot.royal=true;shot.radius=.22;shot.duration=4.2;
            shot.lifetimeRemaining=3.8;shot.position={0,2,0};shot.previousPosition=shot.position;
            shot.velocity={8.5,0,0};
            for(bool head:{false,true}) {
                material.inkOccluder=head;renderer.setWorldMaterial(material);
                renderer.beginColorOnlyWorldEffects();fire_wand_effect::beginBatch(head);
                enemy_magic_effect::projectile(shot,camera.position,time,1,head,true);
                fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
            }
        }
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    const auto difference=[](Image a,Image b) {
        Color* first=LoadImageColors(a);Color* second=LoadImageColors(b);int count=0;
        for(int i=0;i<a.width*a.height;++i) if(std::abs(int(first[i].r)-int(second[i].r))+
            std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b))>6) ++count;
        UnloadImageColors(first);UnloadImageColors(second);return count;
    };
    Image empty=capture(0,.1F),models=capture(1,.1F),moving=capture(1,.4F),
        warning=capture(2,.1F),next=capture(2,.4F),night=capture(2,.1F,true),
        impact=capture(3,.12F),later=capture(3,.4F),cover=capture(4,.1F),hidden=capture(5,.1F),
        shot=capture(6,.1F),low=capture(7,.1F);
    const int visible=difference(empty,warning),motion=difference(warning,next),
        blocked=difference(cover,hidden),lighting=difference(warning,night);
    const bool valid=loaded && difference(models,moving)>200 && difference(empty,models)>10000 &&
        visible>1000 && motion>100 && blocked==0 && lighting==0 &&
        difference(impact,later)>1000 && difference(empty,shot)>100 && difference(empty,low)>500;
    if(!valid) std::cerr<<"Royal boss visibility/motion/depth/night: "<<visible<<' '<<motion<<' '<<blocked<<' '<<lighting<<'\n';
    if(const char* directory=std::getenv("IAN_CAPTURE_ALPAKING")) {
        std::filesystem::create_directories(directory);
        for(auto view:{std::pair{"court",models},std::pair{"court-moving",moving},std::pair{"warning",warning},
                       std::pair{"impact",impact},std::pair{"impact-late",later},std::pair{"shot",shot}})
            ExportImage(view.second,(std::filesystem::path(directory)/(std::string(view.first)+".png")).string().c_str());
    }
    for(Image image:{empty,models,moving,warning,next,night,impact,later,cover,hidden,shot,low}) UnloadImage(image);
    UnloadRenderTexture(target);return valid;
}
