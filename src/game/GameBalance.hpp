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

struct WeaponBalanceDefinition {
    RifleBalanceDefinition rifle;
    BombBalanceDefinition bomb;
};

struct EconomyBalanceDefinition {
    double goldMineInterval;
    int goldMineAmount;
    int waveRewardPerWave;
    double repairCostFraction;
    double sellRefundFraction;
    std::array<double, 2> buildingUpgradeCostMultiplier;
    std::array<int, 2> buildingUpgradeGoldBonus;
    std::array<int, 2> coreUpgradeGold;
};

struct GameplayBalanceDefinition {
    double eyeHeight;
    double walkSpeed;
    double sprintSpeed;
    double jumpSpeed;
    double gravity;
    double playerMaxHealth;
    double pickaxeRange;
    double pickaxeDamage;
    double pickaxeDamageVariation;
    double pickaxeCriticalChance;
    double pickaxeCooldown;
    double firstBuildPhaseSeconds;
    double betweenWaveSeconds;
    double sunsetSeconds;
    double dawnSeconds;
    double minimumPlacementDistance;
    double maximumPlacementDistance;
};

struct GameBalance {
    static constexpr std::size_t EnemyTypeCount = 7;
    static constexpr std::size_t WaveCount = 6;
    static constexpr std::size_t BuildingTypeCount = 9;

    std::array<EnemyDefinition, EnemyTypeCount> enemies;
    std::array<WaveDefinition, WaveCount> waves;
    std::array<BuildingBalanceDefinition, BuildingTypeCount> buildings;
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
