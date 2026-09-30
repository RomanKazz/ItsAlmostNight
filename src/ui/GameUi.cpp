#include "ui/GameUi.hpp"

#include "combat/PlayerWeaponSystem.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiCString.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

class ScopedRayguiFont {
public:
    ScopedRayguiFont() : previous_(GuiGetFont()) { GuiSetFont(uiFont()); }
    ~ScopedRayguiFont() { GuiSetFont(previous_); }
    ScopedRayguiFont(const ScopedRayguiFont&) = delete;
    ScopedRayguiFont& operator=(const ScopedRayguiFont&) = delete;
private:
    Font previous_;
};

constexpr float UiTextScale = 1.68F;
constexpr int MinimapTargetSize = 512;
constexpr int MaximumMinimapTargetSize = 768;
constexpr int MinimapFrameTargetSize = 1024;

std::string localizedCopy(std::string_view text) {
    return currentLanguage() == Language::English
        ? std::string{text}
        : localizeText(text);
}

int fittedRayguiTextSize(
    std::string_view text, Rectangle bounds,
    float horizontalPadding, float verticalPadding) {
    const int preferred = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const float fitted = fitUiTextSize(
        text,
        static_cast<float>(preferred) / UiTextScale,
        7.0F,
        std::max(1.0F, bounds.width - horizontalPadding),
        std::max(1.0F, bounds.height - verticalPadding));
    return std::max(
        10, static_cast<int>(std::floor(fitted * UiTextScale)));
}

Color alphaTint(unsigned char alpha) {
    return {255, 255, 255, alpha};
}

NPatchInfo texturePatch(Texture2D texture, int left, int top,
                        int right, int bottom) {
    return {
        .source = {0.0F, 0.0F, static_cast<float>(texture.width),
                   static_cast<float>(texture.height)},
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .layout = NPATCH_NINE_PATCH,
    };
}

void drawPatched(Texture2D texture, Rectangle bounds,
                 int left, int top, int right, int bottom,
                 Color tint = WHITE) {
    if (!IsTextureValid(texture) || bounds.width <= 0.0F ||
        bounds.height <= 0.0F) {
        return;
    }
    DrawTextureNPatch(texture,
                      texturePatch(texture, left, top, right, bottom),
                      bounds, {0.0F, 0.0F}, 0.0F, tint);
}

} // namespace

GameUi::~GameUi() {
    shutdown();
}

