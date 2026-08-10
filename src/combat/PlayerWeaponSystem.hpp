#pragma once

#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace ian {

enum class PlayerWeapon {
    BareHands,
    Axe,
    Pickaxe,
    Club,
    IceWand,
    FireWand,
    Hammer,
    Rifle,
};

inline constexpr std::size_t PlayerWeaponCount = 8;

// Canonical order shared by input, HUD layout and selection animation.
inline constexpr std::array<PlayerWeapon, PlayerWeaponCount>
    PlayerWeaponHotbarOrder{
        PlayerWeapon::BareHands,
        PlayerWeapon::Club,
        PlayerWeapon::Pickaxe,
        PlayerWeapon::Axe,
        PlayerWeapon::IceWand,
        PlayerWeapon::FireWand,
        PlayerWeapon::Hammer,
        PlayerWeapon::Rifle,
    };

inline constexpr std::array<PlayerWeapon, 4>
    PlayerToolHotbarOrder{
        PlayerWeapon::BareHands,
        PlayerWeapon::Pickaxe,
        PlayerWeapon::Axe,
        PlayerWeapon::Hammer,
    };

inline constexpr std::array<PlayerWeapon, 4>
    PlayerCombatHotbarOrder{
        PlayerWeapon::Club,
        PlayerWeapon::IceWand,
        PlayerWeapon::FireWand,
        PlayerWeapon::Rifle,
    };

[[nodiscard]] inline constexpr bool isPlayerTool(
    PlayerWeapon weapon) {
    return weapon == PlayerWeapon::BareHands ||
           weapon == PlayerWeapon::Pickaxe ||
           weapon == PlayerWeapon::Axe ||
           weapon == PlayerWeapon::Hammer;
}

[[nodiscard]] inline constexpr bool isPlayerCombatWeapon(
    PlayerWeapon weapon) {
    return !isPlayerTool(weapon);
}

template <std::size_t Size>
[[nodiscard]] inline std::size_t playerWeaponVisibleHotbarIndex(
    PlayerWeapon selected,
    const std::array<bool, PlayerWeaponCount>& unlocked,
    const std::array<PlayerWeapon, Size>& order) {
    std::size_t visibleIndex = 0;
    for (const PlayerWeapon weapon : order) {
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
    Vec3 hitPosition;
    bool killed{};
};

enum class WeaponUpgradeError {
    None,
    MaxLevel,
    CoreLevelRequired,
    InsufficientGold,
};

struct WeaponUpgradeResult {
    WeaponUpgradeError error{WeaponUpgradeError::None};
    int level{};
    int goldCost{};

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
    void tick(double deltaSeconds);
    std::optional<WeaponFireResult> fireRifle(
        Vec3 origin, Vec3 direction, EnemySystem& enemies,
        double damageMultiplier = 1.0);
    [[nodiscard]] WeaponUpgradeResult validateUpgrade(int coreLevel, int gold) const;
    WeaponUpgradeResult upgrade(int coreLevel, int gold);

    [[nodiscard]] PlayerWeapon selectedWeapon() const;
    [[nodiscard]] int rifleLevel() const;
    [[nodiscard]] int upgradeGoldCost() const;
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

    PlayerWeapon selectedWeapon_{PlayerWeapon::BareHands};
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
