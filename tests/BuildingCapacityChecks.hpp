#pragma once
#include "TestHarness.hpp"
#include "progression/CoreCardUpgrades.hpp"
#include "progression/RunUpgrade.hpp"

inline void checkBuildingCapacityProgression() {
    using namespace ian;
    require(runUpgradeIndex(RunUpgradeEffect::UnlockWindBlower) == 67 &&
        runUpgradeIndex(RunUpgradeEffect::BuildingCapacity) == 68 &&
        runUpgradeIndex(RunUpgradeEffect::ExtraBuilding) == 69,
        "capacity save IDs remain reserved");
    require(retiredRunUpgrade(RunUpgradeEffect::BuildingCapacity) &&
        retiredRunUpgrade(RunUpgradeEffect::ExtraBuilding),
        "capacity cards are hidden while placement capacity is disabled");
    for (auto rarity : {RunUpgradeRarity::Common, RunUpgradeRarity::Uncommon,
                        RunUpgradeRarity::Rare, RunUpgradeRarity::Epic,
                        RunUpgradeRarity::Legendary}) {
        for (std::uint64_t seed = 0; seed < 1000; ++seed) {
            const auto roll = rollCoreCardUpgrade(rarity, seed);
            require(roll.count >= 1 && roll.count <= 2,
                "Core card keeps the existing multi-stat rules");
            for (std::size_t index = 0; index < roll.count; ++index)
                require(roll.stats[index] != CoreCardStat::Buildings,
                    "Core card never offers building capacity");
            if (roll.count == 2)
                require(roll.stats[0] != roll.stats[1],
                    "Core card multi-stat roll has distinct stats");
        }
    }
}