void GameUi::initialize() {
    shutdown();
    panel_ = loadTexture("assets/ui/cartoony/panel.png");
    insetPanel_ = loadTexture("assets/ui/cartoony/inset.png");
    button_ = loadTexture("assets/ui/cartoony/button.png");
    buttonHover_ = loadTexture("assets/ui/cartoony/button-hover.png");
    buttonPressed_ = loadTexture("assets/ui/cartoony/button-pressed.png");
    buttonActive_ = loadTexture("assets/ui/cartoony/button-active.png");
    buttonActiveHover_ =
        loadTexture("assets/ui/cartoony/button-active-hover.png");
    buttonActivePressed_ =
        loadTexture("assets/ui/cartoony/button-active-pressed.png");
    keyCap_ = loadTexture("assets/ui/cartoony/button.png");
    keyCapPressed_ =
        loadTexture("assets/ui/cartoony/button-active-pressed.png");
    barBack_ = loadTexture("assets/ui/cartoony/bar-back.png");
    barBlue_ = loadTexture("assets/ui/cartoony/bar-blue.png");
    barRed_ = loadTexture("assets/ui/cartoony/bar-red.png");
    barGreen_ = loadTexture("assets/ui/cartoony/bar-green.png");
    barYellow_ = loadTexture("assets/ui/cartoony/bar-yellow.png");
    barPurple_ = loadTexture("assets/ui/cartoony/bar-purple.png");
    sliderKnob_ = loadTexture("assets/ui/cartoony/slider-knob.png");
    sliderKnobHover_ = loadTexture("assets/ui/cartoony/slider-knob.png");
    cursorHand_ = loadTexture("assets/ui/cursorHand_beige.png");
    checkIcon_ = loadTexture("assets/ui/cartoony/check.png");
    arrowLeft_ = loadTexture("assets/ui/arrowBrown_left.png");
    arrowRight_ = loadTexture("assets/ui/arrowBrown_right.png");
    resourceWood_ = loadTexture("assets/ui/resource_wood.png");
    resourceStone_ = loadTexture("assets/ui/resource_stone.png");
    resourceCrystal_ =
        loadTexture("assets/ui/resource_crystal.png");
    resourceCoin_ = loadTexture("assets/ui/resource_coin.png");
    healthPlayer_ = loadTexture("assets/ui/health_player.png");
    healthCore_ = loadTexture("assets/ui/health_core.png");
    const auto smoothHudIcon = [](Texture2D& texture) {
        if (!IsTextureValid(texture)) {
            return;
        }
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    };
    smoothHudIcon(resourceWood_);
    smoothHudIcon(resourceStone_);
    smoothHudIcon(resourceCrystal_);
    smoothHudIcon(resourceCoin_);
    smoothHudIcon(healthPlayer_);
    smoothHudIcon(healthCore_);
    coreBuildingIcon_ = loadTexture("assets/ui/building_core.png");
    smoothHudIcon(coreBuildingIcon_);
    wallBuildingIcon_ = loadTexture("assets/ui/building_wall.png");
    smoothHudIcon(wallBuildingIcon_);
    gateBuildingIcon_ = loadTexture("assets/ui/building_gate.png");
    smoothHudIcon(gateBuildingIcon_);
    constexpr std::array ModularBuildingIconPaths{
        "assets/ui/building_foundation.png",
        "assets/ui/building_floor.png",
        "assets/ui/building_ramp.png",
    };
    for (std::size_t index = 0;
         index < modularBuildingIcons_.size(); ++index) {
        modularBuildingIcons_[index] =
            loadTexture(ModularBuildingIconPaths[index]);
        smoothHudIcon(modularBuildingIcons_[index]);
    }
    constexpr std::array BuildingIconPaths{
        "assets/ui/building_ballista.png",
        "assets/ui/building_cannon.png",
        "assets/ui/building_catapult.png",
        "assets/ui/building_spikes.png",
        "assets/ui/building_turret.png",
        "assets/ui/building_crystal_mine.png",
        "assets/ui/building_lumber_mill.png",
        "assets/ui/building_quarry.png",
        "assets/ui/building_slow_trap.png",
    };
    for (std::size_t index = 0; index < buildingIcons_.size(); ++index) {
        buildingIcons_[index] = loadTexture(BuildingIconPaths[index]);
        smoothHudIcon(buildingIcons_[index]);
    }
    constexpr std::array WeaponIconPaths{
        "assets/ui/weapon_axe.png",
        "assets/ui/weapon_pickaxe.png",
        "assets/ui/weapon_club.png",
        "assets/ui/weapon_rifle.png",
        "assets/ui/weapon_bomb.png",
        "assets/ui/weapon_ice_wand.png",
        "assets/ui/weapon_fire_wand.png",
    };
    for (std::size_t index = 0; index < weaponIcons_.size(); ++index) {
        weaponIcons_[index] = loadTexture(WeaponIconPaths[index]);
        smoothHudIcon(weaponIcons_[index]);
    }
    armorUpgradeIcon_ = loadTexture("assets/ui/upgrade_armor.png");
    extraUpgradeIcon_ = loadTexture("assets/ui/upgrade_extra.png");
    heartUpgradeIcon_ = loadTexture("assets/ui/upgrade_heart.png");
    lifestealUpgradeIcon_ = loadTexture(
        "assets/ui/upgrade_lifesteal.png");
    magnetUpgradeIcon_ = loadTexture("assets/ui/upgrade_magnet.png");
    moveUpgradeIcon_ = loadTexture("assets/ui/upgrade_move.png");
    pierceUpgradeIcon_ = loadTexture("assets/ui/upgrade_pierce.png");
    ricochetUpgradeIcon_ = loadTexture(
        "assets/ui/upgrade_ricochet.png");
    xpGainUpgradeIcon_ = loadTexture("assets/ui/upgrade_xp_gain.png");
    smoothHudIcon(armorUpgradeIcon_);
    smoothHudIcon(extraUpgradeIcon_);
    smoothHudIcon(heartUpgradeIcon_);
    smoothHudIcon(lifestealUpgradeIcon_);
    smoothHudIcon(magnetUpgradeIcon_);
    smoothHudIcon(moveUpgradeIcon_);
    smoothHudIcon(pierceUpgradeIcon_);
    smoothHudIcon(ricochetUpgradeIcon_);
    smoothHudIcon(xpGainUpgradeIcon_);
    levelUpBanner_ = loadTexture("assets/ui/levelup.png");
    smoothHudIcon(levelUpBanner_);
    levelUpRays_ = loadTexture("assets/ui/levelup_rays.png");
    smoothHudIcon(levelUpRays_);
    constexpr std::array CoreMenuTexturePaths{
        "assets/ui/core_menu/core_menu_header.png",
        "assets/ui/core_menu/core_menu_gear.png",
        "assets/ui/core_menu/core_menu_heart.png",
        "assets/ui/core_menu/core_menu_gem.png",
        "assets/ui/core_menu/core_menu_shield.png",
        "assets/ui/core_menu/core_menu_rock.png",
        "assets/ui/core_menu/core_menu_wood.png",
        "assets/ui/core_menu/core_menu_wrench.png",
        "assets/ui/core_menu/core_menu_upgrade.png",
        "assets/ui/core_menu/core_menu_coin.png",
        "assets/ui/core_menu/core_menu_turret.png",
        "assets/ui/core_menu/core_menu_ballista.png",
        "assets/ui/core_menu/core_menu_cannon.png",
        "assets/ui/core_menu/core_menu_spikes.png",
    };
    for (std::size_t index = 0;
         index < coreMenuTextures_.size(); ++index) {
        coreMenuTextures_[index] =
            loadTexture(CoreMenuTexturePaths[index]);
        smoothHudIcon(coreMenuTextures_[index]);
    }
    minimapTarget_ = LoadRenderTexture(
        MinimapTargetSize, MinimapTargetSize);
    minimapFrameTarget_ = LoadRenderTexture(
        MinimapFrameTargetSize, MinimapFrameTargetSize);
    minimapTargetCacheValid_ = false;
    minimapFrameTargetCacheValid_ = false;
    minimapTargetLastRefreshSeconds_ = 0.0;
    if (IsTextureValid(minimapTarget_.texture)) {
        SetTextureFilter(
            minimapTarget_.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (IsTextureValid(minimapFrameTarget_.texture)) {
        SetTextureFilter(
            minimapFrameTarget_.texture, TEXTURE_FILTER_BILINEAR);
    }
    minimapCircleShader_ = LoadShader(
        nullptr, "assets/shaders/minimap_circle.fs");
    panelInsets_ = {16, 16, 16, 18};
    insetInsets_ = {9, 9, 9, 9};
    buttonInsets_ = {12, 10, 12, 14};
    barInsets_ = {12, 4, 12, 4};
    initializeUiText();
    initialized_ = true;

    GuiSetFont(uiFont());
    GuiSetStyle(DEFAULT, TEXT_SIZE, 31);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,
                static_cast<int>(0x354c5effU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,
                static_cast<int>(0x213747ffU));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,
                static_cast<int>(0x182b39ffU));
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x00000000);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x00000000);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,
                static_cast<int>(0xffffffffU));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,
                static_cast<int>(0xe8f7ffffU));
}

