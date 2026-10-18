#include "TestHarness.hpp"
#include "graphics/Renderer.hpp"
#include "ui/EnemySpawnMenu.hpp"
#include <cstdlib>
#include <filesystem>

using namespace ian;

int main() {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Goleling graphics checks");
    if (!IsWindowReady()) return 77;
    {
        Renderer renderer;
        renderer.initialize();
        const auto target = LoadRenderTexture(512, 512);
        Camera3D camera{{3, 3, 6}, {0, 2.4F, 0}, {0, 1, 0}, 4.8F, CAMERA_ORTHOGRAPHIC};
        const auto capture = [&](EnemyAnimationVisual animation, float seconds) {
            BeginTextureMode(target);
            ClearBackground(BLANK);
            BeginMode3D(camera);
            WorldLighting lighting{};
            lighting.cameraPosition = camera.position;
            renderer.beginWorldShader(lighting);
            require(renderer.drawEnemy(EnemyModelVisual::AirTank, animation, seconds,
                {0, .50F, 0}, .35F, WHITE, .90F), "Goleling model and native clips load in the game renderer");
            renderer.endWorldShader();
            EndMode3D();
            EndTextureMode();
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);
            return image;
        };
        Image first = capture(EnemyAnimationVisual::Walk, .13F);
        Image next = capture(EnemyAnimationVisual::Walk, .35F);
        Image strike = capture(EnemyAnimationVisual::MeleeAttack, .6F);
        int visible = 0, motion = 0, attack = 0;
        for (int y = 0; y < 512; ++y) for (int x = 0; x < 512; ++x) {
            const auto pixel = GetImageColor(first, x, y);
            visible += pixel.a > 20;
            motion += ColorToInt(pixel) != ColorToInt(GetImageColor(next, x, y));
            attack += ColorToInt(pixel) != ColorToInt(GetImageColor(strike, x, y));
        }
        require(visible > 10000 && motion > 500 && attack > 500,
            "air tank silhouette, flying wings and headbutt are visible and animated");
        if (const char* directory = std::getenv("IAN_CAPTURE_AIR_TANK")) {
            std::filesystem::create_directories(directory);
            ExportImage(first, (std::filesystem::path(directory) / "goleling.png").string().c_str());
            ExportImage(strike, (std::filesystem::path(directory) / "headbutt.png").string().c_str());
        }
        UnloadImage(first);
        UnloadImage(next);
        UnloadImage(strike);
        UnloadRenderTexture(target);
        GameUi ui;
        ui.initialize();
        const auto icon = ui.enemyTexture(EnemyType::AirTank);
        require(IsTextureValid(icon) && icon.width == 256 && icon.height == 256,
            "air tank has a dedicated compact forecast/spawner icon");
        Image portrait = LoadImageFromTexture(icon);
        int opaque = 0, transparent = 0;
        for (int y=0; y<256; ++y) for (int x=0; x<256; ++x) {
            const auto pixel = GetImageColor(portrait,x,y);
            opaque += pixel.a > 200;
            transparent += pixel.a == 0;
        }
        require(opaque > 10000 && transparent > 3000 && GetImageColor(portrait,0,0).a == 0,
            "Goleling icon retains readable size and actual transparent padding");
        UnloadImage(portrait);
        for (const Vector2 screen : {Vector2{1280,800}, Vector2{540,960}}) {
            const auto layout = enemy_spawn_menu::layout(screen);
            for (const auto& card : layout.cards)
                require(card.y >= layout.catalog.y &&
                    card.y + card.height <= layout.catalog.y + layout.catalog.height + .01F,
                    "all ten enemy entries fit the catalog in landscape and portrait");
        }
        SetWindowSize(1280,800);
        const auto menuTarget = LoadRenderTexture(1280,800);
        BeginTextureMode(menuTarget);
        ClearBackground({45,75,85,255});
        EnemyType selected = EnemyType::AirTank;
        EliteAffix variant = EliteAffix::None;
        int count = 1;
        drawEnemySpawnCatalog(ui,selected,variant,count,0,{1280,800});
        EndTextureMode();
        if (const char* directory = std::getenv("IAN_CAPTURE_AIR_TANK")) {
            Image menu = LoadImageFromTexture(menuTarget.texture);
            ImageFlipVertical(&menu);
            ExportImage(menu,(std::filesystem::path(directory)/"catalog.png").string().c_str());
            UnloadImage(menu);
        }
        UnloadRenderTexture(menuTarget);

    }
    CloseWindow();
}
