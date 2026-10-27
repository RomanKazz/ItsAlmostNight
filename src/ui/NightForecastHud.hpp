#pragma once

#include "ui/GameUi.hpp"
#include "game/Simulation.hpp"
#include "waves/NightEncounter.hpp"

namespace ian {

// Retain the advertised roster while fading out: the simulation advances its
// upcoming forecast to the following night as soon as combat starts.
struct NightForecastPresentation {
    NightForecast forecast{};
    float progress{};

    void update(const SimulationSnapshot& snapshot, double seconds) {
        if (snapshot.sandboxMode || snapshot.coreMaxHealth <= 0 ||
            snapshot.state == RunState::MainMenu || snapshot.state == RunState::Defeat ||
            snapshot.state == RunState::Victory) {
            progress = 0;
            return;
        }
        if (snapshot.state == RunState::Paused || snapshot.runUpgradeChoicePending) return;
        const bool sunset = snapshot.state == RunState::Sunset;
        if (sunset) forecast = {snapshot.upcomingNightPattern, snapshot.upcomingEnemyCounts};
        const float step = static_cast<float>(std::max(0.0, seconds) / (sunset ? .28 : .22));
        progress = std::clamp(progress + (sunset ? step : -step), 0.0F, 1.0F);
    }
    [[nodiscard]] float opacity() const {
        return progress * progress * (3.0F - 2.0F * progress);
    }
};

[[nodiscard]] inline float nightForecastWidth(const NightForecast& forecast) {
    const auto count = std::count_if(forecast.enemies.begin(), forecast.enemies.end(), [](int n) { return n > 0; });
    return std::max(54.0F, static_cast<float>(count) * 42.0F + 12.0F);
}

inline void drawNightForecast(GameUi& ui, const NightForecast& forecast, Rectangle bounds, float opacity = 1.0F) {
    opacity = std::clamp(opacity, 0.0F, 1.0F);
    if (opacity <= 0.0F) return;
    DrawRectangleRounded(bounds, 1.0F, 16,
        {0, 0, 0, static_cast<unsigned char>(115.0F * opacity)});
    const auto count = std::count_if(forecast.enemies.begin(), forecast.enemies.end(), [](int n) { return n > 0; });
    float x = bounds.x + (bounds.width - static_cast<float>(count) * 42.0F) * .5F;
    for (std::size_t type = 0; type < forecast.enemies.size(); ++type) {
        if (forecast.enemies[type] <= 0) continue;
        const auto icon = ui.enemyTexture(static_cast<EnemyType>(type));
        if (IsTextureValid(icon)) {
            const auto source = Rectangle{0, 0, static_cast<float>(icon.width), static_cast<float>(icon.height)};
            DrawTexturePro(icon, source, {x + 3, bounds.y + (bounds.height - 36) * .5F, 36, 36}, {}, 0, Fade(WHITE, opacity));
        }
        x += 42;
    }
}

} // namespace ian