void GameUi::shutdown() {
    minimapTargetCacheValid_ = false;
    minimapFrameTargetCacheValid_ = false;
    minimapTargetLastRefreshSeconds_ = 0.0;
    if (IsShaderValid(minimapCircleShader_)) {
        UnloadShader(minimapCircleShader_);
    }
    minimapCircleShader_ = {};
    if (IsRenderTextureValid(minimapTarget_)) {
        UnloadRenderTexture(minimapTarget_);
    }
    minimapTarget_ = {};
    if (IsRenderTextureValid(minimapFrameTarget_)) {
        UnloadRenderTexture(minimapFrameTarget_);
    }
    minimapFrameTarget_ = {};
    unloadTexture(healthCore_);
    unloadTexture(healthPlayer_);
    unloadTexture(coreBuildingIcon_);
    unloadTexture(wallBuildingIcon_);
    unloadTexture(gateBuildingIcon_);
    for (auto& icon : modularBuildingIcons_) unloadTexture(icon);
    unloadTexture(resourceCrystal_);
    unloadTexture(resourceCoin_);
    unloadTexture(resourceStone_);
    unloadTexture(resourceWood_);
    unloadTexture(levelUpRays_);
    unloadTexture(levelUpBanner_);
    for (auto& texture : coreMenuTextures_) {
        unloadTexture(texture);
    }
    unloadTexture(xpGainUpgradeIcon_);
    unloadTexture(ricochetUpgradeIcon_);
    unloadTexture(pierceUpgradeIcon_);
    unloadTexture(moveUpgradeIcon_);
    unloadTexture(magnetUpgradeIcon_);
    unloadTexture(lifestealUpgradeIcon_);
    unloadTexture(heartUpgradeIcon_);
    unloadTexture(extraUpgradeIcon_);
    unloadTexture(armorUpgradeIcon_);
    for (auto& icon : buildingIcons_) unloadTexture(icon);
    for (auto& icon : weaponIcons_) unloadTexture(icon);
    unloadTexture(arrowRight_);
    unloadTexture(arrowLeft_);
    unloadTexture(checkIcon_);
    unloadTexture(cursorHand_);
    unloadTexture(sliderKnobHover_);
    unloadTexture(sliderKnob_);
    unloadTexture(barPurple_);
    unloadTexture(barYellow_);
    unloadTexture(barGreen_);
    unloadTexture(barRed_);
    unloadTexture(barBlue_);
    unloadTexture(barBack_);
    unloadTexture(keyCapPressed_);
    unloadTexture(keyCap_);
    unloadTexture(buttonActivePressed_);
    unloadTexture(buttonActiveHover_);
    unloadTexture(buttonActive_);
    unloadTexture(buttonPressed_);
    unloadTexture(buttonHover_);
    unloadTexture(button_);
    unloadTexture(insetPanel_);
    unloadTexture(panel_);
    shutdownUiText();
    initialized_ = false;
}

