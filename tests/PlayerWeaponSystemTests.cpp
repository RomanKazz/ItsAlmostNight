#include "TestHarness.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>

void runPlayerWeaponSystemTests() {
    require(
        ian::PlayerWeaponHotbarOrder ==
                std::array{
                    ian::PlayerWeapon::Axe,
                    ian::PlayerWeapon::Pickaxe,
                    ian::PlayerWeapon::Club,
                    ian::PlayerWeapon::IceWand,
                    ian::PlayerWeapon::FireWand,
                    ian::PlayerWeapon::Rifle,
                    ian::PlayerWeapon::Bomb} &&
            ian::isPlayerTool(ian::PlayerWeapon::Pickaxe) &&
            !ian::isPlayerTool(ian::PlayerWeapon::Rifle),
        "tools and combat weapons share one canonical hotbar");

    std::array<bool, ian::PlayerWeaponCount> categoryUnlocks{};
    categoryUnlocks[static_cast<std::size_t>(
        ian::PlayerWeapon::Pickaxe)] = true;
    categoryUnlocks[static_cast<std::size_t>(
        ian::PlayerWeapon::Club)] = true;
    require(
        ian::playerWeaponVisibleHotbarIndex(
            ian::PlayerWeapon::Club, categoryUnlocks) == 1U,
        "unified hotbar index skips locked equipment");

    ian::PlayerWeaponSystem legacySelection;
    legacySelection.restoreState(
        ian::PlayerWeapon::LegacyBareHands, 1);
    require(
        legacySelection.selectedWeapon() == ian::PlayerWeapon::Axe,
        "old suspended runs migrate removed Hands selection to Axe");
    legacySelection.restoreState(ian::PlayerWeapon::Hammer, 1);
    require(
        legacySelection.selectedWeapon() == ian::PlayerWeapon::Axe,
        "old suspended runs migrate removed Hammer selection to Axe");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 1> Spawn{{{0.0, 0.8, -10.0}}};
    enemies.spawnWave(Spawn);

    ian::PlayerWeaponSystem weapon;
    const double baseFireInterval = weapon.fireInterval();
    const double baseReloadDuration = weapon.reloadDuration();
    const int baseMagazineSize = weapon.magazineSize();
    weapon.selectWeapon(ian::PlayerWeapon::Rifle);
    const ian::Vec3 origin{0.0, 0.8, 0.0};
    const ian::Vec3 direction{0.0, 0.0, -1.0};

    const auto firstShot = weapon.fireRifle(origin, direction, enemies);
    require(firstShot.has_value() && firstShot->targetId.has_value(),
            "rifle hits enemy on camera ray");
    require(!firstShot->killed, "first rifle shot leaves basic enemy alive");
    require(!weapon.fireRifle(origin, direction, enemies),
            "rifle cooldown rejects immediate second shot");

    weapon.tick(baseFireInterval);
    const auto secondShot = weapon.fireRifle(origin, direction, enemies);
    require(secondShot.has_value() && !secondShot->killed,
            "stronger basic enemy survives second rifle shot");
    weapon.tick(baseFireInterval);
    const auto thirdShot =
        weapon.fireRifle(origin, direction, enemies);
    require(thirdShot.has_value() && thirdShot->killed,
            "third rifle shot kills basic enemy");

    ian::EnemySystem emptyEnemies;
    for (int shot = 3; shot < baseMagazineSize; ++shot) {
        weapon.tick(baseFireInterval);
        require(weapon.fireRifle(origin, direction, emptyEnemies).has_value(),
                "rifle fires remaining magazine");
    }
    require(weapon.ammunition() == 0 && weapon.reloading(),
            "empty magazine starts automatic reload");
    weapon.tick(baseReloadDuration);
    require(weapon.ammunition() == baseMagazineSize && !weapon.reloading(),
            "reload restores full magazine");

    ian::PlayerWeaponSystem upgradedWeapon;
    require(upgradedWeapon.validateUpgrade(1, 40).error ==
                ian::WeaponUpgradeError::CoreLevelRequired,
            "rifle cannot exceed core level");
    require(upgradedWeapon.validateUpgrade(2, 39).error ==
                ian::WeaponUpgradeError::InsufficientCrystals,
            "rifle upgrade validates crystals");
    const auto levelTwo = upgradedWeapon.upgrade(2, 40);
    require(levelTwo.valid() && levelTwo.level == 2 && levelTwo.crystalCost == 40,
            "rifle reaches level two");
    require(upgradedWeapon.magazineSize() == 10 && upgradedWeapon.ammunition() == 10,
            "rifle upgrade expands current magazine");
    requireNear(upgradedWeapon.rifleDamage(), 3.5, 1e-12,
                "rifle upgrade increases damage");
    require(upgradedWeapon.fireInterval() < baseFireInterval &&
                upgradedWeapon.reloadDuration() < baseReloadDuration,
            "rifle upgrade accelerates firing and reload");

    ian::EnemySystem upgradedTargets;
    upgradedTargets.spawnWave(Spawn);
    upgradedWeapon.selectWeapon(ian::PlayerWeapon::Rifle);
    const auto upgradedShot = upgradedWeapon.fireRifle(origin, direction, upgradedTargets);
    require(upgradedShot.has_value() && !upgradedShot->killed,
            "stronger enemy survives one upgraded rifle shot");
    require(upgradedWeapon.upgrade(3, 80).valid(), "rifle reaches level three");
    require(upgradedWeapon.validateUpgrade(3, 0).error == ian::WeaponUpgradeError::MaxLevel,
            "rifle cannot exceed level three");

    ian::EnemySystem boostedTargets;
    boostedTargets.spawnWave(Spawn);
    ian::PlayerWeaponSystem boostedWeapon;
    boostedWeapon.selectWeapon(ian::PlayerWeapon::Rifle);
    const auto boostedShot = boostedWeapon.fireRifle(
        origin, direction, boostedTargets, 5.0);
    require(boostedShot && boostedShot->killed,
            "loot damage multiplier applies to rifle hits");

    ian::PlayerWeaponSystem specializedWeapon;
    const double specializedBaseDamage =
        specializedWeapon.rifleDamage();
    const double specializedBaseRange =
        specializedWeapon.rifleRange();
    const double specializedBaseInterval =
        specializedWeapon.fireInterval();
    const int specializedBaseMagazine =
        specializedWeapon.magazineSize();
    specializedWeapon.setRifleSkillModifiers(
        1.70, 1.35, 0.70, 4);
    requireNear(
        specializedWeapon.rifleDamage(),
        specializedBaseDamage * 1.70, 1e-12,
        "data-driven rifle specialization changes damage");
    requireNear(
        specializedWeapon.rifleRange(),
        specializedBaseRange * 1.35, 1e-12,
        "data-driven rifle specialization changes range");
    requireNear(
        specializedWeapon.fireInterval(),
        specializedBaseInterval / 0.70, 1e-12,
        "negative fire-rate specialization slows rifle cadence");
    require(
        specializedWeapon.magazineSize() ==
                specializedBaseMagazine + 4 &&
            specializedWeapon.ammunition() ==
                specializedBaseMagazine + 4,
        "magazine modifiers expand both capacity and current ammunition");
}
