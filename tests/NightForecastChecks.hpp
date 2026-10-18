#pragma once

#include "ui/NightForecastHud.hpp"
#include "localization/Localization.hpp"

#include <filesystem>

inline void checkNightForecast(ian::GameUi& ui, const char* captureDirectory) {
    using namespace ian;
    for (int role = 0; role < static_cast<int>(GameBalance::EnemyTypeCount); ++role) {
        const auto texture = ui.enemyTexture(static_cast<EnemyType>(role));
        require(IsTextureValid(texture) && texture.width == 256 && texture.height == 256,
            "every forecast enemy has a dedicated compact icon");
        Image image = LoadImageFromTexture(texture);
        const Color* pixels = LoadImageColors(image);
        int opaque = 0, transparent = 0;
        for (int pixel = 0; pixel < image.width * image.height; ++pixel) {
            opaque += pixels[pixel].a > 200;
            transparent += pixels[pixel].a == 0;
        }
        require(opaque > 10000 && transparent > 3000 && GetImageColor(image, 0, 0).a == 0,
            "enemy icons retain a readable silhouette and genuine transparent padding");
        UnloadImageColors(const_cast<Color*>(pixels));
        UnloadImage(image);
    }
    require(ui.enemyTexture(EnemyType::Splitling).id == ui.enemyTexture(EnemyType::Splitter).id,
        "splitter children share their parent's portrait");
    SetWindowSize(960, 540);
    const auto target = LoadRenderTexture(960, 540);
    WaveDirector director;
    for (const auto language : {Language::English, Language::Russian}) {
        setLanguage(language);
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground({45, 75, 85, 255});
        for (int row = 0; row < 4; ++row) {
            auto forecast = director.forecast(row == 0 ? 1 : row == 1 ? 3 : row == 2 ? 7 : WaveDirector::BossWaveInterval, 73);
            if (row == 2) forecast.enemies.fill(1);
            const float width = nightForecastWidth(forecast);
            drawNightForecast(ui, forecast, {(960 - width) * .5F, 55 + row * 108.0F, width, 48});
        }
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        const auto pill = GetImageColor(image, 455, 79);
        const auto aboveIcon = GetImageColor(image, 480, 58);
        const auto outside = GetImageColor(image, 455, 58);
        require(pill.r >= 24 && pill.r <= 26 && pill.g >= 40 && pill.g <= 43 &&
                pill.b >= 46 && pill.b <= 49 && aboveIcon.r == pill.r &&
                aboveIcon.g == pill.g && outside.r == 45 && outside.g == 75,
            "forecast uses a rounded 45-percent black pill without a title or border");
        if (captureDirectory) {
            std::filesystem::create_directories(captureDirectory);
            const auto file = std::filesystem::path(captureDirectory) /
                (language == Language::English ? "night-forecast-en.png" : "night-forecast-ru.png");
            require(ExportImage(image, file.string().c_str()), "night preview capture exports");
        }
        UnloadImage(image);
    }
    setLanguage(Language::English);
    UnloadRenderTexture(target);
}
