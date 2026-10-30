#pragma once

#include <raylib.h>
#include "ui/MinimapLandscape.hpp"
#include "ui/MinimapMarkers.hpp"
#include "game/LootChestSystem.hpp"
#include "progression/RunUpgrade.hpp"

#include <string_view>
#include <cstdint>
#include <array>
#include <span>

namespace ian {
enum class BuildingType;
enum class EnemyType : std::uint8_t;
enum class ModularBuildPiece;
enum class PlayerWeapon;
struct SimulationSnapshot;
enum class RunUpgradeEffect;

enum class UiBarColor {
    Red,
    Green,
    Yellow,
    Purple,
    Blue,
};

enum class UiResourceIcon {
    Wood,
    Stone,
    Crystal,
    Coin,
    Silver,
};

enum class ShopActionIcon { Purchase, Reroll };

enum class CoreMenuTexture {
    Header,
    Gear,
    Heart,
    Gem,
    Shield,
    Rock,
    Wood,
    Wrench,
    Upgrade,
    Coin,
    Turret,
    Ballista,
    Cannon,
    Spikes,
    UpgradeGreen,
};

enum class MainMenuTexture {
    FocusTopLeft, FocusBottomRight, FocusBottomLeft, FocusTopRight,
    Silver, Spikes, Wrench, Gear,
    SettingsIcon, ExitIcon,
    PlayIcon, TowerIcon, ScrollIcon, ShopIcon, CrownIcon,
    Count,
};

class GameUi {
  public:
    GameUi() = default;
    ~GameUi();

    GameUi(const GameUi&) = delete;
    GameUi& operator=(const GameUi&) = delete;

    void initialize();
    void shutdown();

    void drawPanel(Rectangle bounds, unsigned char alpha = 245) const;
    void drawInsetPanel(Rectangle bounds,
                        unsigned char alpha = 235) const;
    void drawLabel(Rectangle bounds, std::string_view text,
                   int alignment = 0) const;
    void drawProgressBar(Rectangle bounds, float fraction,
                         UiBarColor color) const;
    void drawResourceIcon(Rectangle bounds,
                          UiResourceIcon icon,
                          Color tint = WHITE) const;
    void drawVitalIcon(Rectangle bounds, bool core) const;
    [[nodiscard]] Texture2D resourceTexture(
        UiResourceIcon icon) const;
    [[nodiscard]] Texture2D buildingTexture(BuildingType type) const;
    [[nodiscard]] Texture2D enemyTexture(EnemyType type) const;
    [[nodiscard]] Texture2D modularBuildingTexture(
        ModularBuildPiece piece) const;
    [[nodiscard]] Texture2D weaponTexture(PlayerWeapon weapon) const;
    [[nodiscard]] Texture2D runUpgradeTexture(
        RunUpgradeEffect effect) const;
    [[nodiscard]] Texture2D lootTexture(LootUpgradeEffect effect) const;
    [[nodiscard]] Texture2D stumpPortraitTexture() const { return stumpPortrait_; }
    [[nodiscard]] Texture2D mainMenuLogoTexture() const;
    [[nodiscard]] Texture2D mainMenuTexture(MainMenuTexture texture) const;
    [[nodiscard]] Texture2D coreMenuTexture(
        CoreMenuTexture texture) const;
    [[nodiscard]] Texture2D metaShopTexture(MetaShopUpgrade upgrade) const;
    [[nodiscard]] Texture2D shopActionTexture(ShopActionIcon action) const;
    bool drawButton(Rectangle bounds, std::string_view text) const;
    bool drawToggleButton(Rectangle bounds, std::string_view text,
                          bool active) const;
    bool drawKeyCap(Rectangle bounds, std::string_view text,
                    bool pressed = false,
                    unsigned char alpha = 255) const;
    [[nodiscard]] bool beginMinimapTarget() const;
    void endMinimapTarget() const;
    void drawMinimapTarget(
        Rectangle bounds, float rotationDegrees = 0.0F) const;
    void updateMinimapTarget(int desiredSize);
    [[nodiscard]] int minimapTargetSize() const;
    void prepareMinimapLandscape(const SimulationSnapshot& snapshot);
    void drawMinimapLandscape(const SimulationSnapshot& snapshot, Rectangle bounds, double radius);
    [[nodiscard]] Texture2D mapIconTexture(MapIcon icon) const;
    void updateMinimapMarkers(const SimulationSnapshot& snapshot);
    [[nodiscard]] std::span<const MinimapMarker> minimapMarkers() const { return minimapMarkers_; }
    [[nodiscard]] bool minimapTargetRefreshDue(
        double nowSeconds, double intervalSeconds,
        bool forceRefresh = false) const;
    void markMinimapTargetRefreshed(double nowSeconds);
    [[nodiscard]] bool beginMinimapFrameTarget() const;
    void endMinimapFrameTarget();
    void drawMinimapFrameTarget(
        Rectangle bounds, float rotationDegrees,
        Color tint = WHITE) const;
    [[nodiscard]] bool minimapFrameTargetReady() const noexcept;
    [[nodiscard]] int minimapFrameTargetSize() const noexcept;
    float drawSliderBar(Rectangle bounds, float value,
                        float minimum, float maximum) const;
    void drawCursor() const;

