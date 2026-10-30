#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/TutorialPresentation.hpp"
#include "ui/EnemySpawnMenu.hpp"
#include "presentation/PresentationTimeline.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <chrono>
#include <rlgl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace ian {
bool checkRelicSaveJsonRoundTrip();
struct AppPresentationTestAccess {
    static int run() {
        if (!checkRelicSaveJsonRoundTrip()) {std::cerr << "Relic JSON save roundtrip failed\n";return 1;}
        auto app=std::make_unique<App>();
        {
            const Vec3 anchor{0,1,0};
            const EntityId target{456,1};
            for(int hit=0;hit<100;++hit) app->addFloatingDamageNumber(anchor,4,false,0,target);
            if(app->floatingDamageNumbers_.size()!=1 || app->floatingDamageNumbers_[0].damage!=400) {
                std::cerr<<"Rapid damage was not combined per target\n";return 1;
            }
            const double remaining=app->floatingDamageNumbers_[0].remaining;
            app->addFloatingDamageNumber(anchor,20,true,0,target);
            if(app->floatingDamageNumbers_.size()!=2 || app->floatingDamageNumbers_[0].remaining!=remaining) {
                std::cerr<<"Critical hits merged or damage text lifetime was extended\n";return 1;
            }
            app->floatingDamageNumbers_[0].remaining-=.13;
            app->addFloatingDamageNumber(anchor,4,false,0,target);
            if(app->floatingDamageNumbers_.size()!=3) {std::cerr<<"Damage burst window never closed\n";return 1;}
            app->floatingDamageNumbers_.clear();
            for(std::uint32_t hit=0;hit<100;++hit)
                app->addFloatingDamageNumber(anchor,5,false,0,EntityId{hit,1});
            std::array<bool,7> lanes{};
            if(app->floatingDamageNumbers_.size()!=lanes.size()) {std::cerr<<"Crowded damage labels are unbounded\n";return 1;}
            for(const auto& number:app->floatingDamageNumbers_) {
                if(number.lane>=lanes.size() || lanes[number.lane]) {std::cerr<<"Crowded damage lanes overlap\n";return 1;}
                lanes[number.lane]=true;
            }
            app->floatingDamageNumbers_.clear();
            app->addFloatingDamageNumber(anchor,4,false,.2,target);
            app->addFloatingDamageNumber(anchor,5,false,0,target);
            if(app->floatingDamageNumbers_.size()!=2) {std::cerr<<"Delayed hit displayed before impact\n";return 1;}
            presentation::advanceTimeline(app->floatingDamageNumbers_,1.0);
            if(!app->floatingDamageNumbers_.empty()) {std::cerr<<"Damage numbers did not expire\n";return 1;}
        }
        SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_HIGHDPI);
        InitWindow(800,600,"Presentation event integration");
        if(!IsWindowReady()) return 77;
        {
            GraphicsSettings display;
            display.displayWidth = 960;
            display.displayHeight = 540;
            app->applyDisplaySettings(display);
            if (GetScreenWidth() != 960 || GetScreenHeight() != 540) {
                std::cerr << "Output resolution did not resize the native window\n"; return 1;
            }
            display.displayWidth = 800;
            display.displayHeight = 600;
            app->applyDisplaySettings(display);
        }
        app->renderer_.emplace();app->renderer_->initialize();
        SimulationSnapshot snapshot=app->simulation_.snapshot();
        if (const char* directory=std::getenv("IAN_ECONOMY_ENDLESS_CAPTURE")) {
            const Camera3D camera{{0,2,8},{0,1,0},{0,1,0},55,CAMERA_PERSPECTIVE};
            WorldLighting light{}; light.cameraPosition=camera.position;
            const auto target=LoadRenderTexture(800,600);
            std::array<IceWandProjectile,1> shots{{{
                .id={800,1},.previousPosition={-1,1,0},.position={1,1,0},.velocity={4,0,0},
                .trail={Vec3{1,1,0},Vec3{0,1,0},Vec3{-1,1,0}},.trailCount=3,
                .age=.5,.lifetime=2,.radius=.25,.element=WandElement::Fire,.active=true}}};
            std::array<EnemyProjectile,1> hostile{{{
                .id={801,1},.ownerId={802,1},.position={1,1,0},.previousPosition={-1,1,0},
                .velocity={4,0,0},.radius=.3,.lifetimeRemaining=2,.active=true}}};
            std::array<CannonProjectile,1> shells{};
            shells[0].active=true; shells[0].position={1,1,0}; shells[0].velocity={4,0,0};
            shells[0].trailHead=2; shells[0].trailPointCount=3;
            shells[0].trailPositions[0]={-2,1,0}; shells[0].trailPositions[1]={-1,1,0};
            shells[0].trailPositions[2]={1,1,0};
            std::array<AttackProjectile,1> bullets{};
            bullets[0].active=true; bullets[0].position={1,1,0}; bullets[0].previousPosition={-1,1,0};
            bullets[0].launchOrigin={-2,1,0}; bullets[0].runtime.velocity={4,0,0};
            bullets[0].runtime.previousTravelledDistance=1; bullets[0].runtime.travelledDistance=3;
            bullets[0].configuration.stats.damage=10; bullets[0].configuration.stats.size=2;
            std::array<BombProjectile,1> bombs{{{
                .id={803,1},.position={1,1,0},.fuseRemaining=1,.fuseDuration=3,.active=true}}};
            snapshot.elapsedSeconds=8;
            bool okay=true;
            constexpr std::array names{"fire","ice","cannon","catapult","mortar","turret",
                "gun","missile","shrapnel","stinger","magic","royal","bomb"};
            for(std::size_t kind=0;kind<names.size();++kind) {
                snapshot.fireWandProjectiles={}; snapshot.iceWandProjectiles={}; snapshot.cannonProjectiles={};
                snapshot.defenseProjectiles={}; snapshot.enemyProjectiles={}; snapshot.bombProjectiles={};
                shots[0].element=kind==1 ? WandElement::Ice : WandElement::Fire;
                if(kind==0) snapshot.fireWandProjectiles=shots;
                else if(kind==1) snapshot.iceWandProjectiles=shots;
                else if(kind<=4) {
                    shells[0].type=kind==2 ? BuildingType::Cannon : kind==3 ? BuildingType::Catapult : BuildingType::Mortar;
                    snapshot.cannonProjectiles=shells;
                } else if(kind<=8) {
                    bullets[0].type=kind==5 ? BuildingType::Turret : kind==7 ? BuildingType::MissileLauncher : BuildingType::GunTurret;
                    bullets[0].shrapnel=kind==8; snapshot.defenseProjectiles=bullets;
                } else if(kind<=11) {
                    hostile[0].stinger=kind==9; hostile[0].royal=kind==11;
                    snapshot.enemyProjectiles=hostile;
                } else snapshot.bombProjectiles=bombs;
                for(int pausedBy=0;pausedBy<3;++pausedBy) {
                    snapshot.state=pausedBy==0 ? RunState::Paused : RunState::Wave;
                    snapshot.runUpgradeChoicePending=pausedBy==1; snapshot.runShop.pending=pausedBy==2;
                    std::vector<Color> reference;
                    bool stable=true, visible=true;
                    for(const float alpha : {.05F,.95F}) {
                        okay &= app_detail::worldInterpolationAlpha(snapshot,alpha)==1;
                        app->renderer_->beginFrame();
                        BeginTextureMode(target); ClearBackground({29,47,57,255}); BeginMode3D(camera);
                        app->drawWorldProjectiles(snapshot,camera,light,alpha);
                        EndMode3D(); EndTextureMode();
                        auto capture=LoadImageFromTexture(target.texture);
                        auto* pixels=LoadImageColors(capture);
                        if(reference.empty()) reference.assign(pixels,pixels+capture.width*capture.height);
                        else for(std::size_t i=0;i<reference.size();++i)
                            stable &= reference[i].r==pixels[i].r && reference[i].g==pixels[i].g && reference[i].b==pixels[i].b;
                        int ink=0;
                        for(std::size_t i=0;i<reference.size();++i)
                            ink+=pixels[i].r!=29 || pixels[i].g!=47 || pixels[i].b!=57;
                        visible &= ink>20;
                        UnloadImageColors(pixels); UnloadImage(capture);
                        WaitTime(.005);
                    }
                    if(!stable || !visible) std::cerr << "Projectile pause regression: " << names[kind]
                        << " mode=" << pausedBy << " stable=" << stable << " visible=" << visible << "\n";
                    okay &= stable && visible;
                }
            }
            snapshot.state=RunState::Wave; snapshot.runUpgradeChoicePending=false; snapshot.runShop.pending=false;
            okay &= std::abs(app_detail::worldInterpolationAlpha(snapshot,.25)-.25F)<.00001F;
            snapshot.fireWandProjectiles={}; snapshot.iceWandProjectiles={}; snapshot.cannonProjectiles={};
            snapshot.defenseProjectiles={}; snapshot.enemyProjectiles={}; snapshot.bombProjectiles={};

            app->simulation_.startRunFromSeed(PlayerClass::Vanguard,817);
            app->simulation_.togglePause();
            app->effects_.push_back({.type=PresentationEffectType::ChainLightning,
                .position={-1,1,0},.targetPosition=Vec3{1,1,0},.remaining=.15,.duration=.4});
            std::vector<Color> lightningReference;
            for(int frame=0;frame<2;++frame) {
                app->renderer_->beginFrame();
                BeginTextureMode(target); ClearBackground({29,47,57,255}); BeginMode3D(camera);
                app->drawPresentationEffects(camera,light);
                EndMode3D(); EndTextureMode();
                auto capture=LoadImageFromTexture(target.texture); auto* pixels=LoadImageColors(capture);
                if(frame==0) lightningReference.assign(pixels,pixels+capture.width*capture.height);
                else for(std::size_t i=0;i<lightningReference.size();++i)
                    okay &= lightningReference[i].r==pixels[i].r && lightningReference[i].g==pixels[i].g && lightningReference[i].b==pixels[i].b;
                UnloadImageColors(pixels); UnloadImage(capture); WaitTime(.03);
            }
            app->effects_.clear();
            app->ui_.initialize();
            snapshot.fireWandProjectiles={}; snapshot.state=RunState::Wave;
            snapshot.runUpgradeChoicePending=false; snapshot.runShop.pending=false; snapshot.finalNight=true;
            snapshot.overtimeSeconds=63; snapshot.overtimeActive=true; snapshot.wave=12; snapshot.activeEnemyCount=240;
            snapshot.phaseDuration=0; snapshot.phaseTimeRemaining=0;
            BeginTextureMode(target); ClearBackground({68,95,85,255});
            drawHud(app->ui_,snapshot,HudViewState{.minimapHidden=true},camera,app->userSettings_.controls);
            EndTextureMode();
            auto capture=LoadImageFromTexture(target.texture); ImageFlipVertical(&capture);
            std::filesystem::create_directories(directory);
            ExportImage(capture,(std::filesystem::path(directory)/"endless-hud.png").string().c_str());
            UnloadImage(capture);
            const std::array<WorldLandmarkInstance,1> landmarks{{{
                .id={805,1},.type=WorldLandmarkType::LumberMill,.position={0,0,0},
                .activated=true,.level=2,.upgradeCoinCost=75,.incomePerMinute=90,.nextIncomePerMinute=120}}};
            snapshot.worldLandmarks=landmarks; snapshot.aimedWorldLandmark=landmarks[0].id;
            snapshot.coins=100;
            const auto producer=app->buildInteractionPrompt(snapshot,camera);
            okay &= producer && producer->actionText=="UPGRADE" && producer->worldAnchor.y==2.8F &&
                producer->cost && producer->cost->crystals==75;
            const auto spawnLayout=enemy_spawn_menu::layout({800,600});
            EnemyType selected=EnemyType::Basic; EliteAffix variant{}; int count=1;
            EnemySpawnMenuInput clickNight{
                .pointer={spawnLayout.startNight.x+5,spawnLayout.startNight.y+5},.click=true};
            BeginTextureMode(target);
            const auto start=drawEnemySpawnCatalog(app->ui_,selected,variant,count,0,{800,600},clickNight,true);
            const auto blocked=drawEnemySpawnCatalog(app->ui_,selected,variant,count,0,{800,600},clickNight,false);
            okay &= start.startNight && !start.summon && !blocked.startNight;
            EndTextureMode();
            auto* window=glfwGetCurrentContext();
            const auto mouseCallback=glfwSetMouseButtonCallback(window,nullptr);
            glfwSetMouseButtonCallback(window,mouseCallback);
            if(mouseCallback) mouseCallback(window,GLFW_MOUSE_BUTTON_LEFT,GLFW_PRESS,0);
            SetTargetFPS(120);
            for(int frame=0;frame<35;++frame) {
                BeginDrawing(); ClearBackground({68,95,85,255});
                app->interactionPromptRenderer_.draw(producer,camera,app->ui_,app->userSettings_.controls);
                EndDrawing();
            }
            BeginTextureMode(target); ClearBackground({68,95,85,255});
            app->interactionPromptRenderer_.draw(producer,camera,app->ui_,app->userSettings_.controls);
            EndTextureMode();
            capture=LoadImageFromTexture(target.texture); ImageFlipVertical(&capture);
            ExportImage(capture,(std::filesystem::path(directory)/"world-producer.png").string().c_str());
            if(mouseCallback) mouseCallback(window,GLFW_MOUSE_BUTTON_LEFT,GLFW_RELEASE,0);
            UnloadImage(capture); UnloadRenderTexture(target);
            app->ui_.shutdown(); app->renderer_->shutdown(); app->renderer_.reset();
            CloseWindow();
            if(!okay) std::cerr << "Paused and upgrade-screen projectiles must freeze independently of interpolation alpha\n";
            else std::cout << "13 projectile variants freeze across pause/upgrade/shop screens; lightning and UI renders passed\n";
            return okay?0:1;
        }
        {
            TutorialHintPresentation hint;
            for (int i = 0; i < 20; ++i) hint.update(TutorialObjective::MineWood, false, .016);
            if (hint.visual().opacity < .99F || hint.visual().checkProgress != 0) {
                std::cerr << "Current tutorial step starts with an empty checkbox\n"; return 1;
            }
            auto tutorial = snapshot; tutorial.tutorialWoodTarget = 20; tutorial.wood = 0;
            if (tutorialHintCompleted(TutorialObjective::MineWood, tutorial, ActionMode::Equipment)) {
                std::cerr << "Unfinished tutorial steps cannot get check marks\n"; return 1;
            }
            tutorial.wood = 20;
            const bool completed = tutorialHintCompleted(hint.displayed, tutorial, ActionMode::Equipment);
            hint.update(TutorialObjective::OpenBuildMode, completed, .016);
            for (int i = 0; i < 15; ++i) hint.update(TutorialObjective::OpenBuildMode, false, .016);
            if (hint.displayed != TutorialObjective::MineWood || hint.visual().checkProgress < .99F) {
                std::cerr << "Completed instruction must remain visible long enough to read the tick\n"; return 1;
            }
            const auto frozen = hint.visual(); hint.update(TutorialObjective::OpenBuildMode, false, 0);
            if (hint.visual().opacity != frozen.opacity || hint.visual().checkProgress != frozen.checkProgress) {
                std::cerr << "Paused tutorial transitions must freeze\n"; return 1;
            }
            for (int i = 0; i < 55; ++i) hint.update(TutorialObjective::OpenBuildMode, false, .016);
            if (hint.displayed != TutorialObjective::OpenBuildMode || hint.visual().checkProgress != 0) {
                std::cerr << "Next instruction must replace the finished one with an empty checkbox\n"; return 1;
            }
            hint.update(TutorialObjective::PlaceCore, false, .016);
            for (int i = 0; i < 30; ++i) hint.update(TutorialObjective::PlaceCore, false, .016);
            if (hint.displayed != TutorialObjective::PlaceCore || hint.visual().checkProgress != 0) {
                std::cerr << "Changing instruction without completion must not award a tick\n"; return 1;
            }
            hint.update(std::nullopt, true, .016);
            for (int i = 0; i < 70; ++i) hint.update(std::nullopt, false, .016);
            if (hint.displayed) { std::cerr << "Final tutorial check must fade out cleanly\n"; return 1; }
        }

