#include "app/App.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <cstdlib>
#include <rlgl.h>
namespace ian {
int runFrameBenchmark(const char* output, int frames);
// Reuse the existing test-only access point; no gameplay debug command/state.
struct AppFrameBenchmarkAccess {
 static int benchmark(const char* output, int frames) {
  auto app=std::make_unique<App>();
  SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_MSAA_4X_HINT);
  InitWindow(1280,720,"Frame benchmark"); if(!IsWindowReady()) return 77;
  SetWindowPosition(0,0); SetWindowSize(1920,1200);
  app->simulation_.startRunFromSeed(PlayerClass::None,17866);
  app->renderer_.emplace(); auto& renderer=*app->renderer_;
  renderer.settings()=app->userSettings_.graphics;
  renderer.settings().frameRateLimit=0; renderer.settings().performanceLogging=false;
  renderer.initialize(); renderer.applyFrameRateLimit();
  app->modularBuildingRenderer_.setRenderer(&renderer);
  app->rebuildTerrainGraphics(); app->ui_.initialize();
  app->playerArrival_.reset(); app->displayedToolVisual_=FirstPersonToolVisual::Axe;
  std::ofstream csv(output); csv<<"scene,frame,frame_ms,ui_ms,text_ms,world_ms,overlays_ms,present_ms,visible_enemies\n";
  std::vector<EnemyInstance> enemies(1000);
  const auto player=app->simulation_.snapshot().playerPosition;
  for(std::size_t i=0;i<enemies.size();++i) {
   auto& e=enemies[i]; e.id={static_cast<std::uint32_t>(i+1),1}; e.type=static_cast<EnemyType>(i%3);
   e.position={player.x+(static_cast<double>(i%40)-20)*2.85,0,player.z-8-static_cast<double>(i/40)*2.85};
   e.worldSurfaceHeight=app->simulation_.terrain().getHeight(e.position.x,e.position.z);
   e.health=e.maxHealth=100; e.active=true; e.movementMultiplier=1; e.yaw=0;
  }
  for(int scene=0;scene<2;++scene) for(int f=-180;f<frames;++f) {
   auto& snapshot=const_cast<SimulationSnapshot&>(app->simulation_.snapshot());
   snapshot.playerYaw=std::sin(double(f)*.0015)*.35;
   snapshot.enemies=scene?std::span<const EnemyInstance>{enemies}:std::span<const EnemyInstance>{};
   snapshot.activeEnemyCount=scene?enemies.size():0;
   const auto start=PerformanceClock::now(); app->render();
   const auto& s=app->performanceStats_;
   if(f>=0) csv<<scene<<','<<f<<','<<performanceMilliseconds(start)<<','<<s.uiRender.lastMilliseconds<<','
    <<s.uiTextRender.lastMilliseconds<<','<<s.worldEntitiesRender.lastMilliseconds<<','
    <<s.overlayRender.lastMilliseconds<<','<<s.present.lastMilliseconds<<','<<s.visibleEnemies<<'\n';
  }
  app->ui_.shutdown(); renderer.shutdown(); app.reset(); CloseWindow(); return 0;
 }
};
}
int ian::runFrameBenchmark(const char* output, int frames) {
 const auto root=std::filesystem::current_path();
 const auto destination=std::filesystem::absolute(output);
 const auto sandbox=std::filesystem::temp_directory_path()/"ian-frame-benchmark-current";
 std::filesystem::create_directories(sandbox);
 if(!std::filesystem::exists(sandbox/"assets")) std::filesystem::create_directory_symlink(root/"assets",sandbox/"assets");
 const auto settings=root/"user_settings/game_settings.json";
 if(std::filesystem::exists(settings)) {
  std::filesystem::create_directories(sandbox/"user_settings");
  std::filesystem::copy_file(settings,sandbox/"user_settings/game_settings.json",std::filesystem::copy_options::overwrite_existing);
 }
 std::filesystem::current_path(sandbox);
 const int result=ian::AppFrameBenchmarkAccess::benchmark(destination.string().c_str(),frames);
 std::filesystem::current_path(root); return result;
}
