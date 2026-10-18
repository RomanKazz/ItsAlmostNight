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
        // The real App shares one selection path across buildings and tools.
        app->simulation_.startRun();
        PlayerCommand godmode;
        godmode.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        app->simulation_.tick(1.0 / 60.0, godmode);
        auto buildSnapshot = app->simulation_.snapshot();
        buildSnapshot.coreMaxHealth = 500;
        buildSnapshot.coreLevel = 1;
        buildSnapshot.unlockedBuildings.fill(true);
        const auto catalog = makeConstructionHotbarLayout(buildSnapshot.unlockedBuildings, true, 1, true);
        app->actionMode_ = ActionMode::Buildings;
        app->selectConstructionHotbarItem(buildSnapshot, *catalog.indexOf(ModularBuildPiece::Foundation));
        app->selectNextActionModeItem(buildSnapshot);
        bool construction = app->foundationBuildMode_ && app->modularBuildPiece_ == ModularBuildPiece::Foundation;
        app->selectNextBuildingItem(buildSnapshot);
        construction &= app->modularBuildPiece_ == ModularBuildPiece::Ramp;
        app->selectNextBuildingItem(buildSnapshot);
        construction &= !app->foundationBuildMode_ && !app->pendingBuildingCancel_ &&
            app->pendingBuildingSelection_ == BuildingType::GunTurret;
        app->selectNextBuildingItem(buildSnapshot, -1);
        construction &= app->foundationBuildMode_ && app->modularBuildPiece_ == ModularBuildPiece::Ramp;
        if (!construction) std::cerr << "Construction selection failed\n";
        // Verify context at the actual preview/drag boundary using a live frame.
        const auto frame = app->simulation_.placeFoundation({.2,
            app->simulation_.terrain().getHeight(.2, 4.2), 4.2});
        construction &= frame.has_value();
        if (frame) {
            buildSnapshot = app->simulation_.snapshot();
            const double cell = app->simulation_.terrain().config().cellSize;
            const Vec3 center{(frame->anchor.x + 1.0) * cell, frame->floorHeight,
                (frame->anchor.z + 1.0) * cell};
            buildSnapshot.playerPosition = {center.x, center.y + 3, center.z + 3};
            buildSnapshot.playerYaw = 0;
            buildSnapshot.playerPitch = -std::atan2(3.0, 3.0);
            buildSnapshot.aimedModularBuilding = frame->id;
            buildSnapshot.aimedModularBuildingCandidate = frame->id;
            app->selectModularBuildPiece(ModularBuildPiece::Foundation);
            app->updateModularPlacementPreview(buildSnapshot);
            construction &= app->modularBuildPiece_ == ModularBuildPiece::FloorPlatform &&
                app->platformFramePreview_ && app->platformFramePreview_->valid() &&
                app->platformFramePreview_->anchor.x == frame->anchor.x &&
                app->platformFramePreview_->anchor.z == frame->anchor.z &&
                app->platformFramePreview_->storey == 1;
            if (!construction) std::cerr << "Platform center did not propose a supported upper floor\n";
            app->beginModularPlacementDrag();
            construction &= app->modularDragPiece_ == ModularBuildPiece::FloorPlatform;
            buildSnapshot.aimedModularBuilding.reset();
            buildSnapshot.aimedModularBuildingCandidate.reset();
            app->updateModularPlacementPreview(buildSnapshot);
            construction &= app->modularBuildPiece_ == ModularBuildPiece::FloorPlatform &&
                app->modularDragStorey_ == 1;
            if (!construction) std::cerr << "Platform drag failed\n";
            const bool floorPlaced = app->finishModularPlacementDrag();
            construction &= floorPlaced;
            if (!floorPlaced) std::cerr << "Platform drag failed to commit the upper floor\n";
            buildSnapshot = app->simulation_.snapshot();
            buildSnapshot.aimedModularBuilding.reset();
            buildSnapshot.aimedModularBuildingCandidate.reset();
            app->updateModularPlacementPreview(buildSnapshot);
            construction &= app->modularBuildPiece_ == ModularBuildPiece::Foundation &&
                app->modularAutomaticFoundationPreviews_.empty();
            if (!construction) std::cerr << "Platform return to ground failed\n";
            // Aiming at the ground frame's outer edge extends its ground row.
            buildSnapshot.playerPosition = {center.x, center.y + 3, center.z + 3};
            const double edgeZ = frame->anchor.z * cell + .04 * cell;
            buildSnapshot.playerPitch = -std::atan2(3.0, buildSnapshot.playerPosition.z - edgeZ);
            buildSnapshot.aimedModularBuilding = frame->id;
            buildSnapshot.aimedModularBuildingCandidate = frame->id;
            app->updateModularPlacementPreview(buildSnapshot);
            construction &= app->modularBuildPiece_ == ModularBuildPiece::Foundation &&
                app->platformFramePreview_ && app->platformFramePreview_->storey == 0 &&
                app->platformFramePreview_->anchor != frame->anchor;
            if (!construction) std::cerr << "Platform edge did not extend the ground row\n";
            const auto upper = std::ranges::find_if(buildSnapshot.platformFrames,
                [](const PlatformFrameInstance& candidate) { return candidate.storey == 1; });
            construction &= upper != buildSnapshot.platformFrames.end();
            if (upper != buildSnapshot.platformFrames.end()) {
                buildSnapshot.playerPosition = {center.x, upper->floorHeight + 3, center.z + 3};
                buildSnapshot.playerPitch = -std::atan2(3.0, 3.0);
                buildSnapshot.aimedModularBuilding = upper->id;
                buildSnapshot.aimedModularBuildingCandidate = upper->id;
                app->updateModularPlacementPreview(buildSnapshot);
                construction &= app->modularBuildPiece_ == ModularBuildPiece::FloorPlatform &&
                    app->platformFramePreview_ && app->platformFramePreview_->storey == 2;
                buildSnapshot.playerPitch = -std::atan2(3.0, buildSnapshot.playerPosition.z - edgeZ);
                app->updateModularPlacementPreview(buildSnapshot);
                construction &= app->modularBuildPiece_ == ModularBuildPiece::FloorPlatform &&
                    app->platformFramePreview_ && app->platformFramePreview_->storey == 1 &&
                    app->platformFramePreview_->anchor.z != upper->anchor.z;
            }
        }
        okay &= construction;
        if (!construction) std::cerr << "Unified construction selection or contextual platform/drag failed\n";
        // Continuous deck aiming: ignore stale broad candidates, cover mesh
        // seams, and never extend a storey into its existing neighbour.
        bool deckAim = frame.has_value();
        if (frame) {
            const double cell = app->simulation_.terrain().config().cellSize;
            const double storeyHeight = modularStoreyHeight(app->simulation_.terrain().config());
            GridCoord neighborAnchor{};
            std::optional<PlatformFrameInstance> neighborBase, neighborTop;
            for (const auto delta : {GridCoord{2,0,0},GridCoord{-2,0,0},
                                      GridCoord{0,0,2},GridCoord{0,0,-2}}) {
                neighborAnchor={frame->anchor.x+delta.x,0,frame->anchor.z+delta.z};
                const Vec3 hit{(neighborAnchor.x+.5)*cell,frame->floorHeight,
                    (neighborAnchor.z+.5)*cell};
                if (app->simulation_.previewFoundationAtHeight(hit,frame->floorHeight).valid()) {
                    auto groundAim=app->simulation_.snapshot();
                    groundAim.playerPosition={(frame->anchor.x+1.0+delta.x*.505)*cell,
                        frame->floorHeight+3,(frame->anchor.z+1.0+delta.z*.505)*cell};
                    groundAim.playerYaw=0;groundAim.playerPitch=-PI*.5;
                    groundAim.aimedModularBuilding.reset();groundAim.aimedModularBuildingCandidate.reset();
                    app->clearModularPlacementDrag();
                    app->selectModularBuildPiece(ModularBuildPiece::Foundation);
                    app->updateModularPlacementPreview(groundAim);
                    const bool groundMagnet=app->platformFramePreview_ && app->platformFramePreview_->valid() &&
                        app->platformFramePreview_->storey==0 &&
                        app->platformFramePreview_->anchor.x==neighborAnchor.x &&
                        app->platformFramePreview_->anchor.z==neighborAnchor.z;
                    deckAim &= groundMagnet;
                    if (!groundMagnet) std::cerr << "Ground edge did not snap past the half-overlapping occupied cell\n";
                }
                neighborBase=app->simulation_.placeFoundationAtHeight(hit,frame->floorHeight);
                if (neighborBase) {
                    neighborTop=app->simulation_.placeFloorPlatform(
                        neighborAnchor,1,frame->floorHeight+storeyHeight);
                    break;
                }
            }
            deckAim &= neighborBase.has_value() && neighborTop.has_value();
            if (!neighborBase || !neighborTop) std::cerr << "Deck fixture neighbour failed " << bool(neighborBase) << '/' << bool(neighborTop) << '\n';
            auto deckSnapshot=app->simulation_.snapshot();
            app->clearModularPlacementDrag();
            app->selectModularBuildPiece(ModularBuildPiece::Foundation);
            // Every sample aims straight down through the deck. The candidate
            // deliberately names the lower, wrong frame, as it did at mesh gaps.
            for (double offset : {.65,1.0,1.49,1.51,1.99,2.0,2.01,2.49,2.51,3.0,3.35}) {
                const bool alongX=neighborAnchor.x!=frame->anchor.x;
                deckSnapshot.playerPosition={
                    (alongX?std::min(frame->anchor.x,neighborAnchor.x)+offset:frame->anchor.x+1.0)*cell,
                    frame->floorHeight+storeyHeight+3,
                    (alongX?frame->anchor.z+1.0:std::min(frame->anchor.z,neighborAnchor.z)+offset)*cell};
                deckSnapshot.playerYaw=0;deckSnapshot.playerPitch=-PI*.5;
                deckSnapshot.aimedModularBuilding.reset();
                deckSnapshot.aimedModularBuildingCandidate=frame->id;
                app->updateModularPlacementPreview(deckSnapshot);
                const bool valid=app->modularBuildPiece_==ModularBuildPiece::FloorPlatform &&
                    app->platformFramePreview_ && app->platformFramePreview_->valid() &&
                    app->platformFramePreview_->storey==2 &&
                    app->platformFramePreview_->anchor.x>=std::min(frame->anchor.x,neighborAnchor.x) &&
                    app->platformFramePreview_->anchor.x<=std::max(frame->anchor.x,neighborAnchor.x) &&
                    app->platformFramePreview_->anchor.z>=std::min(frame->anchor.z,neighborAnchor.z) &&
                    app->platformFramePreview_->anchor.z<=std::max(frame->anchor.z,neighborAnchor.z);
                deckAim &= valid;
                if (!valid) std::cerr << "Deck aim failed at offset " << offset << " error " <<
                    (app->platformFramePreview_?int(app->platformFramePreview_->error):-1) << " anchor " << (app->platformFramePreview_?app->platformFramePreview_->anchor.x:-999) << '/' << (app->platformFramePreview_?app->platformFramePreview_->anchor.z:-999) << " level " << (app->platformFramePreview_?app->platformFramePreview_->storey:-1) << '\n';
            }
            if (neighborTop) {
                // A genuinely exposed edge still extends the same storey.
                deckSnapshot.playerPosition={(neighborAnchor.x+1)*cell,
                    neighborTop->floorHeight+3,
                    (neighborAnchor.z+(neighborAnchor.z>frame->anchor.z?1.95:.05))*cell};
                app->updateModularPlacementPreview(deckSnapshot);
                deckAim &= app->platformFramePreview_ && app->platformFramePreview_->storey==1 &&
                    app->platformFramePreview_->anchor.z!=neighborAnchor.z;
            }
            if (neighborTop) {
                const auto occupiedUpper=app->simulation_.placeFloorPlatform(
                    frame->anchor,2,frame->floorHeight+2*storeyHeight);
                deckAim &= occupiedUpper.has_value();
                deckSnapshot=app->simulation_.snapshot();
                const bool alongX=neighborAnchor.x!=frame->anchor.x;
                // Aim from below the new upper floor at a half-overlapping
                // footprint. The complete neighbouring footprint is free.
                const double direction=alongX?(neighborAnchor.x-frame->anchor.x)/2.0
                    :(neighborAnchor.z-frame->anchor.z)/2.0;
                deckSnapshot.playerPosition={
                    (alongX?frame->anchor.x+1.0+direction*1.10:frame->anchor.x+1.0)*cell,
                    frame->floorHeight+storeyHeight+2,
                    (alongX?frame->anchor.z+1.0:frame->anchor.z+1.0+direction*1.10)*cell};
                deckSnapshot.playerYaw=0;deckSnapshot.playerPitch=-PI*.5;
                app->updateModularPlacementPreview(deckSnapshot);
                const bool magnet=app->platformFramePreview_ && app->platformFramePreview_->valid() &&
                    app->platformFramePreview_->storey==2 &&
                    app->platformFramePreview_->anchor.x==neighborAnchor.x &&
                    app->platformFramePreview_->anchor.z==neighborAnchor.z &&
                    std::abs(app->platformFramePreview_->floorHeight-frame->floorHeight-2*storeyHeight)<1e-6;
                deckAim &= magnet;
                if (!magnet) std::cerr << "Occupied floor did not magnetize to the free neighbour on the same storey\n";
                if (magnet) {
                    app->beginModularPlacementDrag();
                    deckAim &= app->modularDragStart_ &&
                        app->modularDragStart_->x==neighborAnchor.x &&
                        app->modularDragStart_->z==neighborAnchor.z &&
                        app->modularDragStorey_==2;
                    deckAim &= app->finishModularPlacementDrag();
                    const auto placed=app->simulation_.snapshot();
                    deckAim &= std::ranges::any_of(placed.platformFrames,[&](const auto& candidate) {
                        return candidate.storey==2 && candidate.anchor.x==neighborAnchor.x &&
                            candidate.anchor.z==neighborAnchor.z;
                    });
                    app->clearModularPlacementDrag();
                    // A successful placement must not instantly propose a
                    // third storey at the unchanged aim. Small jitter is safe;
                    // looking elsewhere or reselecting explicitly re-arms it.
                    const auto blocked=app->modularFloorPreviewBlockedAim_;
                    bool rearm=blocked.has_value() && !app->platformFramePreview_;
                    if (blocked) {
                        auto unchanged=app->simulation_.snapshot();
                        unchanged.playerPosition={blocked->x,blocked->y+3,blocked->z};
                        unchanged.playerYaw=0;unchanged.playerPitch=-PI*.5;
                        unchanged.aimedModularBuilding.reset();
                        unchanged.aimedModularBuildingCandidate.reset();
                        for (double jitter : {0.0,.08,-.08,0.0}) {
                            unchanged.playerPosition.x=blocked->x+jitter;
                            app->updateModularPlacementPreview(unchanged);
                            rearm &= !app->platformFramePreview_ && !app->foundationTerrainHit_ &&
                                app->modularFloorPreviewBlockedAim_.has_value();
                        }
                        unchanged.playerPosition.x=blocked->x+.8;
                        app->updateModularPlacementPreview(unchanged);
                        rearm &= !app->modularFloorPreviewBlockedAim_;
                        unchanged.playerPosition.x=blocked->x;
                        app->updateModularPlacementPreview(unchanged);
                        rearm &= app->platformFramePreview_ && app->platformFramePreview_->valid() &&
                            app->platformFramePreview_->storey==3;
                        app->modularFloorPreviewBlockedAim_=*blocked;
                        app->selectModularBuildPiece(ModularBuildPiece::Foundation);
                        app->updateModularPlacementPreview(unchanged);
                        rearm &= !app->modularFloorPreviewBlockedAim_ && app->platformFramePreview_ &&
                            app->platformFramePreview_->storey==3;
                        app->clearModularPlacementDrag();
                        rearm &= !app->finishModularPlacementDrag() && !app->modularFloorPreviewBlockedAim_;
                    }
                    deckAim &= rearm;
                    if (!rearm) std::cerr << "Placement instantly proposed another storey or failed to re-arm after retargeting\n";
                }
            }
        }
        okay &= deckAim;
        if (!deckAim) std::cerr << "Continuous platform targeting or exposed perimeter failed\n";
        // An occupied footprint follows the same interpolated visual origin;
        // validity changes must not teleport it back to the snapped grid cell.
        const Camera3D footprintCamera{{2,15,1},{2,0,1},{0,0,-1},8,CAMERA_ORTHOGRAPHIC};
        const PlatformFramePlacement occupiedFloor{.error=ModularPlacementError::Occupied,
            .anchor={0,0,0},.floorHeight=0,.storey=1};
        std::array<double,3> footprintCenter{};
        for (std::size_t step=0;step<footprintCenter.size();++step) {
            const Vec3 visual{1.0+static_cast<double>(step),0,1};
            lighting.cameraPosition=footprintCamera.position;
            app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});
            BeginMode3D(footprintCamera);app->renderer_->beginWorldShader(lighting);
            WorldMaterialState footprintMaterial{};footprintMaterial.unlitAmount=1;
            app->renderer_->setWorldMaterial(footprintMaterial);
            app->modularBuildingRenderer_.drawWorld({{},{},{},{},1},
                {.platformFrame=&occupiedFloor,.visualOrigin=&visual});
            app->renderer_->endWorldShader();EndMode3D();EndTextureMode();
            Image capture=LoadImageFromTexture(target.texture);
            auto* pixels=LoadImageColors(capture);int samples=0;double sumX=0;
            for (int y=0;y<capture.height;++y) for(int x=0;x<capture.width;++x) {
                const auto pixel=pixels[y*capture.width+x];
                if (pixel.r>100 && pixel.r>pixel.g*1.5) { ++samples;sumX+=x; }
            }
            footprintCenter[step]=samples>0?sumX/samples:-1000;
            UnloadImageColors(pixels);UnloadImage(capture);
        }
        const bool footprintSlide=footprintCenter[0]>0 &&
            std::abs(footprintCenter[1]-footprintCenter[0])>40 &&
            std::abs(footprintCenter[2]-2*footprintCenter[1]+footprintCenter[0])<2;
        okay &= footprintSlide;
        if (!footprintSlide) std::cerr << "Occupied floor preview skipped smooth visual origin\n";
        // Construction range also appears while inspecting Core with equipment.
        app->setFoundationBuildMode(false);
        app->actionMode_ = ActionMode::Equipment;
        auto rangeSnapshot = app->simulation_.snapshot();
        const std::array rangeBuildings{
            BuildingInstance{.id={68100,1}, .type=BuildingType::Core,
                .gridPosition={0,0}, .health=500, .maxHealth=500},
            BuildingInstance{.id={68101,1}, .type=BuildingType::Wall,
                .gridPosition={2,0}, .health=100, .maxHealth=100}};
        rangeSnapshot.buildings = rangeBuildings;
        rangeSnapshot.coreId = rangeBuildings[0].id;
        rangeSnapshot.coreBuildRadius = 4;
        rangeSnapshot.selectedBuilding.reset();
        rangeSnapshot.buildingPreview.reset();
        const float ground = static_cast<float>(app->simulation_.terrain().getHeight(0,0));
        const Camera3D rangeCamera{{0,ground+20,0},{0,ground,0},{0,0,-1},12,CAMERA_ORTHOGRAPHIC};
        std::array<int,3> rangeInk{};
        for (std::size_t hover = 0; hover < rangeInk.size(); ++hover) {
            rangeSnapshot.aimedBuilding = hover == 0 ? std::nullopt
                : std::optional<EntityId>{rangeBuildings[hover == 1 ? 1 : 0].id};
            app->renderer_->beginFrame();
            BeginTextureMode(target);ClearBackground({29,47,57,255});
            BeginMode3D(rangeCamera);app->renderer_->beginWorldShader(lighting);
            app->drawWorldOverlays(rangeSnapshot,lighting);
            app->renderer_->endWorldShader();EndMode3D();EndTextureMode();
            Image capture=LoadImageFromTexture(target.texture);
            auto* pixels=LoadImageColors(capture);
            for(int i=0;i<capture.width*capture.height;++i) {
                const auto pixel=pixels[i];
                rangeInk[hover] += std::abs(int(pixel.r)-29)+std::abs(int(pixel.g)-47)+std::abs(int(pixel.b)-57)>24;
            }
            if (hover == 2) {
                if (const char* directory = std::getenv("IAN_BUILD_RADIUS_CAPTURE")) {
                    std::filesystem::create_directories(directory);
                    ImageFlipVertical(&capture);
                    ExportImage(capture, (std::filesystem::path(directory)/"core-build-radius.png").string().c_str());
                }
            }
            UnloadImageColors(pixels);UnloadImage(capture);
        }
        const bool coreRange = rangeInk[0] == 0 && rangeInk[1] == 0 && rangeInk[2] > 100;
        okay &= coreRange;
        if (!coreRange) std::cerr << "Core hover range failed: " << rangeInk[0] << '/' << rangeInk[1] << '/' << rangeInk[2] << '\n';
        // Real placement event: the tower and its support share a spring, but
        // only the tower emits dust. Exercise delay, animation and exact settle.
        app->effects_.clear();
        const EntityId placedTower{69100,1}, placedSupport{69101,1};
        const std::array placementEvents{GameEvent{
            .type=GameEventType::BuildingPlaced, .entityId=placedTower,
            .buildingType=BuildingType::GunTurret,
            .platformFrame=PlatformFrameInstance{.id=placedSupport}, .position={0,0,0}}};
        app->processPresentationEvents(placementEvents, snapshot);
        bool placement = count(PresentationEffectType::BuildingPlaced) == 2;
        const Camera3D placementCamera{{3,2.7F,-5},{0,.55F,0},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
        lighting.cameraPosition=placementCamera.position;
        std::array<std::uint64_t, 5> placementHashes{};
        std::array<int, 5> placementPixels{};
        const std::array<float, 5> placementProgress{.26F,.55F,.26F,.26F,1};
        for (std::size_t frameIndex=0; frameIndex<placementProgress.size(); ++frameIndex) {
            if (frameIndex == 2) app->effects_.pop_back(); // linked support must not add particles
            for (auto& effect:app->effects_) {
                effect.remaining=effect.duration*(1-placementProgress[frameIndex]);
                effect.startDelayRemaining=frameIndex==3?.1:0;
            }
            const float scale=app->buildingAnimationScaleAt({0,0,0},placedTower);
            placement &= scale > 0 && scale <= 1.14F &&
                (frameIndex != 3 || scale == 1) && (frameIndex != 4 || scale == 1);
            app->renderer_->beginFrame();
            BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(placementCamera);
            app->drawPresentationEffects(placementCamera,lighting);
            EndMode3D();EndTextureMode();
            Image image=LoadImageFromTexture(target.texture);
            auto* pixels=LoadImageColors(image);
            for (int i=0;i<image.width*image.height;++i) {
                placementPixels[frameIndex] += pixels[i].r>60;
                placementHashes[frameIndex] = placementHashes[frameIndex]*31U +
                    static_cast<unsigned int>(pixels[i].r)*65536U +
                    static_cast<unsigned int>(pixels[i].g)*256U + pixels[i].b;
            }
            UnloadImageColors(pixels);UnloadImage(image);
            if (frameIndex < 2) {
                if (const char* directory=std::getenv("IAN_BUILD_PLACEMENT_CAPTURE")) {
                    std::filesystem::create_directories(directory);
                    app->renderer_->beginFrame();
                    BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(placementCamera);
                    app->renderer_->beginWorldShader(lighting);
                    WorldMaterialState placementMaterial{};placementMaterial.unlitAmount=.35F;
                    app->renderer_->setWorldMaterial(placementMaterial);
                    DrawCube({0,-.10F,0},3,.2F,3,{116,125,84,255});
                    static_cast<void>(app->renderer_->drawGunTurret({0,0,0},0,0,WHITE,scale));
                    app->renderer_->endWorldShader();app->drawPresentationEffects(placementCamera,lighting);
                    EndMode3D();EndTextureMode();
                    Image capture=LoadImageFromTexture(target.texture);ImageFlipVertical(&capture);
                    ExportImage(capture,(std::filesystem::path(directory)/
                        ("placement-"+std::to_string(frameIndex)+".png")).string().c_str());
                    UnloadImage(capture);
                }
            }
        }
        placement &= placementPixels[0]>100 && placementPixels[1]>100 &&
            placementHashes[0]!=placementHashes[1] && placementHashes[0]==placementHashes[2] &&
            placementPixels[3]==0 && placementPixels[4]==0;
        okay &= placement;
        if (!placement) std::cerr << "Construction feedback failed animation, support deduplication or delay/expiry\n";
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