        {
            auto map = MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
            auto world = WorldConfig::defaults(); world.terrainAmplitude = 0;
            auto supportRun = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
            supportRun->startRun(); PlayerCommand god;
            god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
            supportRun->tick(0, god);
            const Vec3 hit{.2, 0, 4.2};
            const auto initial = supportRun->previewFloorPlatformWithFoundation(hit);
            const auto foundation = supportRun->placeFoundationAtHeight(hit, initial.foundation.floorHeight);
            if (!foundation) { std::cerr << "Automatic floor fixture needs a lower support\n"; return 1; }
            const auto preview = supportRun->previewFloorPlatformWithFoundation(hit, foundation->floorHeight);
            const auto built = supportRun->placeFloorPlatformWithFoundation(hit, foundation->floorHeight);
            if (!preview.valid() || preview.foundation.existingFrame != foundation->id || !built ||
                supportRun->snapshot().platformFrames.size() != 2) {
                std::cerr << "Automatic floor must reuse the existing lower support without duplication\n";
                return 1;
            }
        }

        {
            const float previousIntensity = app->motionSwayIntensity_;
            app->motionSwayIntensity_ = 1.0F;
            std::array<double, 4> atStop{};
            const std::array rates{30, 60, 144, 240};
            for (std::size_t rate = 0; rate < rates.size(); ++rate) {
                app->suspendGameplayInput();
                const double dt = 1.0 / rates[rate];
                for (int frame = 0; frame < rates[rate] / 2; ++frame)
                    app->updateWeaponLookSway(2.5 * dt, -1.5 * dt, dt);
                atStop[rate] = app->weaponLookYawOffset_;
                if (atStop[rate] >= -.01 || app->weaponLookPitchOffset_ <= .005) {
                    std::cerr << "Weapon must lag behind the turn without changing camera aim\n";
                    return 1;
                }
                bool softReturn = false;
                for (int frame = 0; frame < rates[rate]; ++frame) {
                    app->updateWeaponLookSway(0, 0, dt);
                    softReturn |= app->weaponLookYawOffset_ > 0;
                }
                if (!softReturn || std::abs(app->weaponLookYawOffset_) > 1e-5 ||
                    std::abs(app->weaponLookPitchOffset_) > 1e-5) {
                    std::cerr << "Weapon sway must spring back once and settle\n";
                    return 1;
                }
            }
            for (const auto value : atStop) if (std::abs(value - atStop[0]) > 1e-8) {
                std::cerr << "Weapon weight must stay consistent across frame rates\n";
                return 1;
            }
            for (int frame = 0; frame < 240; ++frame) {
                app->updateWeaponLookSway(frame % 2 ? -2 : 2, .8, 1.0 / 240);
                if (!std::isfinite(app->weaponLookYawOffset_) || std::abs(app->weaponLookYawOffset_) > .038 ||
                    std::abs(app->weaponLookPitchOffset_) > .027) {
                    std::cerr << "Rapid turns must keep weapon sway bounded\n";
                    return 1;
                }
            }
            app->motionSwayIntensity_ = 0;
            app->updateWeaponLookSway(1, 1, 1.0 / 60);
            if (app->weaponLookYawOffset_ != 0 || app->weaponLookPitchOffset_ != 0 ||
                app->weaponLookYawVelocity_ != 0 || app->weaponLookPitchVelocity_ != 0) {
                std::cerr << "Zero motion intensity must fully disable look sway\n";
                return 1;
            }
            app->motionSwayIntensity_ = previousIntensity;
            app->suspendGameplayInput();
        }

