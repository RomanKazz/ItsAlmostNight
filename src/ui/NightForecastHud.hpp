#pragma once

#include "ui/GameUi.hpp"
#include "waves/NightEncounter.hpp"

namespace ian {

[[nodiscard]] inline float nightForecastWidth(const NightForecast& forecast) {
    const auto count = std::count_if(forecast.enemies.begin(), forecast.enemies.end(), [](int n) { return n > 0; });
    return std::max(54.0F, static_cast<float>(count) * 42.0F + 12.0F);
}

inline void drawNightForecast(GameUi& ui, const NightForecast& forecast, Rectangle bounds) {
    DrawRectangleRounded(bounds, 1.0F, 16, {0, 0, 0, 77});
    const auto count = std::count_if(forecast.enemies.begin(), forecast.enemies.end(), [](int n) { return n > 0; });
    float x = bounds.x + (bounds.width - static_cast<float>(count) * 42.0F) * .5F;
    for (std::size_t type = 0; type < forecast.enemies.size(); ++type) {
        if (forecast.enemies[type] <= 0) continue;
        const auto icon = ui.enemyTexture(static_cast<EnemyType>(type));
        if (IsTextureValid(icon)) {
            const auto source = Rectangle{0, 0, static_cast<float>(icon.width), static_cast<float>(icon.height)};
            DrawTexturePro(icon, source, {x + 3, bounds.y + (bounds.height - 36) * .5F, 36, 36}, {}, 0, WHITE);
        }
        x += 42;
    }
}

} // namespace ian