bool GameUi::beginMinimapTarget() const {
    if (!IsRenderTextureValid(minimapTarget_)) {
        return false;
    }
    BeginTextureMode(minimapTarget_);
    ClearBackground(BLANK);
    return true;
}

void GameUi::endMinimapTarget() const {
    EndTextureMode();
}

void GameUi::drawMinimapTarget(
    Rectangle bounds, float rotationDegrees) const {
    if (!IsRenderTextureValid(minimapTarget_)) {
        return;
    }
    if (IsShaderValid(minimapCircleShader_)) {
        BeginShaderMode(minimapCircleShader_);
    }
    const Rectangle destination{
        bounds.x + bounds.width * 0.5F,
        bounds.y + bounds.height * 0.5F,
        bounds.width, bounds.height};
    DrawTexturePro(
        minimapTarget_.texture,
        {0.0F, 0.0F,
         static_cast<float>(minimapTarget_.texture.width),
         -static_cast<float>(minimapTarget_.texture.height)},
        destination,
        {bounds.width * 0.5F, bounds.height * 0.5F},
        rotationDegrees, WHITE);
    if (IsShaderValid(minimapCircleShader_)) {
        EndShaderMode();
    }
}

void GameUi::updateMinimapTarget(int desiredSize) {
    desiredSize = std::clamp(
        desiredSize, MinimapTargetSize, MaximumMinimapTargetSize);
    if (IsRenderTextureValid(minimapTarget_) &&
        minimapTarget_.texture.width == desiredSize &&
        minimapTarget_.texture.height == desiredSize) {
        return;
    }
    if (IsRenderTextureValid(minimapTarget_)) {
        UnloadRenderTexture(minimapTarget_);
    }
    minimapTarget_ = LoadRenderTexture(desiredSize, desiredSize);
    minimapTargetCacheValid_ = false;
    if (IsTextureValid(minimapTarget_.texture)) {
        SetTextureFilter(
            minimapTarget_.texture, TEXTURE_FILTER_BILINEAR);
    }
}

int GameUi::minimapTargetSize() const {
    return IsRenderTextureValid(minimapTarget_)
        ? minimapTarget_.texture.width
        : 0;
}

bool GameUi::minimapTargetRefreshDue(
    double nowSeconds, double intervalSeconds,
    bool forceRefresh) const {
    return forceRefresh || !minimapTargetCacheValid_ ||
        nowSeconds < minimapTargetLastRefreshSeconds_ ||
        nowSeconds - minimapTargetLastRefreshSeconds_ >=
            std::max(intervalSeconds, 0.0);
}

void GameUi::markMinimapTargetRefreshed(double nowSeconds) {
    minimapTargetCacheValid_ = true;
    minimapTargetLastRefreshSeconds_ = nowSeconds;
}

bool GameUi::beginMinimapFrameTarget() const {
    if (!IsRenderTextureValid(minimapFrameTarget_)) {
        return false;
    }
    BeginTextureMode(minimapFrameTarget_);
    ClearBackground(BLANK);
    return true;
}

void GameUi::endMinimapFrameTarget() {
    EndTextureMode();
    if (IsTextureValid(minimapFrameTarget_.texture)) {
        GenTextureMipmaps(&minimapFrameTarget_.texture);
        SetTextureFilter(
            minimapFrameTarget_.texture, TEXTURE_FILTER_TRILINEAR);
    }
    minimapFrameTargetCacheValid_ = true;
}