        {
            std::array<BuildingInstance, 1> structures{{{
                .id = {4321, 1}, .type = BuildingType::GunTurret,
                .gridPosition = {0, 0}, .health = 90, .maxHealth = 100,
            }}};
            auto hitSnapshot = snapshot;
            hitSnapshot.buildings = structures;
            const std::array hits{GameEvent{.type = GameEventType::BuildingDamaged,
                .entityId = structures[0].id, .buildingType = BuildingType::GunTurret,
                .position = {}, .amount = 10}};
            for (int i = 0; i < 30; ++i) app->processPresentationEvents(hits, hitSnapshot);
            const auto impacts = std::ranges::count(app->effects_, PresentationEffectType::BuildingDamaged, &PresentationEffect::type);
            if (impacts != 1 || app->buildingImpactVisuals_.size() != 1 || !app->effects_.back().targetPosition) {
                std::cerr << "Building hits must coalesce into one contact burst and recoil per target\n";
                return 1;
            }
            app->buildingImpactVisuals_.front().remaining = .18;
            const auto displacement = app->buildingImpactOffsetAt(structures[0].id);
            if (std::hypot(displacement.x, displacement.z) < .01 || std::hypot(displacement.x, displacement.z) > .16) {
                std::cerr << "Building impact recoil must be visible and bounded\n";
                return 1;
            }
            const Camera3D hitCamera{{3, 2, 5}, {0, .75F, 0}, {0, 1, 0}, 48, CAMERA_PERSPECTIVE};
            const auto target = LoadRenderTexture(640, 400);
            WorldLighting light{}; light.cameraPosition = hitCamera.position;
            for (const bool reduced : {false, true}) {
                app->userSettings_.accessibility.reduceFlashes = reduced;
                for (auto& effect : app->effects_) effect.remaining = effect.duration - .07;
                app->renderer_->beginFrame();
                BeginTextureMode(target); ClearBackground({29, 47, 57, 255}); BeginMode3D(hitCamera);
                app->renderer_->beginWorldShader(light);
                static_cast<void>(app->renderer_->drawGunTurret({}, 0, 0));
                app->renderer_->endWorldShader();
                app->drawPresentationEffects(hitCamera, light);
                EndMode3D(); EndTextureMode();
                if (const char* directory = std::getenv("IAN_HIT_CAPTURE")) {
                    std::filesystem::create_directories(directory);
                    auto image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                    ExportImage(image, (std::string(directory) + (reduced ? "/building-hit-reduced.png" : "/building-hit.png")).c_str());
                    UnloadImage(image);
                }
            }
            UnloadRenderTexture(target);
            app->userSettings_.accessibility.reduceFlashes = false;
            app->effects_.clear(); app->buildingImpactVisuals_.clear();
        }

