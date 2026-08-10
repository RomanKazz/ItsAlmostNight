#include "combat/PlayerWeaponSystem.hpp"

#include <algorithm>

namespace ian {

PlayerWeaponSystem::PlayerWeaponSystem(RifleBalanceDefinition definition)
    : definition_(definition), ammunition_(definition.magazineSize) {}

void PlayerWeaponSystem::reset() {
    selectedWeapon_ = PlayerWeapon::BareHands;
    rifleLevel_ = 1;
    ammunition_ = definition_.magazineSize;
    fireCooldownRemaining_ = 0.0;
    reloadRemaining_ = 0.0;
}

void PlayerWeaponSystem::setRifleSkillModifiers(
    double damage, double range, double fireRate,
    int magazineBonus) {
    const int previousMagazine = magazineSize();
    rifleDamageMultiplier_ = std::max(0.05, damage);
    rifleRangeMultiplier_ = std::max(0.05, range);
    rifleFireRateMultiplier_ = std::max(0.05, fireRate);
    rifleMagazineBonus_ = std::max(0, magazineBonus);
    ammunition_ = std::clamp(
        ammunition_ + magazineSize() - previousMagazine,
        0, magazineSize());
}

void PlayerWeaponSystem::selectWeapon(PlayerWeapon weapon) { selectedWeapon_ = weapon; }

void PlayerWeaponSystem::tick(double deltaSeconds) {
    fireCooldownRemaining_ = std::max(0.0, fireCooldownRemaining_ - deltaSeconds);
    if (reloadRemaining_ <= 0.0) {
        return;
    }

    reloadRemaining_ = std::max(0.0, reloadRemaining_ - deltaSeconds);
    if (reloadRemaining_ <= 0.0) {
        ammunition_ = magazineSize();
    }
}

std::optional<WeaponFireResult> PlayerWeaponSystem::fireRifle(Vec3 origin, Vec3 direction,
                                                              EnemySystem& enemies,
                                                              double damageMultiplier) {
    if (selectedWeapon_ != PlayerWeapon::Rifle || fireCooldownRemaining_ > 0.0 ||
        reloadRemaining_ > 0.0 || ammunition_ <= 0) {
        return std::nullopt;
    }

    --ammunition_;
    fireCooldownRemaining_ = fireInterval();
    WeaponFireResult result;
    const auto targetId = enemies.raycast(origin, direction, rifleRange());
    if (targetId) {
        const auto target = enemies.enemy(*targetId);
        const auto damage = enemies.damage(
            *targetId, rifleDamage() * std::max(damageMultiplier, 0.0));
        if (target && damage) {
            result.targetId = *targetId;
            result.hitPosition = target->position;
            result.killed = damage->killed;
        }
    } else {
        result.hitPosition = {
            origin.x + direction.x * rifleRange(),
            origin.y + direction.y * rifleRange(),
            origin.z + direction.z * rifleRange(),
        };
    }

    if (ammunition_ == 0) {
        beginReload();
    }
    return result;
}

PlayerWeapon PlayerWeaponSystem::selectedWeapon() const {
    return selectedWeapon_;
}

WeaponUpgradeResult PlayerWeaponSystem::validateUpgrade(int coreLevel, int gold) const {
    if (rifleLevel_ >= 3) {
        return {.error = WeaponUpgradeError::MaxLevel, .level = rifleLevel_};
    }
    if (coreLevel <= rifleLevel_) {
        return {.error = WeaponUpgradeError::CoreLevelRequired, .level = rifleLevel_};
    }
    const int cost = upgradeGoldCost();
    if (gold < cost) {
        return {
            .error = WeaponUpgradeError::InsufficientGold,
            .level = rifleLevel_,
            .goldCost = cost,
        };
    }
    return {
        .error = WeaponUpgradeError::None,
        .level = rifleLevel_,
        .goldCost = cost,
    };
}

WeaponUpgradeResult PlayerWeaponSystem::upgrade(int coreLevel, int gold) {
    const WeaponUpgradeResult validation = validateUpgrade(coreLevel, gold);
    if (!validation.valid()) {
        return validation;
    }
    const int previousMagazineSize = magazineSize();
    ++rifleLevel_;
    ammunition_ += magazineSize() - previousMagazineSize;
    return {
        .error = WeaponUpgradeError::None,
        .level = rifleLevel_,
        .goldCost = validation.goldCost,
    };
}

int PlayerWeaponSystem::rifleLevel() const {
    return rifleLevel_;
}

int PlayerWeaponSystem::upgradeGoldCost() const {
    if (rifleLevel_ >= 1 && rifleLevel_ <= 2) {
        return definition_.upgradeGold[static_cast<std::size_t>(rifleLevel_ - 1)];
    }
    return 0;
}

double PlayerWeaponSystem::rifleRange() const {
    return definition_.range * rifleRangeMultiplier_;
}

double PlayerWeaponSystem::rifleDamage() const {
    return (definition_.damage +
            definition_.damagePerLevel * static_cast<double>(rifleLevel_ - 1)) *
        rifleDamageMultiplier_;
}

double PlayerWeaponSystem::fireInterval() const {
    return definition_.fireInterval /
           ((1.0 + definition_.fireRateBonusPerLevel *
                       static_cast<double>(rifleLevel_ - 1)) *
            rifleFireRateMultiplier_);
}

double PlayerWeaponSystem::reloadDuration() const {
    return definition_.reloadDuration -
           definition_.reloadReductionPerLevel * static_cast<double>(rifleLevel_ - 1);
}

int PlayerWeaponSystem::magazineSize() const {
    return definition_.magazineSize +
           definition_.magazineBonusPerLevel * (rifleLevel_ - 1) +
           rifleMagazineBonus_;
}

int PlayerWeaponSystem::ammunition() const {
    return ammunition_;
}

bool PlayerWeaponSystem::reloading() const {
    return reloadRemaining_ > 0.0;
}

double PlayerWeaponSystem::reloadRemaining() const {
    return reloadRemaining_;
}

void PlayerWeaponSystem::beginReload() {
    reloadRemaining_ = reloadDuration();
}

} // namespace ian
