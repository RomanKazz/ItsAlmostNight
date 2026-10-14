#include "TestHarness.hpp"
#include "ui/PlayerDeathOverlay.hpp"
#include "localization/Localization.hpp"
#include <filesystem>

int main() {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(960,600,"Player death card");
    if (!IsWindowReady()) return 77;
    initializeLocalization();
    GameUi ui;ui.initialize();
    SimulationSnapshot snapshot{};
    snapshot.state=RunState::Wave;
    snapshot.playerRespawning=true;
    snapshot.playerRespawnDuration=5;
    snapshot.playerRespawnTimeRemaining=3.7;
    snapshot.deathLostWood=15;snapshot.deathLostStone=8;snapshot.deathLostCrystals=3;
    const auto capture=[&](Language language,float reveal=0) {
        setLanguage(language);
        const auto target=LoadRenderTexture(960,600);
        BeginTextureMode(target);ClearBackground({61,92,105,255});
        beginSdfUiTextBatch();
        drawPlayerDeathOverlay(ui,snapshot,{960,600},reveal);
        endSdfUiTextBatch();EndTextureMode();
        Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);
        UnloadRenderTexture(target);return result;
    };
    std::filesystem::create_directories("/tmp/ian-death-card");
    for (const auto language : {Language::English,Language::Russian}) {
        Image card=capture(language);
        ExportImage(card,language==Language::English?"/tmp/ian-death-card/en.png":"/tmp/ian-death-card/ru.png");
        int cream=0,red=0;
        const auto pixels=LoadImageColors(card);
        for (int i=0;i<card.width*card.height;++i) {
            cream+=pixels[i].r>200 && pixels[i].g>180 && pixels[i].b>140;
            red+=pixels[i].r>170 && pixels[i].r>pixels[i].g*1.3;
        }
        require(cream>400 && red>200,"death card has readable cream text and red headline in both languages");
        UnloadImageColors(pixels);UnloadImage(card);
    }
    snapshot.playerRespawnTimeRemaining=0;
    Image black=capture(Language::English);
    require(GetImageColor(black,480,300).r<2,"relocation is hidden by a full black frame");UnloadImage(black);
    snapshot.playerRespawning=false;snapshot.playerRespawnProtectionRemaining=2.5;
    Image protectedFrame=capture(Language::English);
    ExportImage(protectedFrame,"/tmp/ian-death-card/protected.png");UnloadImage(protectedFrame);
    Image reveal=capture(Language::English,PlayerRespawnRevealSeconds);
    require(GetImageColor(reveal,480,300).r<2,"respawn reveal starts from the same black frame");UnloadImage(reveal);
    ui.shutdown();shutdownUiText();CloseWindow();
}