void GameUi::drawMinimapFrameTarget(
    Rectangle bounds, float rotationDegrees, Color tint) const {
    if (!minimapFrameTargetCacheValid_ ||
        !IsRenderTextureValid(minimapFrameTarget_)) {
        return;
    }
    DrawTexturePro(
        minimapFrameTarget_.texture,
        {0.0F, 0.0F,
         static_cast<float>(minimapFrameTarget_.texture.width),
         -static_cast<float>(minimapFrameTarget_.texture.height)},
        {bounds.x + bounds.width * 0.5F,
         bounds.y + bounds.height * 0.5F,
         bounds.width, bounds.height},
        {bounds.width * 0.5F, bounds.height * 0.5F},
        rotationDegrees, tint);
}

bool GameUi::minimapFrameTargetReady() const noexcept {
    return minimapFrameTargetCacheValid_ &&
        IsRenderTextureValid(minimapFrameTarget_);
}

int GameUi::minimapFrameTargetSize() const noexcept {
    return IsRenderTextureValid(minimapFrameTarget_)
        ? minimapFrameTarget_.texture.width
        : 0;
}

void GameUi::drawPanel(Rectangle bounds, unsigned char alpha) const {
    if (IsTextureValid(panel_)) {
        DrawTextureNPatch(
            panel_, texturePatch(panel_, panelInsets_.left,
                                 panelInsets_.top, panelInsets_.right,
                                 panelInsets_.bottom),
            bounds, {0.0F, 0.0F}, 0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {239, 244, 247, alpha});
    }
}

void GameUi::drawInsetPanel(Rectangle bounds,
                            unsigned char alpha) const {
    if (IsTextureValid(insetPanel_)) {
        DrawTextureNPatch(
                          insetPanel_,
                          texturePatch(insetPanel_, insetInsets_.left,
                                       insetInsets_.top,
                                       insetInsets_.right,
                                       insetInsets_.bottom), bounds,
                          {0.0F, 0.0F}, 0.0F, alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {209, 220, 227, alpha});
    }
}

void GameUi::drawCardPanel(Rectangle bounds, Color accent,
                           unsigned char alpha) const {
    if (IsTextureValid(panel_)) {
        DrawTextureNPatch(panel_,
                          texturePatch(panel_, panelInsets_.left,
                                       panelInsets_.top,
                                       panelInsets_.right,
                                       panelInsets_.bottom), bounds,
                          {0.0F, 0.0F}, 0.0F,
                          alphaTint(alpha));
    } else {
        DrawRectangleRounded(bounds, 0.08F, 8,
                             {24, 31, 48, alpha});
    }
    accent.a = static_cast<unsigned char>(
        static_cast<unsigned int>(accent.a) * alpha / 255U);
    DrawRectangleRoundedLinesEx(bounds, 0.08F, 8, 2.5F, accent);
}

void GameUi::drawLabel(Rectangle bounds, std::string_view text,
                       int alignment) const {
    const ScopedRayguiFont fontScope;
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, alignment);
    const std::string localized = localizedCopy(text);
    const int previousSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    GuiSetStyle(
        DEFAULT, TEXT_SIZE,
        fittedRayguiTextSize(localized, bounds, 16.0F, 8.0F));
    withNullTerminatedUiText(localized, [bounds](const char* value) {
        (void)GuiLabel(bounds, value);
    });
    GuiSetStyle(DEFAULT, TEXT_SIZE, previousSize);
}

void GameUi::drawProgressBar(Rectangle bounds, float fraction,
                             UiBarColor color) const {
    fraction = std::clamp(fraction, 0.0F, 1.0F);
    if (IsTextureValid(barBack_)) {
        drawPatched(barBack_, bounds, barInsets_.left,
                    barInsets_.top, barInsets_.right,
                    barInsets_.bottom);
    } else {
        DrawRectangleRounded(bounds, 0.45F, 8,
                             {17, 21, 54, 245});
    }

    Texture2D fill = barGreen_;
    if (color == UiBarColor::Red) {
        fill = barRed_;
    } else if (color == UiBarColor::Yellow) {
        fill = barYellow_;
    } else if (color == UiBarColor::Purple) {
        fill = barPurple_;
    } else if (color == UiBarColor::Blue) {
        fill = barBlue_;
    }
    const float inset = std::clamp(bounds.height * 0.12F, 2.0F, 5.0F);
    Rectangle filled{
        bounds.x + inset, bounds.y + inset,
        std::max(0.0F, (bounds.width - inset * 2.0F) * fraction),
        std::max(0.0F, bounds.height - inset * 2.0F)};
    if (filled.width <= 0.0F) {
        return;
    }
    if (IsTextureValid(fill)) {
        drawPatched(fill, filled, barInsets_.left,
                    barInsets_.top, barInsets_.right,
                    barInsets_.bottom);
    } else {
        const Color fallback =
            color == UiBarColor::Red
                ? Color{190, 55, 50, 255}
                : (color == UiBarColor::Yellow
                       ? Color{218, 170, 58, 255}
                       : (color == UiBarColor::Purple
                              ? Color{139, 112, 218, 255}
                              : color == UiBarColor::Blue
                              ? Color{55, 166, 214, 255}
                              : Color{72, 164, 82, 255}));
        DrawRectangleRec(filled, fallback);
    }
}

