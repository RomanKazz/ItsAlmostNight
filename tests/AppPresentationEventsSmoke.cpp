#include "app/App.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <rlgl.h>

namespace ian {
struct AppPresentationTestAccess {
    static int run() {
        auto app=std::make_unique<App>();
        SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_HIGHDPI);
        InitWindow(800,600,"Presentation event integration");
        if(!IsWindowReady()) return 77;
        app->renderer_.emplace();app->renderer_->initialize();
        SimulationSnapshot snapshot=app->simulation_.snapshot();
        const std::array events{
            GameEvent{.type=GameEventType::ChainLightningHit,.entityId=EntityId{810,1},.sourceId=EntityId{990,1},
                .buildingType=BuildingType::TeslaTower,.position={0,1.26,0},.targetPosition=Vec3{1.4,.7,-2},.secondaryImpact=true},
            GameEvent{.type=GameEventType::ChainLightningHit,.entityId=EntityId{811,1},
                .position={1.4,.7,-2},.targetPosition=Vec3{-1.4,.7,-2},.amount=1,.damage=1,.secondaryImpact=true},
            GameEvent{.type=GameEventType::MissileExplosion,.position={0,0,0},.intensity=1.6},
            GameEvent{.type=GameEventType::SawSplinterLaunched,.position={0,1,0},.targetPosition=Vec3{1,1,1}},
            GameEvent{.type=GameEventType::BossRoyalImpact,.position={3,0,0},.intensity=1}};
        app->processPresentationEvents(events,snapshot);
        const auto count=[&](PresentationEffectType type){return std::ranges::count(app->effects_,type,&PresentationEffect::type);};
        bool okay=count(PresentationEffectType::ChainLightning)==2 && count(PresentationEffectType::Explosion)==1 &&
            count(PresentationEffectType::SawSplinter)==1 && count(PresentationEffectType::RoyalImpact)==1;
        if(!okay) std::cerr<<"Gameplay events did not queue their own independent presentation effects\n";
        std::erase_if(app->effects_,[](const auto& effect){return effect.type!=PresentationEffectType::ChainLightning;});
        const Camera3D camera{{0,2.3F,-6},{0,.9F,-1},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
        const auto target=LoadRenderTexture(800,600);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;
        int visible[2]{};
        for(int frame=0;frame<2;++frame) {
            for(auto& effect:app->effects_){effect.startDelayRemaining=0;effect.remaining=effect.duration-(frame==0?.08:.16);}
            app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
            app->drawPresentationEffects(camera,lighting);EndMode3D();EndTextureMode();
            Image image=LoadImageFromTexture(target.texture);auto* pixels=LoadImageColors(image);
            for(int i=0;i<image.width*image.height;++i) visible[frame]+=pixels[i].b>160 && pixels[i].g>100;
            if(const char* capture=std::getenv("IAN_TESLA_CAPTURE")) {
                ImageFlipVertical(&image);std::filesystem::create_directories(capture);
                const auto file=std::filesystem::path(capture)/("gameplay-lightning-"+std::to_string(frame)+".png");
                ExportImage(image,file.string().c_str());
            }
            UnloadImageColors(pixels);UnloadImage(image);
        }
        okay &= visible[0]>300 && visible[1]>300;
        if(visible[0]<=300 || visible[1]<=300) std::cerr<<"Queued gameplay lightning is invisible in App renderer\n";
        // Duplicate discharges must stay bounded, alive and animated.
        for(int i=0;i<200;++i)app->processPresentationEvents({events.data(),1},snapshot);
        okay &= count(PresentationEffectType::ChainLightning)==2;
        // Exercise the actual App effect pass through postprocessing. Rear ink must
        // not darken the opaque electrical core where it crosses prop silhouettes.
        app->renderer_->settings().postProcessing=true;
        app->renderer_->settings().ssao=true;
        app->renderer_->settings().bloom=false;
        app->renderer_->settings().outlineWidth=3;
        app->renderer_->settings().outlineStrength=1;
        const Camera3D inkCamera{{0,1,-6},{0,1,-1},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
        int bright[2]{};
        for(int ink=0;ink<2;++ink) {
            app->renderer_->settings().inkOutlines=ink!=0;
            for(auto& effect:app->effects_)effect.remaining=effect.duration-.08;
            app->renderer_->beginFrame();app->renderer_->beginWorldPass({29,47,57,255},inkCamera);
            BeginMode3D(inkCamera);app->renderer_->beginWorldShader(lighting);
            WorldMaterialState prop{};prop.unlitAmount=1;app->renderer_->setWorldMaterial(prop);
            DrawCube({-.25F,.9F,-.5F},.50F,2.6F,.2F,{90,105,110,255});
            app->renderer_->endWorldShader();app->drawPresentationEffects(inkCamera,lighting);EndMode3D();
            app->renderer_->endWorldPass();rlDrawRenderBatchActive();
            Image image=LoadImageFromScreen();auto* pixels=LoadImageColors(image);
            const auto center=GetWorldToScreenEx({0,.7F,-2},inkCamera,image.width,image.height);
            // Probe four columns across the silhouette: the arc uses a live
            // animation clock, and a two-column sliver landed exactly on the
            // ten-pixel cutoff for otherwise fully visible electrical cores.
            for(int y=int(center.y)-100;y<=int(center.y)+100;++y)for(int x=int(center.x);x<=int(center.x)+3;++x) {
                const auto pixel=pixels[y*image.width+x];bright[ink]+=pixel.r>180 && pixel.g>200 && pixel.b>200;
            }
            if(const char* capture=std::getenv("IAN_TESLA_CAPTURE")) {
                const auto file=std::filesystem::path(capture)/(ink?"lightning-ink-on.png":"lightning-ink-off.png");
                ExportImage(image,file.string().c_str());
            }
            UnloadImageColors(pixels);UnloadImage(image);app->renderer_->endFrame();
        }
        okay &= bright[0]>10 && bright[1]>bright[0]*.65;
        if(!(bright[0]>10 && bright[1]>bright[0]*.65))
            std::cerr<<"Rear world ink covers electrical core: "<<bright[0]<<" / "<<bright[1]<<"\n";
        UnloadRenderTexture(target);app->renderer_.reset();app.reset();CloseWindow();
        return okay?0:1;
    }
};
int runPresentationEventsSmoke() {
    const auto original=std::filesystem::current_path();
    const auto sandbox=std::filesystem::temp_directory_path()/"ian-presentation-events-smoke";
    std::filesystem::create_directories(sandbox);
    if(!std::filesystem::exists(sandbox/"assets"))std::filesystem::create_directory_symlink(original/"assets",sandbox/"assets");
    std::filesystem::current_path(sandbox);
    const int result=AppPresentationTestAccess::run();
    std::filesystem::current_path(original);return result;
}
}
