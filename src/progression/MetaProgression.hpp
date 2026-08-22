#pragma once

#include "game/PlayerClass.hpp"

#include <string_view>

namespace ian {

struct MetaProgression {
    int runsPlayed{};
    int stageClears{};
    int bestWave{};
    int enemiesDefeated{};
    int lootCollected{};
    int resourcesGathered{};

    bool operator==(const MetaProgression&) const = default;
};

[[nodiscard]] bool isPlayerClassUnlocked(
    PlayerClass playerClass, const MetaProgression& progression);
[[nodiscard]] std::string_view playerClassUnlockRequirement(
    PlayerClass playerClass);
[[nodiscard]] float playerClassUnlockProgress(
    PlayerClass playerClass, const MetaProgression& progression);

[[nodiscard]] bool loadMetaProgression(
    std::string_view path, MetaProgression& progression);
[[nodiscard]] bool saveMetaProgression(
    std::string_view path, const MetaProgression& progression);

} // namespace ian