void GameUi::drawResourceIcon(Rectangle bounds,
                              UiResourceIcon icon,
                              Color tint) const {
    const Texture2D texture = resourceTexture(icon);
    if (!IsTextureValid(texture)) {
        return;
    }
    DrawTexturePro(
        texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        bounds, {0.0F, 0.0F}, 0.0F, tint);
}

Texture2D GameUi::buildingTexture(BuildingType type) const {
    switch (type) {
    case BuildingType::Core: return coreBuildingIcon_;
    case BuildingType::Wall: return wallBuildingIcon_;
    case BuildingType::Gate: return gateBuildingIcon_;
    case BuildingType::Turret: return buildingIcons_[0];
    case BuildingType::Cannon: return buildingIcons_[1];
    case BuildingType::Catapult: return buildingIcons_[2];
    case BuildingType::SpikeTrap: return buildingIcons_[3];
    case BuildingType::GunTurret: return buildingIcons_[4];
    case BuildingType::CrystalMine: return buildingIcons_[5];
    case BuildingType::LumberMill: return buildingIcons_[6];
    case BuildingType::Quarry: return buildingIcons_[7];
    case BuildingType::SlowTrap: return buildingIcons_[8];
    default: return {};
    }
}

Texture2D GameUi::modularBuildingTexture(
    ModularBuildPiece piece) const {
    switch (piece) {
    case ModularBuildPiece::Foundation:
        return modularBuildingIcons_[0];
    case ModularBuildPiece::FloorPlatform:
        return modularBuildingIcons_[1];
    case ModularBuildPiece::Ramp:
        return modularBuildingIcons_[2];
    case ModularBuildPiece::Wall:
        return {};
    }
    return {};
}

Texture2D GameUi::weaponTexture(PlayerWeapon weapon) const {
    switch (weapon) {
    case PlayerWeapon::Axe: return weaponIcons_[0];
    case PlayerWeapon::Pickaxe: return weaponIcons_[1];
    case PlayerWeapon::Club: return weaponIcons_[2];
    case PlayerWeapon::Rifle: return weaponIcons_[3];
    case PlayerWeapon::Bomb: return weaponIcons_[4];
    case PlayerWeapon::IceWand: return weaponIcons_[5];
    case PlayerWeapon::FireWand: return weaponIcons_[6];
    default: return {};
    }
}

Texture2D GameUi::runUpgradeTexture(RunUpgradeEffect effect) const {
    switch (effect) {
    case RunUpgradeEffect::MoveSpeed:
        return moveUpgradeIcon_;
    case RunUpgradeEffect::MaximumHealth:
        return heartUpgradeIcon_;
    case RunUpgradeEffect::RecoverableArmor:
        return armorUpgradeIcon_;
    case RunUpgradeEffect::ProductionSpeed:
        return xpGainUpgradeIcon_;
    case RunUpgradeEffect::BloodHarvest:
        return lifestealUpgradeIcon_;
    case RunUpgradeEffect::Overkill:
    case RunUpgradeEffect::GunPiercing:
        return pierceUpgradeIcon_;
    case RunUpgradeEffect::Ricochet:
        return ricochetUpgradeIcon_;
    case RunUpgradeEffect::Salvager:
        return magnetUpgradeIcon_;
    case RunUpgradeEffect::TwinBatteries:
        return extraUpgradeIcon_;
    default:
        return {};
    }
}

Texture2D GameUi::levelUpBannerTexture() const {
    return levelUpBanner_;
}

Texture2D GameUi::levelUpRaysTexture() const {
    return levelUpRays_;
}

Texture2D GameUi::coreMenuTexture(
    CoreMenuTexture texture) const {
    const auto index = static_cast<std::size_t>(texture);
    return index < coreMenuTextures_.size()
        ? coreMenuTextures_[index]
        : Texture2D{};
}

void GameUi::drawVitalIcon(Rectangle bounds, bool core) const {
    const Texture2D texture = core ? healthCore_ : healthPlayer_;
    if (!IsTextureValid(texture)) {
        return;
    }
    DrawTexturePro(
        texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        bounds, {0.0F, 0.0F}, 0.0F, WHITE);
}

