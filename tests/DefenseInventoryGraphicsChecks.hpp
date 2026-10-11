#pragma once

#include "ui/RunDefenseInventory.hpp"
#include "localization/Localization.hpp"
#include <cstdlib>
#include <filesystem>

inline bool checkDefenseInventoryGraphics() {
    using namespace ian;
    initializeLocalization();
    GameUi ui;
    ui.initialize();
    SimulationSnapshot snapshot;
    snapshot.playerClass = PlayerClass::Vanguard;
    snapshot.tomeSlotLimit = 4;
    snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Damage)] = 3;
    snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::AttackSpeed)] = 2;
    const auto target = LoadRenderTexture(640, 240);
    bool visible = true;
    for (const bool full : {false, true}) {
        snapshot.buildingArsenalCount = full ? 4 : 1;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 2;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockTurret)] = full ? 4 : 0;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = full ? 8 : 0;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockSpikeTrap)] = full ? 3 : 0;
        for (int warm = 0; warm < 3; ++warm) {
            BeginTextureMode(target);
            ClearBackground({54, 99, 103, 255});
            drawRunHammerInventory(ui, snapshot, {20, 82, 600, 52.8F}, 1, Vector2{-1, -1});
            drawRunDefenseInventory(ui, snapshot, {20, 142, 600, 52.8F}, 1, Vector2{-1, -1});
            EndTextureMode();
        }
        Image capture = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&capture);
        for (int slot = 0; slot < (full ? 4 : 1); ++slot) {
            int bright = 0;
            const int left = 20 + static_cast<int>(static_cast<float>(slot) * 52.8F);
            for (int y = 146; y < 184; ++y) for (int x = left + 4; x < left + 46; ++x) {
                const auto pixel = GetImageColor(capture, x, y);
                bright += pixel.r > 150 || pixel.g > 150 || pixel.b > 150;
            }
            visible = visible && bright > 35;
        }
        // The rows keep an unobstructed gap between their square backgrounds.
        const auto gap = GetImageColor(capture, 25, 136);
        visible = visible && gap.r == 54 && gap.g == 99 && gap.b == 103;
        if (const char* directory = std::getenv("IAN_CAPTURE_DEFENSE_INVENTORY")) {
            std::filesystem::create_directories(directory);
            ExportImage(capture, (std::filesystem::path(directory) /
                (full ? "defenses-full.png" : "defenses-starter.png")).c_str());
        }
        UnloadImage(capture);
    }
    UnloadRenderTexture(target);
    ui.shutdown();
    return visible;
}
