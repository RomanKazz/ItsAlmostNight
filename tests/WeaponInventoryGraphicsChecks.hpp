#pragma once

#include "ui/RunWeaponInventory.hpp"
#include "ui/RunDefenseInventory.hpp"
#include "ui/HudHotbar.hpp"

#include <filesystem>

inline void checkWeaponInventory(ian::GameUi& ui, const char* captureDirectory) {
    using namespace ian;
    SetWindowSize(960, 540);
    const auto target = LoadRenderTexture(960, 540);
    for (int count : {2, 4, 6, 7}) {
        SimulationSnapshot snapshot{};
        snapshot.playerClass = PlayerClass::Vanguard;
        snapshot.tomeSlotLimit = 4;
        snapshot.weaponSlotLimit = count == 6 ? 6 : 4;
        snapshot.buildingArsenalCount = 1;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 1;
        snapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Damage)] = 2;
        snapshot.bombsRemaining = 12;
        snapshot.selectedWeapon = PlayerWeapon::Pickaxe;
        for (int i = 0; i < count; ++i) {
            const auto weapon = PlayerWeaponHotbarOrder[static_cast<std::size_t>(i)];
            snapshot.unlockedWeapons[static_cast<std::size_t>(weapon)] = true;
            snapshot.weaponLevels[static_cast<std::size_t>(weapon)] = 3;
            require(IsTextureValid(ui.weaponTexture(weapon)), "every equipment slot uses its existing weapon icon");
        }
        BeginDrawing(); BeginTextureMode(target);
        ClearBackground({61, 82, 66, 255});
        drawRunHammerInventory(ui, snapshot, {20, 82, 600, 52.8F}, 1, Vector2{900, 500});
        drawRunDefenseInventory(ui, snapshot, {20, 142, 600, 52.8F}, 1, Vector2{900, 500});
        drawRunWeaponInventory(ui, snapshot, {20, 202, 600, 52.8F}, Vector2{900, 500});
        HudViewState view{};
        view.weaponHotbarSelectionPosition = 1;
        view.weaponHotbarSelectionAlpha = 1;
        hud_detail::drawWeaponHotbar(ui, snapshot, view);
        EndTextureMode(); EndDrawing();
        Image image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
        if (captureDirectory) {
            std::filesystem::create_directories(captureDirectory);
            const auto path = std::filesystem::path(captureDirectory) / ("equipment-slots-" + std::to_string(count) + ".png");
            require(ExportImage(image, path.string().c_str()), "equipment inventory capture exports");
        }
        int bright = 0;
        for (int y = 218; y < 235; ++y) for (int x = 70; x < 77; ++x) {
            const auto color = GetImageColor(image, x, y);
            bright += color.r > 210 && color.g > 210;
        }
        const auto untouched = GetImageColor(image, 73, 265);
        require(bright < 4 && untouched.r == 61 && untouched.g == 82,
            "upper equipment row has no hotbar selection outline and stays within the inventory grid");
        int keyPixels = 0;
        for (int y = 202; y < 211; ++y) for (int x = 20; x < 28; ++x) {
            const auto color = GetImageColor(image, x, y);
            keyPixels += color.r > 210 && color.g > 210;
        }
        require(keyPixels == 0, "upper equipment cells have no hotbar key badges");
        const int firstHotbarX = (960 - (count * 96 + (count - 1) * 7)) / 2;
        const auto hotbar = GetImageColor(image, firstHotbarX + 2, 470);
        require(hotbar.r > 90 && hotbar.g < 80,
            "the original bottom equipment hotbar remains visible alongside the new upper inventory row");
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
}
