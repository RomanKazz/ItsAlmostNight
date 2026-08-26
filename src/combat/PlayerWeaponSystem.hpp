#pragma once

#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace ian {

enum class PlayerWeapon {
    // Reserved numeric value for suspended runs created before Hands were
    // removed. Selection is normalized to Axe at the system boundary.
    LegacyBareHands,
    Axe,
    Pickaxe,
    Club,
    IceWand,
    FireWand,
    Hammer,
    Rifle,
    Bomb,
};

inline constexpr std::size_t PlayerWeaponCount = 9;

// Canonical order shared by input, HUD layout and selection animation.
inline constexpr std::array<PlayerWeapon, PlayerWeaponCount - 2U>
    PlayerWeaponHotbarOrder{
        PlayerWeapon::Axe,
        PlayerWeapon::Pickaxe,
        PlayerWeapon::Club,
        PlayerWeapon::IceWand,
        PlayerWeapon::FireWand,
        PlayerWeapon::Rifle,
        PlayerWeapon::Bomb,
    };

[[nodiscard]] inline constexpr bool isPlayerTool(
    PlayerWeapon weapon) {
    return weapon == PlayerWeapon::Pickaxe ||
           weapon == PlayerWeapon::Axe ||
           weapon == PlayerWeapon::Hammer;
}

[[nodiscard]] inline constexpr bool isPlayerCombatWeapon(
    PlayerWeapon weapon) {
    return weapon == PlayerWeapon::Club ||
           weapon == PlayerWeapon::IceWand ||
           weapon == PlayerWeapon::FireWand ||
           weapon == PlayerWeapon::Rifle ||
           weapon == PlayerWeapon::Bomb;
}

[[nodiscard]] inline std::size_t playerWeaponVisibleHotbarIndex(
    PlayerWeapon selected,
    const std::array<bool, PlayerWeaponCount>& unlocked) {
    std::size_t visibleIndex = 0;
    for (const PlayerWeapon weapon : PlayerWeaponHotbarOrder) {
        if (!unlocked[static_cast<std::size_t>(weapon)]) {
            continue;
        }
        if (weapon == selected) {
            return visibleIndex;
        }
        ++visibleIndex;
    }
    return 0;
}

struct WeaponFireResult {
    std::optional<EntityId> targetId;
    std::optional<EnemyType> targetType;
    std::uint8_t targetEliteAffixes{};
    Vec3 hitPosition;
    double damage{};
    bool killed{};
};

enum class WeaponUpgradeError {
    None,
    MaxLevel,
    CoreLevelRequired,
    InsufficientCrystals,
};

struct WeaponUpgradeResult {
    WeaponUpgradeError error{WeaponUpgradeError::None};
    int level{};
    int crystalCost{};

    [[nodiscard]] bool valid() const { return error == WeaponUpgradeError::None; }
};

class PlayerWeaponSystem {
  public:
    explicit PlayerWeaponSystem(
        RifleBalanceDefinition definition = GameBalance::defaults().weapons.rifle);

    void reset();
    void setRifleSkillModifiers(double damage, double range,
                                double fireRate, int magazineBonus);
    void selectWeapon(PlayerWeapon weapon);
    void restoreState(PlayerWeapon selectedWeapon, int rifleLevel);
    void tick(double deltaSeconds);
    std::optional<WeaponFireResult> fireRifle(
        Vec3 origin, Vec3 direction, EnemySystem& enemies,
        double damageMultiplier = 1.0);
    [[nodiscard]] WeaponUpgradeResult validateUpgrade(int coreLevel, int crystals) const;
    WeaponUpgradeResult upgrade(int coreLevel, int crystals);

    [[nodiscard]] PlayerWeapon selectedWeapon() const;
    [[nodiscard]] int rifleLevel() const;
    [[nodiscard]] int upgradeCrystalCost() const;
    [[nodiscard]] double rifleRange() const;
    [[nodiscard]] double rifleDamage() const;
    [[nodiscard]] double fireInterval() const;
    [[nodiscard]] double reloadDuration() const;
    [[nodiscard]] int magazineSize() const;
    [[nodiscard]] int ammunition() const;
    [[nodiscard]] bool reloading() const;
    [[nodiscard]] double reloadRemaining() const;

  private:
    void beginReload();

    PlayerWeapon selectedWeapon_{PlayerWeapon::Axe};
    RifleBalanceDefinition definition_;
    int rifleLevel_{1};
    int ammunition_{};
    double fireCooldownRemaining_{};
    double reloadRemaining_{};
    double rifleDamageMultiplier_{1.0};
    double rifleRangeMultiplier_{1.0};
    double rifleFireRateMultiplier_{1.0};
    int rifleMagazineBonus_{};
};

} // namespace ian