Texture2D GameUi::resourceTexture(
    UiResourceIcon icon) const {
    if (icon == UiResourceIcon::Stone) {
        return resourceStone_;
    }
    if (icon == UiResourceIcon::Crystal) {
        return resourceCrystal_;
    }
    if (icon == UiResourceIcon::Coin) {
        return resourceCoin_;
    }
    return resourceWood_;
}

bool GameUi::drawButton(Rectangle bounds,
                        std::string_view text) const {
    const bool arrowLeft = text == "<" || text == "-";
    const bool arrowRight = text == ">" || text == "+";
    if (arrowLeft || arrowRight) {
        const bool hovered = CheckCollisionPointRec(
            GetMousePosition(), bounds);
        const bool pressed = hovered &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const Texture2D buttonTexture =
            pressed && IsTextureValid(buttonPressed_)
                ? buttonPressed_
                : hovered && IsTextureValid(buttonHover_)
                    ? buttonHover_ : button_;
        if (IsTextureValid(buttonTexture)) {
            drawPatched(buttonTexture, bounds, buttonInsets_.left,
                        buttonInsets_.top, buttonInsets_.right,
                        buttonInsets_.bottom);
        }
        const Texture2D arrow =
            arrowLeft ? arrowLeft_ : arrowRight_;
        if (IsTextureValid(arrow)) {
            const float size = std::min(
                bounds.height * 0.42F, 28.0F);
            DrawTexturePro(
                arrow,
                {0.0F, 0.0F, static_cast<float>(arrow.width),
                 static_cast<float>(arrow.height)},
                {bounds.x + (bounds.width - size) * 0.5F,
                 bounds.y + (bounds.height - size) * 0.5F,
                 size, size},
                {0.0F, 0.0F}, 0.0F, WHITE);
        }
        return GuiButton(bounds, "") != 0;
    }
    return drawToggleButton(bounds, text, false);
}

bool GameUi::drawToggleButton(Rectangle bounds,
                              std::string_view text,
                              bool active) const {
    const ScopedRayguiFont fontScope;
    const bool hovered = CheckCollisionPointRec(
        GetMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Texture2D texture = active
        ? (pressed && IsTextureValid(buttonActivePressed_)
               ? buttonActivePressed_
               : hovered && IsTextureValid(buttonActiveHover_)
                   ? buttonActiveHover_ : buttonActive_)
        : (pressed && IsTextureValid(buttonPressed_)
               ? buttonPressed_
               : hovered && IsTextureValid(buttonHover_)
                   ? buttonHover_ : button_);
    if (IsTextureValid(texture)) {
        drawPatched(texture, bounds, buttonInsets_.left,
                    buttonInsets_.top, buttonInsets_.right,
                    buttonInsets_.bottom);
    }
    const std::string localized = localizedCopy(text);
    const int previousSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int normalText = GuiGetStyle(BUTTON, TEXT_COLOR_NORMAL);
    const int focusedText = GuiGetStyle(BUTTON, TEXT_COLOR_FOCUSED);
    const int pressedText = GuiGetStyle(BUTTON, TEXT_COLOR_PRESSED);
    const float checkSpace = active ? 34.0F : 0.0F;
    GuiSetStyle(
        DEFAULT, TEXT_SIZE,
        fittedRayguiTextSize(
            localized, bounds, 24.0F + checkSpace, 10.0F));
    if (active) {
        constexpr int DarkText = static_cast<int>(0x352000ffU);
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, DarkText);
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, DarkText);
        GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, DarkText);
    }
    const bool clicked = withNullTerminatedUiText(
        localized, [bounds](const char* value) {
            return GuiButton(bounds, value) != 0;
        });
    GuiSetStyle(DEFAULT, TEXT_SIZE, previousSize);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, normalText);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, focusedText);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, pressedText);
    if (active && IsTextureValid(checkIcon_)) {
        const float size = std::clamp(
            bounds.height * 0.32F, 14.0F, 22.0F);
        DrawTexturePro(
            checkIcon_,
            {0.0F, 0.0F, static_cast<float>(checkIcon_.width),
             static_cast<float>(checkIcon_.height)},
            {bounds.x + bounds.width - size - 12.0F,
             bounds.y + (bounds.height - size) * 0.5F,
             size, size},
            {0.0F, 0.0F}, 0.0F, WHITE);
    }
    return clicked;
}

