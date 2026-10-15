#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <string_view>

namespace ian {
struct MetaProgression;

enum class MetaShopUpgrade { SilverGain, Rerolls, Luck, DefenseDamage, DefenseFireRate, BuildingHealth, ProductionSpeed, ResourceYield, TomeSlots, WeaponSlots, Count };
inline constexpr std::size_t MetaShopUpgradeCount = static_cast<std::size_t>(MetaShopUpgrade::Count);

struct MetaShopDefinition {
    MetaShopUpgrade upgrade;
    std::string_view id;
    std::string_view title;
    std::string_view description;
    int maximumLevel;
    int baseCost;
    int bonusPerLevel;
    std::string_view requiredUnlock;
};
inline constexpr std::array<MetaShopDefinition, MetaShopUpgradeCount> MetaShopDefinitions{{
    {MetaShopUpgrade::SilverGain, "silver_gain", "SILVER", "Earn more silver from quest rewards.", 20, 2, 5, {}},
    {MetaShopUpgrade::Rerolls, "rerolls", "REROLLS", "Start each run with more level-up card rerolls.", 5, 4, 1, "meta.refresh"},
    {MetaShopUpgrade::Luck, "luck", "LUCK", "Improve your chance of finding rarer level-up cards.", 10, 3, 2, {}},
    {MetaShopUpgrade::DefenseDamage, "defense_damage", "DEFENSE DAMAGE", "Increase damage dealt by your defenses.", 10, 3, 5, {}},
    {MetaShopUpgrade::DefenseFireRate, "defense_fire_rate", "FIRE RATE", "Make your defenses attack faster.", 10, 3, 3, {}},
    {MetaShopUpgrade::BuildingHealth, "building_health", "FORTIFY", "Increase the maximum health of your structures.", 10, 3, 5, {}},
    {MetaShopUpgrade::ProductionSpeed, "production_speed", "PRODUCTION", "Make mines, lumber mills and quarries produce faster.", 10, 3, 5, {}},
    {MetaShopUpgrade::ResourceYield, "resource_yield", "RESOURCE YIELD", "Gather more wood, stone and crystals from resource nodes.", 10, 3, 5, {}},
    {MetaShopUpgrade::TomeSlots, "tome_slots", "TOME SLOTS", "Start with 4 different tomes. Each rank adds one slot, up to 6.", 2, 10, 1, {}},
    {MetaShopUpgrade::WeaponSlots, "weapon_slots", "EQUIPMENT SLOTS", "Start with 4 tools and weapons. Add one slot per rank, up to 6.", 2, 10, 1, {}},
}};

inline constexpr int BaseRunTomeSlots = 4;
inline constexpr int MaximumRunTomeSlots = 6;
inline constexpr int BaseRunWeaponSlots = 4;
inline constexpr int MaximumRunWeaponSlots = 6;

struct MetaRunBonuses {
    double luck{};
    double defenseDamage{1.0};
    double defenseFireRate{1.0};
    double buildingHealth{1.0};
    double productionSpeed{1.0};
    double resourceYield{1.0};
    int rerolls{};
    int additionalTomeSlots{};
    int additionalWeaponSlots{};
    bool operator==(const MetaRunBonuses&) const = default;
};

[[nodiscard]] inline int runWeaponSlotLimit(const MetaRunBonuses& bonuses) {
    return BaseRunWeaponSlots + std::clamp(bonuses.additionalWeaponSlots, 0,
        MaximumRunWeaponSlots - BaseRunWeaponSlots);
}

enum class MetaShopPurchaseResult { Purchased, InvalidUpgrade, Locked, MaximumLevel, InsufficientSilver };
[[nodiscard]] int metaShopLevel(const MetaProgression&, MetaShopUpgrade);
[[nodiscard]] int metaShopCost(const MetaProgression&, MetaShopUpgrade);
[[nodiscard]] bool metaShopAvailable(const MetaProgression&, MetaShopUpgrade);
[[nodiscard]] MetaShopPurchaseResult purchaseMetaShopUpgrade(MetaProgression&, MetaShopUpgrade);
[[nodiscard]] MetaRunBonuses metaRunBonuses(const MetaProgression&);
[[nodiscard]] bool validMetaRunBonuses(const MetaRunBonuses&);
// Returns the actual wallet increase. Fractional percent bonuses carry across rewards.
[[nodiscard]] int creditMetaSilver(MetaProgression&, int baseAmount);
} // namespace ian
