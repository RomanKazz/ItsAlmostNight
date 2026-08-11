#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

struct EnemyDefinition {
    double health;
    double speed;
    double damage;
    double ramWindup;
    double ramDamageMultiplier;
    double ramCooldown;
};

struct WaveDefinition {
    int budget;
    int basic;
    int fast;
    int heavy;
    int ranged;
    int sapper;
    int flying;
    int splitter;
    bool boss;
    int groupSize;
    double groupInterval;
};

struct BuildingBalanceDefinition {
    int wood;
    int stone;
    int gold;
    double maxHealth;
    int unlockCoreLevel;
    int maxCount;
};

struct ModularBuildingBalanceDefinition {
    int wood;
    int stone;
    int gold;
};

struct RifleBalanceDefinition {
    double range;
    double damage;
    double damagePerLevel;
    double fireInterval;
    double fireRateBonusPerLevel;
    double reloadDuration;
    double reloadReductionPerLevel;
    int magazineSize;
    int magazineBonusPerLevel;
    std::array<int, 2> upgradeGold;
};

struct BombBalanceDefinition {
    int startingBombs;
    double throwSpeed;
    double upwardSpeed;
    double gravity;
    double fuseDuration;
    double groundHeight;
    double explosionRadius;
    double explosionDamage;
    double knockbackStrength;
};

struct ClubBalanceDefinition {
    double damageMultiplier;
    double areaRadius;
    double knockbackStrength;
    double maxDamagePerAttack;
};

struct IceWandBalanceDefinition {
    double cooldown;
    double directDamage;
    double projectileSpeed;
    double projectileRadius;
    double maxLifetime;
    double explosionRadius;
    double freezeDuration;
    double eliteFreezeMultiplier;
    double bossSlowAmount;
    double chargeUpDuration;
    double areaDamageMultiplier;
};

struct FireWandBalanceDefinition {
    double cooldown;
    double directDamage;
    double projectileSpeed;
    double projectileRadius;
    double maxLifetime;
    double explosionRadius;
    double burnDuration;
    double burnDamagePerSecond;
    double burnTickInterval;
    double chargeUpDuration;
    double areaDamageMultiplier;
};

struct WeaponBalanceDefinition {
    RifleBalanceDefinition rifle;
    BombBalanceDefinition bomb;
    ClubBalanceDefinition club;
    IceWandBalanceDefinition iceWand;
    FireWandBalanceDefinition fireWand;
};

struct EconomyBalanceDefinition {
    double goldMineInterval;
    int goldMineAmount;
    int waveRewardPerWave;
    double repairCostFraction;
    double repairCooldownSeconds;
    double sellRefundFraction;
    std::array<double, 2> buildingUpgradeCostMultiplier;
    std::array<int, 2> buildingUpgradeGoldBonus;
    std::array<int, 2> coreUpgradeGold;
};

struct GameplayBalanceDefinition {
    double eyeHeight;
    double walkSpeed;
    double sprintSpeed;
    double playerAcceleration;
    double playerDeceleration;
    double jumpSpeed;
    double gravity;
    double playerMaxHealth;
    double playerRespawnSeconds;
    double playerDeathResourceLossFraction;
    double fallDamageSafeSpeed;
    double fallDamagePerSpeedSquared;
    double ropeFallDamageReduction;
    double pickaxeRange;
    double resourceGatherRange;
    double pickaxeDamage;
    double pickaxeDamageVariation;
    double pickaxeCriticalChance;
    double pickaxeCooldown;
    double axeStoneEfficiency;
    double pickaxeWoodEfficiency;
    double firstBuildPhaseSeconds;
    double betweenWaveSeconds;
    double sunsetSeconds;
    double dawnSeconds;
    double minimumPlacementDistance;
    double maximumPlacementDistance;
};

struct GameBalance {
    static constexpr std::size_t EnemyTypeCount = 9;
    static constexpr std::size_t WaveCount = 6;
    static constexpr std::size_t BuildingTypeCount = 13;

    std::array<EnemyDefinition, EnemyTypeCount> enemies;
    std::array<WaveDefinition, WaveCount> waves;
    std::array<BuildingBalanceDefinition, BuildingTypeCount> buildings;
    std::array<ModularBuildingBalanceDefinition, 3>
        modularBuildings;
    WeaponBalanceDefinition weapons;
    EconomyBalanceDefinition economy;
    GameplayBalanceDefinition gameplay;

    [[nodiscard]] static GameBalance defaults();
};

struct GameBalanceLoadResult {
    GameBalance balance;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const { return errors.empty(); }
};

[[nodiscard]] GameBalanceLoadResult parseGameBalance(std::string_view enemiesJson,
                                                     std::string_view wavesJson,
                                                     std::string_view buildingsJson,
                                                     std::string_view weaponsJson,
                                                     std::string_view economyJson,
                                                     std::string_view gameplayJson);
[[nodiscard]] GameBalanceLoadResult loadGameBalance(std::string_view enemiesPath,
                                                    std::string_view wavesPath,
                                                    std::string_view buildingsPath,
                                                    std::string_view weaponsPath,
                                                    std::string_view economyPath,
                                                    std::string_view gameplayPath);

} // namespace ian
