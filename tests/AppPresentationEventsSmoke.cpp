#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
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
        bool wandEvents = true;
        for (const auto charge : {GameEventType::IceWandChargeStarted, GameEventType::FireWandChargeStarted}) {
            app->toolSwingRemaining_ = .3;
            app->iceWandRecoilRemaining_ = .1;
            const std::array start{GameEvent{.type=charge}};
            app->processPresentationEvents(start, snapshot);
            wandEvents &= app->toolSwingRemaining_ == 0 && app->iceWandRecoilRemaining_ == 0;
            const std::array shot{GameEvent{.type=charge == GameEventType::IceWandChargeStarted
                ? GameEventType::IceWandFired : GameEventType::FireWandFired}};
            app->processPresentationEvents(shot, snapshot);
            wandEvents &= app->iceWandRecoilRemaining_ == app->iceWandRecoilDuration_ &&
                app->iceWandRecoilDuration_ >= .26;
        }
        auto fastSnapshot = snapshot;
        fastSnapshot.fireWandCooldownRemaining = .06;
        const std::array fastCast{GameEvent{.type=GameEventType::FireWandFired}};
        app->processPresentationEvents(fastCast, fastSnapshot);
        wandEvents &= std::abs(app->iceWandRecoilDuration_ - .06) < 1e-6;
        if (!wandEvents) std::cerr << "Wand events did not drive independent charge/release presentation\n";
        const auto count=[&](PresentationEffectType type){return std::ranges::count(app->effects_,type,&PresentationEffect::type);};
        bool okay=wandEvents && count(PresentationEffectType::ChainLightning)==2 && count(PresentationEffectType::Explosion)==1 &&
            count(PresentationEffectType::SawSplinter)==1 && count(PresentationEffectType::RoyalImpact)==1;
        if(!okay) std::cerr<<"Gameplay events did not queue their own independent presentation effects\n";
        // Sustained hits share one per-enemy clock across damage sources.
        const EntityId flashEnemy{58100,1}, otherEnemy{58101,1};
        const auto flashKey = [](EntityId id) { return (static_cast<std::uint64_t>(id.generation)<<32U) | id.index; };
        app->startEnemyHitFlash(flashEnemy);
        const auto firstFlash = app->enemyHitFlashById_.at(flashKey(flashEnemy));
        const std::array repeatedHits{
            GameEvent{.type=GameEventType::ProjectileHit,.entityId=flashEnemy,.damage=1},
            GameEvent{.type=GameEventType::PoisonHit,.entityId=flashEnemy,.damage=1},
            GameEvent{.type=GameEventType::CannonHit,.entityId=flashEnemy,.damage=1}};
        for(int hit=0;hit<30;++hit) app->processPresentationEvents(repeatedHits,snapshot);
        app->startEnemyHitFlash(otherEnemy);
        bool flashLimit = std::abs(firstFlash.nextAllowedAt-firstFlash.startedAt-1.0/6.0)<1e-9 &&
            app->enemyHitFlashById_.at(flashKey(flashEnemy)).startedAt==firstFlash.startedAt &&
            app->enemyHitFlashById_.contains(flashKey(otherEnemy));
        // Simulate a frame in the dark gap: expired visuals must not erase the
        // still-active throttle. This is what the old duration-based cleanup did.
        auto& cooldownFlash=app->enemyHitFlashById_.at(flashKey(flashEnemy));
        cooldownFlash.startedAt=GetTime()-.12;
        cooldownFlash.nextAllowedAt=GetTime()+.06;
        auto emptySnapshot=snapshot; emptySnapshot.enemies={};
        const Camera3D flashCamera{{0,2,-6},{0,1,0},{0,1,0},60,CAMERA_PERSPECTIVE};
        BeginDrawing(); BeginMode3D(flashCamera);
        app->drawWorldEnemies(emptySnapshot,flashCamera);
        EndMode3D(); EndDrawing();
        flashLimit &= app->enemyHitFlashById_.contains(flashKey(flashEnemy));
        if(app->enemyHitFlashById_.contains(flashKey(flashEnemy))) {
            const double heldStart=app->enemyHitFlashById_.at(flashKey(flashEnemy)).startedAt;
            app->startEnemyHitFlash(flashEnemy);
            flashLimit &= app->enemyHitFlashById_.at(flashKey(flashEnemy)).startedAt==heldStart;
            app->enemyHitFlashById_.at(flashKey(flashEnemy)).nextAllowedAt=GetTime()-.01;
            app->startEnemyHitFlash(flashEnemy);
            flashLimit &= app->enemyHitFlashById_.at(flashKey(flashEnemy)).startedAt>heldStart;
        }
        okay &= flashLimit;
        if(!flashLimit) std::cerr<<"Enemy damage flashes exceeded six starts per second or lost their cooldown during cleanup\n";
        app->effects_.clear();
        app->processPresentationEvents(events,snapshot);
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
        // Exercise the actual placement pass, not just the model renderer.
        // A horizontal camera cleanly separates the fan (<1.4m) from the old
        // 2m gate fallback which previously appeared over its preview.
        app->effects_.clear();
        app->actionMode_=ActionMode::Buildings;
        app->placementPreviewEntranceSeconds_=1;
        app->gameplayCamera_={{0,1.5F,-4},{0,1.5F,0},{0,1,0},3,CAMERA_ORTHOGRAPHIC};
        snapshot.playerPosition={10,1.7,-10};
        snapshot.buildingPreview=BuildingPreview{.type=BuildingType::WindBlower,.gridPosition={0,0},.rotation=0,.placement={}};
        app->renderer_->settings().postProcessing=false;
        int upperInk=0,lowerInk=0;
        for (int rotation=0;rotation<2;++rotation) {
            app->placementRotationYaw_=static_cast<double>(rotation)*1.5707963267948966;
            snapshot.buildingPreview->rotation=static_cast<std::uint8_t>(rotation*2);
            app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});
            BeginMode3D(app->gameplayCamera_);app->renderer_->beginWorldShader(lighting);
            app->drawWorldOverlays(snapshot,lighting);app->renderer_->endWorldShader();
            EndMode3D();EndTextureMode();
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            auto* pixels=LoadImageColors(image);
            for(int y=0;y<600;++y)for(int x=250;x<550;++x) {
                const auto p=pixels[y*800+x];
                const bool drawn=std::abs(int(p.r)-29)+std::abs(int(p.g)-47)+std::abs(int(p.b)-57)>24;
                if(y<300)upperInk+=drawn;else lowerInk+=drawn;
            }
            if(const char* capture=std::getenv("IAN_WIND_CAPTURE")) {
                std::filesystem::create_directories(capture);
                const auto path=std::filesystem::path(capture)/("wind-preview-"+std::to_string(rotation)+".png");ExportImage(image,path.string().c_str());
            }
            UnloadImageColors(pixels);UnloadImage(image);
        }
        if(upperInk>20 || lowerInk<100) {
            std::cerr<<"Wind preview contains legacy gate geometry or is invisible: "<<upperInk<<" / "<<lowerInk<<"\n";okay=false;
        }
        // Exercise the actual burn pass on every airborne rig: their roots
        // are below the native body, so a root-anchored flame floats underneath.
        const Camera3D burnCamera{{0,2.4F,7},{0,2.4F,0},{0,1,0},6,CAMERA_ORTHOGRAPHIC};
        for (const auto type : {EnemyType::Basic,EnemyType::Flying,EnemyType::Bee,EnemyType::AirTank,EnemyType::Boss}) {
            std::array<EnemyInstance,1> enemies{};
            auto& enemy = enemies[0];
            enemy.id = {900+static_cast<std::uint32_t>(type),1};
            enemy.type = type; enemy.active = true; enemy.health = enemy.maxHealth = 100;
            enemy.state = EnemyState::MoveToCore;
            enemy.bossHoverOffset = .6;
            snapshot.enemies = enemies;
            snapshot.elapsedSeconds = .31;
            app->effects_.clear();
            app->effects_.push_back({.type=PresentationEffectType::EnemyBurn,.entityId=enemy.id,
                .remaining=1,.duration=1});
            app->renderer_->beginFrame();
            BeginTextureMode(target);ClearBackground({29,47,57,255});
            BeginMode3D(burnCamera);app->renderer_->beginWorldShader(lighting);
            app->drawWorldEnemies(snapshot,burnCamera);
            app->renderer_->endWorldShader();EndMode3D();EndTextureMode();
            auto root = app_detail::enemyRenderPosition(enemy);
            root.y += static_cast<float>(app->simulation_.terrain().getHeight(0,0));
            const auto bounds = app->renderer_->enemyWorldBounds(app_detail::enemyModelVisual(enemy),
                root,0,app_detail::enemyVisualScale(type));
            const float expected = enemyIsFlying(type) || type==EnemyType::Boss ? bounds.min.y : root.y;
            const bool attached = app->burnVisualCandidates_.size()==1 &&
                std::abs(app->burnVisualCandidates_[0].position.y-expected)<.01F;
            if (!attached) { std::cerr<<"Burn detached from native body: "<<static_cast<int>(type)<<'\n';okay=false; }
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            int orange=0;
            for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x) {
                const auto p=GetImageColor(image,x,y);
                orange += p.r>210 && p.g>60 && p.g<210 && p.b<80;
            }
            if(orange<100){std::cerr<<"Burn invisible on rig "<<static_cast<int>(type)<<'\n';okay=false;}
            if(const char* capture=std::getenv("IAN_AIR_FIRE_CAPTURE")) {
                std::filesystem::create_directories(capture);
                ExportImage(image,(std::filesystem::path(capture)/("burn-"+std::to_string(static_cast<int>(type))+".png")).string().c_str());
            }
            UnloadImage(image);
        }
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
