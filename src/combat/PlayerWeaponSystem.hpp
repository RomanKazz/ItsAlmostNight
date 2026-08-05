#pragma once

#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <optional>

namespace ian {

enum class PlayerWeapon {
    BareHands,
    Axe,
    Pickaxe,
    Club,
    Hammer,
    Rifle,
};

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
};

} // namespace ian