  private:
    struct SliceInsets {
        int left{};
        int top{};
        int right{};
        int bottom{};
    };

    static Texture2D loadTexture(const char* path);
    static void unloadTexture(Texture2D& texture);

    Texture2D stumpPortrait_{};
    Texture2D panel_{};
    Texture2D insetPanel_{};
    Texture2D button_{};
    Texture2D buttonHover_{};
    Texture2D buttonPressed_{};
    Texture2D buttonActive_{};
    Texture2D buttonActiveHover_{};
    Texture2D buttonActivePressed_{};
    Texture2D keyCap_{};
    Texture2D keyCapPressed_{};
    Texture2D barBack_{};
    Texture2D barBlue_{};
    Texture2D barRed_{};
    Texture2D barGreen_{};
    Texture2D barYellow_{};
    Texture2D barPurple_{};
    Texture2D sliderKnob_{};
    Texture2D sliderKnobHover_{};
    Texture2D cursorHand_{};
    Texture2D checkIcon_{};
    Texture2D arrowLeft_{};
    Texture2D arrowRight_{};
    Texture2D resourceWood_{};
    Texture2D resourceStone_{};
    Texture2D resourceCrystal_{};
    Texture2D resourceCoin_{};
    Texture2D resourceSilver_{};
    Texture2D healthPlayer_{};
    Texture2D healthCore_{};
    Texture2D coreBuildingIcon_{};
    Texture2D wallBuildingIcon_{};
    Texture2D gateBuildingIcon_{};
    std::array<Texture2D, 3> modularBuildingIcons_{};
    std::array<Texture2D, 17> buildingIcons_{};
    std::array<Texture2D, 7> weaponIcons_{};
    std::array<Texture2D, 11> enemyIcons_{};
    Texture2D armorUpgradeIcon_{};
    Texture2D extraUpgradeIcon_{};
    Texture2D heartUpgradeIcon_{};
    Texture2D lifestealUpgradeIcon_{};
    Texture2D magnetUpgradeIcon_{};
    Texture2D moveUpgradeIcon_{};
    Texture2D pierceUpgradeIcon_{};
    Texture2D ricochetUpgradeIcon_{};
    [[nodiscard]] Texture2D runUpgradeSymbolTexture(RunUpgradeEffect effect) const;
    std::array<Texture2D, RunUpgradeEffectCount> tomeIcons_{};
    std::array<Texture2D, MetaShopUpgradeCount> metaShopIcons_{};
    Texture2D shopPurchaseIcon_{};
    std::array<Texture2D, 6> upgradeSymbols_{};
    Texture2D poisonUpgradeIcon_{};
    std::array<Texture2D, LootUpgradeEffectCount> lootIcons_{};
    Texture2D mainMenuLogo_{};
    std::array<Texture2D, static_cast<std::size_t>(MainMenuTexture::Count)>
        mainMenuTextures_{};
    std::array<Texture2D, 15> coreMenuTextures_{};
    SliceInsets panelInsets_{16, 16, 16, 18};
    SliceInsets insetInsets_{9, 9, 9, 9};
    SliceInsets buttonInsets_{12, 10, 12, 14};
    SliceInsets barInsets_{12, 4, 12, 4};
    RenderTexture2D minimapTarget_{};
    MinimapLandscape minimapLandscape_{};
    std::vector<MinimapMarker> minimapMarkers_;
    std::array<Texture2D, static_cast<std::size_t>(MapIcon::Count)> mapIcons_{};
    RenderTexture2D minimapFrameTarget_{};
    Shader minimapCircleShader_{};
    bool minimapTargetCacheValid_{};
    bool minimapFrameTargetCacheValid_{};
    double minimapTargetLastRefreshSeconds_{};
    bool initialized_{};
};

} // namespace ian
