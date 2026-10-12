#pragma once

#include "enemies/EnemyCollision.hpp"
#include "graphics/Renderer.hpp"

#include <array>
#include <iostream>

inline bool checkEnemyCylinderModels(ian::Renderer& renderer) {
    using namespace ian;
    constexpr std::array<EnemyModelVisual,10> models{EnemyModelVisual::Minion,EnemyModelVisual::Rogue,
        EnemyModelVisual::Warrior,EnemyModelVisual::Boss,EnemyModelVisual::Mage,EnemyModelVisual::Sapper,
        EnemyModelVisual::Flying,EnemyModelVisual::Splitter,EnemyModelVisual::Splitling,EnemyModelVisual::BossSummon};
    constexpr std::array<float,10> scales{.94F,.55F,.67F,1.35F,.94F,.52F,.52F,.58F,.60F,.52F};
    const auto target=LoadRenderTexture(256,256);
    const Camera3D camera{{5,3.2F,7},{0,2.2F,0},{0,1,0},6.5F,CAMERA_ORTHOGRAPHIC};
    bool valid=true;
    for(std::size_t type=0;type<models.size();++type) {
        EnemyInstance enemy{};enemy.type=type==9 ? EnemyType::Flying : static_cast<EnemyType>(type);
        if(type==9) enemy.summonerId=EntityId{1,1};
        const auto shape=enemyCollisionCylinder(enemy);
        const auto capture=[&](bool cylinder,EnemyAnimationVisual animation,float time) {
            BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);
            if(cylinder) DrawCylinderEx({0,static_cast<float>(shape.center.y-shape.halfHeight),0},
                {0,static_cast<float>(shape.center.y+shape.halfHeight),0},
                static_cast<float>(shape.radius),static_cast<float>(shape.radius),64,WHITE);
            else {
                WorldLighting lighting{};lighting.cameraPosition=camera.position;
                renderer.beginWorldShader(lighting);
                valid &= renderer.drawEnemy(models[type],animation,time,
                    {0,type==3?static_cast<float>(AlpakingHover):(type==6 || type==9)?1.25F:.02F,0},0,WHITE,scales[type]);
                renderer.endWorldShader();
            }
            // Hide authored foot triangles below the actual walkable surface.
            DrawPlane({0,0,0},{20,20},BLACK);
            EndMode3D();EndTextureMode();
            Image image=LoadImageFromTexture(target.texture);
            return image;
        };
        Image cylinder=capture(true,EnemyAnimationVisual::Idle,0);
        Color* mask=LoadImageColors(cylinder);
        for(const auto animation:{EnemyAnimationVisual::Idle,EnemyAnimationVisual::Walk,
                                  EnemyAnimationVisual::MeleeAttack,EnemyAnimationVisual::Hit}) {
            for(const float time:{0.0F,.23F,.5F,.85F}) {
            Image pose=capture(false,animation,time);Color* pixels=LoadImageColors(pose);
            int outside=0,visible=0;
            for(int i=0;i<256*256;++i) {
                if(pixels[i].r+pixels[i].g+pixels[i].b<20) continue;
                ++visible;
                if(mask[i].r+mask[i].g+mask[i].b<20) ++outside;
            }
            if(visible<100 || outside>20) {
                std::cerr<<"Enemy cylinder silhouette type/pose/outside/visible: "<<type<<' '
                         <<static_cast<int>(animation)<<' '<<time<<' '<<outside<<' '<<visible<<'\n';valid=false;
            }
            UnloadImageColors(pixels);UnloadImage(pose);
            }
        }
        UnloadImageColors(mask);UnloadImage(cylinder);
    }
    UnloadRenderTexture(target);return valid;
}