        {
            auto sample = snapshot;
            sample.state = RunState::Gathering;
            std::array<BuildingInstance, 1> targets{{{
                .id = {987, 1}, .type = BuildingType::GunTurret,
                .gridPosition = {0, 0}, .health = 50, .maxHealth = 100,
            }}};
            sample.buildings = targets;
            sample.aimedBuilding = targets[0].id;
            const Camera3D view{{0, 3, 8}, {0, 1, 0}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
            const auto damaged = app->buildInteractionPrompt(sample, view);
            targets[0].health = 100;
            const auto healthy = app->buildInteractionPrompt(sample, view);
            targets[0].type = BuildingType::Wall;
            targets[0].health = 50;
            const auto fence = app->buildInteractionPrompt(sample, view);
            if (!damaged || !damaged->showRepair || !damaged->showRemove || !damaged->showRotate ||
                !healthy || healthy->showRepair || !healthy->showRemove || !healthy->showRotate ||
                !fence || !fence->showRepair || fence->showRotate || !fence->actionText.empty()) {
                std::cerr << "Building repair/remove prompts disagree with health or fence input\n";
                return 1;
            }
            app->removalDragActive_ = true;
            app->removalDragTargets_ = {targets[0].id};
            const auto selected = app->buildingFeedbackHighlightAt({}, targets[0].id, 0, 3);
            app->removalDragActive_ = false;
            app->removalDragTargets_.clear();
            const auto cleared = app->buildingFeedbackHighlightAt({}, targets[0].id, 0, 3);
            if (selected.removalAmount < .6F || cleared.removalAmount != 0) {
                std::cerr << "Removal tint does not clear after releasing removal mode\n";
                return 1;
            }
            std::array<BuildingInstance, 2> removalBuildings{targets[0], targets[0]};
            removalBuildings[1].id = {988, 1};
            sample.buildings = removalBuildings;
            const auto first = removalBuildings[0].id, second = removalBuildings[1].id;
            const int removalKey = controlKey(app->userSettings_.controls, ControlAction::Sell);
            const auto removalFrame = [&](bool held, std::optional<EntityId> aim, double seconds) {
                PollInputEvents();
                PlayAutomationEvent({0, held ? 2U : 1U, {removalKey, 0, 0, 0}});
                app->processRemovalInput(sample, aim, true, seconds);
            };
            removalFrame(true, first, .016);
            removalFrame(true, std::nullopt, .10);
            removalFrame(true, std::nullopt, .10);
            removalFrame(true, first, .016);
            removalFrame(false, first, .016);
            if (app->removalDragActive_ || !app->queuedBuildingSales_.empty()) {
                std::cerr << "Looking away must cancel without rearming while X remains held\n";
                return 1;
            }
            removalFrame(true, first, .016);
            removalFrame(false, std::nullopt, .016);
            if (!app->queuedBuildingSales_.empty()) {
                std::cerr << "Releasing X off target must cancel immediately\n";
                return 1;
            }
            removalFrame(true, first, .016);
            removalFrame(true, std::nullopt, .08);
            removalFrame(true, second, .016);
            removalFrame(false, second, .016);
            if (app->queuedBuildingSales_.size() != 2 || app->removalDragActive_) {
                std::cerr << "Brief gaps must preserve intentional multi-building removal\n";
                return 1;
            }
            app->queuedBuildingSales_.clear();
        }

        {
            const auto original=app->metaProgression_;
            const auto persisted=app->persistedMetaProgression_;
            app->metaProgression_={};app->persistedMetaProgression_={};
            auto runSnapshot=snapshot;runSnapshot.sandboxMode=false;runSnapshot.unlimitedResources=false;
            const auto send=[&](std::initializer_list<GameEvent> batch) {
                app->recordMetaProgression(std::span(batch.begin(),batch.size()),runSnapshot);
            };
            const auto hint=[&] {
                return presentedTutorialObjective(TutorialObjective::PlaceCore,ActionMode::Equipment,
                    app->metaProgression_.runsPlayed);
            };
            send({{.type=GameEventType::RunStarted}});
            bool hints=app->metaProgression_.runsPlayed==1 && hint()==TutorialObjective::OpenBuildMode;
            send({{.type=GameEventType::BuildingRejected},{.type=GameEventType::PauseChanged}});
            hints &= app->metaProgression_.placementControlHintsUsed==0;
            send({{.type=GameEventType::BuildingPlaced},{.type=GameEventType::ModularBuildingPlaced}});
            hints &= app->metaProgression_.placementControlHintsUsed==1;
            send({{.type=GameEventType::BuildingPlaced}});
            hints &= app->metaProgression_.placementControlHintsUsed==2;
            send({{.type=GameEventType::BuildingPlaced}});
            hints &= app->metaProgression_.placementControlHintsUsed==2;
            send({{.type=GameEventType::RunRestarted}});
            hints &= app->metaProgression_.runsPlayed==2 && !hint();
            app->metaProgression_=original;app->persistedMetaProgression_=persisted;
            app->questGameplayTracker_={};
            if(!hints) {std::cerr<<"First-run or placement-hint lifetime failed\n";return 1;}
        }
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
        // Destruction travels through real App events, including the model's
        // authored upgrade and delayed dust. Exercise both model and effect passes.
        app->effects_.clear();app->soldBuildingVisuals_.clear();
        const BuildingInstance wreck{.id={70100,1},.type=BuildingType::GunTurret,
            .gridPosition={0,0},.rotation=2,.level=6,.health=0,.maxHealth=500};
        const Vec3 wreckCenter=buildingWorldPosition(wreck);
        const std::array destructionEvent{GameEvent{.type=GameEventType::BuildingDestroyed,
            .entityId=wreck.id,.buildingType=wreck.type,.building=wreck,.position=wreckCenter}};
        app->processPresentationEvents(destructionEvent,snapshot);
        bool destruction=app->soldBuildingVisuals_.size()==1 &&
            app->soldBuildingVisuals_[0].building && app->soldBuildingVisuals_[0].building->level==6 &&
            app->soldBuildingVisuals_[0].building->rotation==2 &&
            count(PresentationEffectType::Debris)==1 && app->effects_.back().variant==2;
        const double debrisDelay=app->effects_.back().startDelayRemaining;
        const Camera3D destructionCamera{{3,2.6F,-5},{static_cast<float>(wreckCenter.x),.65F,
            static_cast<float>(wreckCenter.z)},{0,1,0},6,CAMERA_ORTHOGRAPHIC};
        app->gameplayCamera_=destructionCamera;lighting.cameraPosition=destructionCamera.position;
        const std::array<float,6> wreckTimes{0,.14F,.30F,.55F,.95F,1.10F};
        std::array<int,6> wreckPixels{};
        std::array<std::uint64_t,6> wreckHashes{};
        for(std::size_t frameIndex=0;frameIndex<wreckTimes.size();++frameIndex) {
            const float seconds=wreckTimes[frameIndex];
            for(auto& visual:app->soldBuildingVisuals_)visual.remaining=visual.duration-seconds;
            for(auto& effect:app->effects_) {
                effect.startDelayRemaining=std::max(0.0,debrisDelay-seconds);
                effect.remaining=effect.duration-std::max(0.0,seconds-debrisDelay);
            }
            app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});
            BeginMode3D(destructionCamera);app->drawPresentationEffects(destructionCamera,lighting);
            EndMode3D();EndTextureMode();
            Image capture=LoadImageFromTexture(target.texture);auto* pixels=LoadImageColors(capture);
            for(int i=0;i<capture.width*capture.height;++i) {
                wreckPixels[frameIndex]+=pixels[i].r>70;
                wreckHashes[frameIndex]=wreckHashes[frameIndex]*31U+pixels[i].r*65536U+
                    pixels[i].g*256U+pixels[i].b;
            }
            UnloadImageColors(pixels);UnloadImage(capture);
            if(const char* directory=std::getenv("IAN_DESTRUCTION_CAPTURE")) {
                std::filesystem::create_directories(directory);
                app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});
                BeginMode3D(destructionCamera);app->renderer_->beginWorldShader(lighting);
                WorldMaterialState wreckMaterial{};wreckMaterial.unlitAmount=.3F;
                app->renderer_->setWorldMaterial(wreckMaterial);
                DrawCube({static_cast<float>(wreckCenter.x),-.12F,static_cast<float>(wreckCenter.z)},
                    5,.2F,5,{116,125,84,255});
                app->drawSoldBuildingVisuals();app->renderer_->endWorldShader();
                app->drawPresentationEffects(destructionCamera,lighting);EndMode3D();EndTextureMode();
                Image modelCapture=LoadImageFromTexture(target.texture);ImageFlipVertical(&modelCapture);
                ExportImage(modelCapture,(std::filesystem::path(directory)/
                    ("destruction-"+std::to_string(frameIndex)+".png")).string().c_str());
                UnloadImage(modelCapture);
            }
        }
        destruction &= wreckPixels[0]==0 && wreckPixels[1]>100 && wreckPixels[2]>100 &&
            wreckPixels[5]==0 && wreckHashes[1]!=wreckHashes[2];
        app->effects_.clear();app->soldBuildingVisuals_.clear();
        const PlatformFrameInstance fallingFrame{.id={70101,1},.anchor={0,0,0},.floorHeight=8,.storey=2};
        const std::array collapseEvent{GameEvent{.type=GameEventType::ModularBuildingDestroyed,
            .entityId=fallingFrame.id,.platformFrame=fallingFrame,.position={1,8,1},.structuralCollapse=true}};
        app->processPresentationEvents(collapseEvent,snapshot);
        destruction &= app->soldBuildingVisuals_.size()==1 &&
            app->soldBuildingVisuals_[0].remaining>app->soldBuildingVisuals_[0].duration &&
            app->effects_.back().startDelayRemaining>.6 && app->effects_.back().variant==1;
        app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});
        BeginMode3D(destructionCamera);
        app->drawPresentationEffects(destructionCamera,lighting);EndMode3D();EndTextureMode();
        Image delayed=LoadImageFromTexture(target.texture);auto* delayedPixels=LoadImageColors(delayed);
        int premature=0;
        for(int i=0;i<delayed.width*delayed.height;++i)premature+=delayedPixels[i].r>70;
        UnloadImageColors(delayedPixels);UnloadImage(delayed);
        destruction &= premature==0;
        okay &= destruction;
        if(!destruction)std::cerr << "Building destruction model, animation, dust delay or expiry failed\n";
        app->effects_.clear();
        const double rootGround=app->simulation_.terrain().getHeight(0,0);
        const std::array roots{GameEvent{.type=GameEventType::StumpRootWave,.position={0,rootGround,0},
            .targetPosition=Vec3{0,rootGround,-1},.intensity=1,.duration=1.25}};
        app->processPresentationEvents(roots,snapshot);
        bool rootOkay=count(PresentationEffectType::StumpRootWave)==1;
        for(auto& effect:app->effects_) effect.remaining=effect.duration-.43;
        const Camera3D rootCamera{{7,static_cast<float>(rootGround)+5,5},
            {0,static_cast<float>(rootGround),-3},{0,1,0},55,CAMERA_PERSPECTIVE};
        lighting.cameraPosition=rootCamera.position;
        app->renderer_->beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(rootCamera);
        app->drawPresentationEffects(rootCamera,lighting);EndMode3D();EndTextureMode();
        Image rootImage=LoadImageFromTexture(target.texture);auto* rootPixels=LoadImageColors(rootImage);
        int brown=0;
        for(int i=0;i<rootImage.width*rootImage.height;++i) brown+=rootPixels[i].r>50 &&
            int(rootPixels[i].r)*10>int(rootPixels[i].g)*13;
        rootOkay &= brown>200;
        if(const char* capture=std::getenv("IAN_CAPTURE_STUMP")) {
            ImageFlipVertical(&rootImage);std::filesystem::create_directories(capture);
            ExportImage(rootImage,(std::filesystem::path(capture)/"app-roots.png").string().c_str());
        }
        UnloadImageColors(rootPixels);UnloadImage(rootImage);
        okay &= rootOkay;
        if(!rootOkay) std::cerr<<"Stump roots failed event routing or actual App rendering\n";
        // Destructive UI actions run only in the test's dedicated temporary folder.
        if(!std::filesystem::current_path().filename().string().starts_with("ian-presentation-events-smoke-")) {
            std::cerr<<"Refusing progress reset test outside its sandbox\n";return 1;
        }
        app->questSystem_=QuestSystem({
            {"first_hit","First victory","Defeat one enemy",QuestCategory::General,
                {{QuestMetric::EnemiesDefeated,QuestScope::Lifetime,1,QuestComparison::AtLeast,{},{}}}, {},{3,{},0},"Enemy defeated"},
            {"first_unlock","Abacus","Defeat one enemy",QuestCategory::Items,
                {{QuestMetric::EnemiesDefeated,QuestScope::Lifetime,1,QuestComparison::AtLeast,{},{}}}, {},{0,"item.abacus",5}}});
        app->metaProgression_={};app->persistedMetaProgression_={};app->questGameplayTracker_={};
        app->questCompletionNotices_.clear();app->questCompletionRemaining_=0;
        app->simulation_.returnToMainMenu();
        app->simulation_.startRunFromSeed(PlayerClass::Stump,614);
        app->playerArrival_.reset();app->hudHidden_=false;app->coreDefenseMenuVisible_=false;
        app->renderer_->setGraphicsPanelVisible(false);
        auto questSnapshot=app->simulation_.snapshot();
        const std::array runStart{GameEvent{.type=GameEventType::RunStarted}};
        const std::array kill{GameEvent{.type=GameEventType::EnemyKilled}};
        app->recordMetaProgression(runStart,questSnapshot);
        app->recordMetaProgression(kill,questSnapshot);
        bool quests=app->questCompletionNotices_.size()==2 && app->metaProgression_.silver==0 &&
            app->questCompletionNotices_.front().title=="Enemy defeated" &&
            app->questCompletionNotices_.back().title=="Abacus";
        app->recordMetaProgression(kill,questSnapshot);
        quests &= app->questCompletionNotices_.size()==2;
        app->updateQuestCompletionNotices(.2);
        const double held=app->questCompletionRemaining_;
        app->simulation_.togglePause();app->updateQuestCompletionNotices(100);
        quests &= app->questCompletionRemaining_==held;
        app->simulation_.togglePause();app->updateQuestCompletionNotices(5);
        quests &= app->questCompletionNotices_.size()==1 && app->questCompletionNotices_.front().id=="first_unlock";
        app->updateQuestCompletionNotices(.1);
        quests &= app->questCompletionRemaining_>4;
        okay &= quests;
        if(!quests) std::cerr<<"Quest completion queue, duplicate filtering, reward ownership or pause timing failed\n";
        app->metaProgression_.silver=99;app->metaProgression_.placementControlHintsUsed=2;
        app->metaProgression_.quests.ownedUnlocks.insert("class.berserker");
        app->metaProgression_.quests.purchasedUnlocks.insert("class.berserker");
        app->renderer_->settings().brightness=.17F;app->userSettings_.controls.mouseSensitivity=1.6F;
        app->persistUserSettings(true);
        const auto read=[](const char* path) {std::ifstream input(path);return std::string(std::istreambuf_iterator<char>(input),{});};
        const auto settingsBefore=read("user_settings/game_settings.json");
        for(const auto* name:{"user_settings/suspended_run.json","user_settings/suspended_run.json.bak",
            "user_settings/suspended_run.json.tmp","user_settings/suspended_run.bin","user_settings/meta_progression.json.bak"}) {
            std::ofstream file(name);file<<"old progress";
        }
        app->renderer_->setGraphicsPanelVisible(true);app->resumeAfterSettings_=true;
        bool reset=app->resetAllProgress();
        MetaProgression clean;app->questSystem_.initialize(clean);
        MetaProgression saved;
        reset &= app->metaProgression_==clean && app->persistedMetaProgression_==clean &&
            loadMetaProgression("user_settings/meta_progression.json",saved) && saved==clean &&
            app->simulation_.snapshot().state==RunState::MainMenu && app->questCompletionNotices_.empty() &&
            !app->resumeAfterSettings_ && read("user_settings/game_settings.json")==settingsBefore;
        reset &= app->simulation_.takeEvents().empty();
        for(const auto* name:{"user_settings/suspended_run.json","user_settings/suspended_run.json.bak",
            "user_settings/suspended_run.json.tmp","user_settings/suspended_run.bin","user_settings/meta_progression.json.bak"})
            reset &= !std::filesystem::exists(name);
        app->metaProgression_.silver=88;
        const auto unchanged=app->metaProgression_;
        std::filesystem::create_directory("user_settings/meta_progression.json.bak");
        {std::ofstream file("user_settings/meta_progression.json.bak/blocked");file<<"test";}
        reset &= !app->resetAllProgress() && app->metaProgression_==unchanged;
        std::filesystem::remove_all("user_settings/meta_progression.json.bak");
        okay &= reset;
        if(!reset) std::cerr<<"Full progress reset, backup cleanup, settings preservation or failed-write handling failed\n";
        // Exercise the actual keyboard handlers, including every modifier group.
        // raylib's public automation API uses event types 1 (up) and 2 (down).
        app->renderer_->setGraphicsPanelVisible(false);
        app->simulation_.startRunFromSeed(PlayerClass::Vanguard,91234);
        app->playerArrival_.reset();app->resetRunInputState();
        app->coreDefenseMenuVisible_=false;app->coreMenuPointerActive_=false;
        app->userSettings_.debugHotkeysEnabled=false;
        const auto seedBefore=app->simulation_.terrain().seed();
        const auto graphicsBefore=app->renderer_->settings();
        const auto timeBefore=app->environment_.timeOfDay();
        const auto frozenBefore=app->environment_.frozen();
        const auto spawnTypeBefore=app->debugSpawnType_;
        const auto chord=[&](int key,int modifier=KEY_NULL) {
            PollInputEvents();
            if(modifier!=KEY_NULL) PlayAutomationEvent({0,2,{modifier,0,0,0}});
            PlayAutomationEvent({0,2,{key,0,0,0}});
            app->processInput();
            PlayAutomationEvent({0,1,{key,0,0,0}});
            if(modifier!=KEY_NULL) PlayAutomationEvent({0,1,{modifier,0,0,0}});
            PollInputEvents();
        };
        bool debugKeys=true;
        const auto cleanDebugState=[&] {
            return !app->pendingUnlimitedResources_ && !app->pendingRevealChest_ &&
                !app->pendingDefeatAllEnemies_ && !app->pendingToggleInvulnerability_ &&
                !app->pendingGrantInsight_ && !app->pendingDamageCore_ &&
                !app->pendingSpawnEnemy_ && !app->pendingChainLightning_ &&
                !app->enemySpawnMenuVisible_ && !app->itemGrantMenuVisible_ &&
                !app->sandboxCardMenuVisible_ && !app->objectiveDebugMenuVisible_ &&
                !app->performanceOverlayVisible_ && !app->showColliders_ &&
                !app->showFlowField_ && !app->showSpatialHash_ && !app->showTerrainWireframe_ && !app->hudHidden_;
        };
        for(int modifier:{KEY_NULL,KEY_LEFT_CONTROL,KEY_RIGHT_CONTROL,KEY_LEFT_SHIFT,KEY_RIGHT_SHIFT}) {
            for(int key:{KEY_F1,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_F9,
                    KEY_F10,KEY_F11,KEY_F12,KEY_B,KEY_O,KEY_I,KEY_Z,KEY_Y,
                    KEY_H,KEY_L,KEY_K,KEY_J,KEY_M,KEY_LEFT_BRACKET,KEY_RIGHT_BRACKET,KEY_APOSTROPHE,KEY_BACKSLASH}) {
                chord(key,modifier);
                debugKeys &= cleanDebugState() && app->renderer_->settings()==graphicsBefore &&
                    app->simulation_.terrain().seed()==seedBefore && app->debugSpawnType_==spawnTypeBefore &&
                    app->environment_.timeOfDay()==timeBefore && app->environment_.frozen()==frozenBefore;
                if(!debugKeys) {std::cerr<<"Disabled debug chord changed state: "<<key<<'/'<<modifier<<'\n';break;}
            }
        }
        // Rebound reveal and HUD debug actions obey the same switch.
        setControlKey(app->userSettings_.controls,ControlAction::RevealChest,KEY_T);
        setControlKey(app->userSettings_.controls,ControlAction::ToggleHud,KEY_T);
        chord(KEY_T);debugKeys &= cleanDebugState();
        app->userSettings_.controls={};
        // Ordinary play and settings stay usable with debugging switched off.
        chord(KEY_W);debugKeys &= app->input_.moveForward>0;
        chord(KEY_TAB);debugKeys &= app->actionMode_==ActionMode::Buildings;
        chord(KEY_TAB);debugKeys &= app->actionMode_==ActionMode::Equipment;
        chord(KEY_F2);
        debugKeys &= app->renderer_->graphicsPanelVisible() && !app->debugSettings_ &&
            app->simulation_.snapshot().state==RunState::Paused;
        app->closeSettings();
        chord(KEY_F3);
        debugKeys &= app->feedback_.active() && app->feedback_.needsCapture() &&
            !app->renderer_->graphicsPanelVisible() && !app->debugSettings_ &&
            app->simulation_.snapshot().state==RunState::Paused;
        const auto feedbackTick=app->simulation_.snapshot().tick;
        app->update();
        chord(KEY_B);chord(KEY_I);chord(KEY_TAB);
        debugKeys &= app->simulation_.snapshot().tick==feedbackTick &&
            !app->enemySpawnMenuVisible_ && !app->pendingToggleInvulnerability_ &&
            app->actionMode_==ActionMode::Equipment;
        // The screenshot is read before buffer swap and before overlay artwork.
        BeginDrawing();ClearBackground(RED);
        DrawRectangle(0,GetScreenHeight()/2,GetScreenWidth(),GetScreenHeight()/2,BLUE);
        app->captureFeedbackFrame();EndDrawing();
        debugKeys &= app->feedback_.ready() && app->feedback_.save("feedback");
        const auto report=app->feedback_.savedDirectory();
        auto shot=LoadImage((report/"screenshot.png").string().c_str());
        if(IsImageValid(shot)) {
            const auto top=GetImageColor(shot,shot.width/2,shot.height/4);
            const auto bottom=GetImageColor(shot,shot.width/2,shot.height*3/4);
            debugKeys &= top.r>200 && top.b<100 && bottom.b>200 && bottom.r<100;
            UnloadImage(shot);
        } else debugKeys=false;
        app->render(); // Frozen-overlay frames must still present and poll input.
        debugKeys &= GetFrameTime()>0 && app->feedback_.ready();
        app->closeFeedback();
        debugKeys &= app->feedbackClosing_ && app->feedback_.ready() &&
            app->simulation_.snapshot().state==RunState::BuildPhase;
        app->render(); // Closing input is swallowed before revealing the game.
        app->processInput();debugKeys &= !app->feedback_.active() && !app->feedbackClosing_;
        debugKeys &= glfwGetInputMode(glfwGetCurrentContext(),GLFW_CURSOR)==GLFW_CURSOR_DISABLED;
        debugKeys &= app->pendingYaw_==0 && app->pendingPitch_==0;
        // Cover repeated sessions as well as Save/Done: an outgoing cancelled
        // modal must never replace relative look with a merely hidden cursor.
        for(int repeat=0;repeat<2;++repeat) {
            app->beginFeedback();
            debugKeys &= app->feedback_.capture(GenImageColor(64,64,GREEN));
            app->closeFeedback();app->render();app->processInput();
            debugKeys &= glfwGetInputMode(glfwGetCurrentContext(),GLFW_CURSOR)==GLFW_CURSOR_DISABLED;
        }
        app->simulation_.togglePause();
        chord(KEY_F3);debugKeys &= app->feedback_.active() && !app->resumeAfterFeedback_;
        app->closeFeedback();debugKeys &= app->simulation_.snapshot().state==RunState::Paused;
        app->simulation_.togglePause();
        app->openSettings(false);
        const auto graphicsBeforeCancel = app->renderer_->settings();
        app->settingsMenuState_.graphicsDraft = graphicsBeforeCancel;
        app->settingsMenuState_.graphicsDraft->displayWidth = 960;
        app->settingsMenuState_.graphicsDraft->displayHeight = 540;
        app->settingsMenuState_.graphicsDraft->brightness = .25F;
        chord(KEY_F3);app->closeFeedback();
        debugKeys &= app->renderer_->graphicsPanelVisible() && app->simulation_.snapshot().state==RunState::Paused;
        app->closeSettings();
        UserSettings cancelledGraphics;
        debugKeys &= app->simulation_.snapshot().state==RunState::BuildPhase &&
            !app->settingsMenuState_.graphicsDraft && app->renderer_->settings()==graphicsBeforeCancel &&
            loadUserSettings("user_settings/game_settings.json", cancelledGraphics) &&
            cancelledGraphics.graphics == graphicsBeforeCancel;
        app->userSettings_.debugHotkeysEnabled=true;
        app->persistUserSettings(true);
        UserSettings enabledSettings;
        debugKeys &= loadUserSettings("user_settings/game_settings.json",enabledSettings) && enabledSettings.debugHotkeysEnabled;
        chord(KEY_I);debugKeys &= app->pendingToggleInvulnerability_;
        chord(KEY_O);debugKeys &= app->pendingUnlimitedResources_;
        chord(KEY_J,KEY_LEFT_CONTROL);debugKeys &= app->pendingGrantInsight_;
        chord(KEY_B);debugKeys &= app->enemySpawnMenuVisible_;
        chord(KEY_B);debugKeys &= !app->enemySpawnMenuVisible_;
        chord(KEY_F5);debugKeys &= app->itemGrantMenuVisible_;
        chord(KEY_F5);debugKeys &= !app->itemGrantMenuVisible_;
        chord(KEY_F5,KEY_LEFT_SHIFT);debugKeys &= app->sandboxCardMenuVisible_;
        chord(KEY_F5,KEY_LEFT_SHIFT);debugKeys &= !app->sandboxCardMenuVisible_;
        chord(KEY_F3,KEY_LEFT_CONTROL);debugKeys &= app->renderer_->settings().shadows!=graphicsBefore.shadows;
        app->renderer_->settings()=graphicsBefore;
        chord(KEY_F2,KEY_LEFT_CONTROL);debugKeys &= app->debugSettings_ && app->renderer_->graphicsPanelVisible();
        app->closeSettings();
        // Turning the option off closes existing debug menus and clears queued cheats.
        app->enemySpawnMenuVisible_=app->itemGrantMenuVisible_=app->sandboxCardMenuVisible_=true;
        app->objectiveDebugMenuVisible_=app->performanceOverlayVisible_=app->showColliders_=true;
        app->userSettings_.debugHotkeysEnabled=false;
        app->processInput();debugKeys &= cleanDebugState();
        app->persistUserSettings(true);
        UserSettings persisted;
        debugKeys &= loadUserSettings("user_settings/game_settings.json",persisted) && !persisted.debugHotkeysEnabled;
        // Feedback also preserves the exact menu/upgrade state it interrupted.
        app->simulation_.returnToMainMenu();
        chord(KEY_F3);
        debugKeys &= app->feedback_.active() && !app->resumeAfterFeedback_ &&
            app->simulation_.snapshot().state==RunState::MainMenu;
        app->closeFeedback();
        debugKeys &= app->simulation_.snapshot().state==RunState::MainMenu;
        app->simulation_.startRunFromSeed(PlayerClass::Vanguard,91234);
        PlayerCommand insight;
        insight.grantInsight=GrantInsightCommand{1000};
        app->simulation_.tick(1.0/60.0,insight);
        const auto upgradeCount=app->simulation_.snapshot().runUpgradeChoiceCount;
        debugKeys &= app->simulation_.snapshot().runUpgradeChoicePending;
        chord(KEY_F3);app->update();app->closeFeedback();
        debugKeys &= app->simulation_.snapshot().runUpgradeChoicePending &&
            app->simulation_.snapshot().runUpgradeChoiceCount==upgradeCount &&
            app->simulation_.snapshot().state!=RunState::Paused;
        okay &= debugKeys;
        if(!debugKeys) std::cerr<<"Debug shortcut gate, persistence or normal-input isolation failed\n";
        UnloadRenderTexture(target);app->renderer_.reset();app.reset();CloseWindow();
        return okay?0:1;
    }
};
int runPresentationEventsSmoke() {
    const auto original=std::filesystem::current_path();
    const auto sandbox=std::filesystem::temp_directory_path()/("ian-presentation-events-smoke-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox);
    if(!std::filesystem::exists(sandbox/"assets"))std::filesystem::create_directory_symlink(original/"assets",sandbox/"assets");
    std::filesystem::current_path(sandbox);
    const int result=AppPresentationTestAccess::run();
    std::filesystem::current_path(original);
    if(!std::getenv("IAN_CAPTURE_STUMP") && !std::getenv("IAN_TESLA_CAPTURE")) {
        std::error_code error;std::filesystem::remove_all(sandbox,error);
    }
    return result;
}
}