bool GameUi::drawKeyCap(Rectangle bounds, std::string_view text,
                        bool pressed, unsigned char alpha) const {
    const bool hovered = CheckCollisionPointRec(
        GetMousePosition(), bounds);
    const bool held = pressed ||
        (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    const Texture2D texture =
        held && IsTextureValid(keyCapPressed_)
            ? keyCapPressed_
            : keyCap_;
    const float offset = held ? 2.0F : 0.0F;
    if (IsTextureValid(texture)) {
        drawPatched(
            texture,
            {bounds.x, bounds.y + offset,
             bounds.width, bounds.height},
            buttonInsets_.left, buttonInsets_.top,
            buttonInsets_.right,
            buttonInsets_.bottom,
            hovered ? Color{255, 255, 245, alpha}
                    : Color{255, 255, 255, alpha});
    } else {
        DrawRectangleRounded(
            {bounds.x, bounds.y + offset,
             bounds.width, bounds.height},
            0.16F, 4,
            held ? Color{193, 164, 113, alpha}
                 : Color{220, 195, 145, alpha});
    }
    float fontSize = std::clamp(
        bounds.height * 0.46F, 10.0F, 22.0F);
    Vector2 textSize = measureUiText(text, fontSize);
    while (textSize.x > bounds.width * 0.82F && fontSize > 7.0F) {
        fontSize -= 1.0F;
        textSize = measureUiText(text, fontSize);
    }
    drawUiText(
        text,
        {bounds.x + (bounds.width - textSize.x) * 0.5F,
         bounds.y + offset +
             (bounds.height - textSize.y) * 0.5F - 1.0F},
        fontSize,
        held ? Color{64, 37, 8, alpha}
             : Color{250, 247, 255, alpha});
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

Texture2D GameUi::keyCapTexture(bool pressed) const {
    if (pressed && IsTextureValid(keyCapPressed_)) {
        return keyCapPressed_;
    }
    return keyCap_;
}

float GameUi::drawSliderBar(
    Rectangle bounds, float value,
    float minimum, float maximum) const {
    if (maximum < minimum) {
        std::swap(minimum, maximum);
    }
    value = std::clamp(value, minimum, maximum);
    const float range = maximum - minimum;
    const Rectangle hitBounds{
        bounds.x, bounds.y - 8.0F,
        bounds.width, bounds.height + 16.0F};
    const Vector2 mouse = GetMousePosition();
    const bool hovered =
        CheckCollisionPointRec(mouse, hitBounds);
    if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        bounds.width > 0.0F && range > 0.0F) {
        const float position = std::clamp(
            (mouse.x - bounds.x) / bounds.width, 0.0F, 1.0F);
        value = minimum + range * position;
    }
    const float displayed = range > 0.0F
        ? std::clamp((value - minimum) / range, 0.0F, 1.0F)
        : 0.0F;
    drawProgressBar(bounds, displayed, UiBarColor::Blue);

    const Texture2D knob = hovered &&
        IsTextureValid(sliderKnobHover_)
        ? sliderKnobHover_ : sliderKnob_;
    if (IsTextureValid(knob)) {
        const float knobHeight = std::max(
            bounds.height * 1.45F, 26.0F);
        const float knobWidth = knobHeight *
            static_cast<float>(knob.width) /
            std::max(static_cast<float>(knob.height), 1.0F);
        const float centerX =
            bounds.x + bounds.width * displayed;
        DrawTexturePro(
            knob,
            {0.0F, 0.0F, static_cast<float>(knob.width),
             static_cast<float>(knob.height)},
            {centerX - knobWidth * 0.5F,
             bounds.y + (bounds.height - knobHeight) * 0.5F,
             knobWidth, knobHeight},
            {0.0F, 0.0F}, 0.0F, WHITE);
    }
    return value;
}

void GameUi::drawCursor() const {
    if (!IsTextureValid(cursorHand_)) {
        return;
    }
    const Vector2 mouse = GetMousePosition();
    constexpr float Scale = 1.12F;
    const float width =
        static_cast<float>(cursorHand_.width) * Scale;
    const float height =
        static_cast<float>(cursorHand_.height) * Scale;
    DrawTexturePro(
        cursorHand_,
        {0.0F, 0.0F, static_cast<float>(cursorHand_.width),
         static_cast<float>(cursorHand_.height)},
        {mouse.x - 2.0F, mouse.y - 1.0F, width, height},
        {0.0F, 0.0F}, 0.0F, WHITE);
}

Texture2D GameUi::loadTexture(const char* path) {
    Texture2D texture = LoadTexture(path);
    return IsTextureValid(texture) ? texture : Texture2D{};
}

void GameUi::unloadTexture(Texture2D& texture) {
    if (IsTextureValid(texture)) {
        UnloadTexture(texture);
    }
    texture = {};
}

} // namespace ian
