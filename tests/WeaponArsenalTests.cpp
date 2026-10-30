#include "TestHarness.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <memory>

namespace {
using namespace ian;
std::size_t owned(const Simulation& run) {
    return static_cast<std::size_t>(std::ranges::count_if(PlayerWeaponHotbarOrder,
        [&](auto weapon) { return run.snapshot().unlockedWeapons[static_cast<std::size_t>(weapon)]; }));
}
void chooseLevels(Simulation& run) {
    while (run.snapshot().runUpgradeChoicePending) {
        const auto& offer = run.snapshot();
        std::size_t choice = 0;
        for (std::size_t i = 0; i < offer.runUpgradeChoiceCount; ++i)
            if (!runUpgradeWeapon(offer.runUpgradeChoices[i])) { choice = i; break; }
        require(run.selectRunUpgrade(choice), "equipment fixture resolves rewards");
    }
}
std::unique_ptr<Simulation> fixture(int extra = 0, PlayerClass character = PlayerClass::Vanguard) {
    auto balance = GameBalance::defaults();
    auto& core = balance.buildings[static_cast<std::size_t>(BuildingType::Core)];
    core.wood = core.stone = core.crystals = 0;
    auto map = MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33; world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto run = std::make_unique<Simulation>(balance, map, world);
    MetaRunBonuses bonuses; bonuses.additionalWeaponSlots = extra;
    require(run->configureNextRunBonuses(bonuses), "equipment meta bonus configures the next run");
    run->startRunFromSeed(character, 44871);
    return run;
}
void restore(Simulation& run, const SuspendedRunState& saved) {
    run.returnToMainMenu();
    require(run.loadSuspendedRunState(saved), "equipment checkpoint restores");
}
}

int main() {
    using namespace ian;
    auto run = fixture();
    require(owned(*run) == 2 && run->snapshot().weaponSlotLimit == 4,
        "axe and pickaxe occupy two of four starting equipment slots");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockRifle) &&
            run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockBomb) && owned(*run) == 4,
        "two distinct weapons complete the initial four-slot loadout");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockIceWand) &&
            !run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockFireWand) &&
            !run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockClub) && owned(*run) == 4,
        "all weapon types share the same acquisition limit");
    PlayerCommand god; god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    run->tick(0, god);
    PlayerCommand place; place.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0, 0}, 0};
    run->tick(0, place);
    require(run->snapshot().coreId.has_value(), "equipment fixture places a core");
    PlayerCommand upgrade; upgrade.upgradeBuilding = UpgradeBuildingCommand{*run->snapshot().coreId};
    run->tick(0, upgrade); run->tick(0, god); chooseLevels(*run);
    require(owned(*run) == 4 && run->snapshot().coreLevel == 2 &&
            run->grantDebugRunUpgrade(RunUpgradeEffect::UpgradeAxe),
        "owned tools remain upgradable with all equipment slots occupied");

    auto saved = run->saveSuspendedRunState();
    saved.runUpgradeChoicePending = true; saved.runUpgradeChoiceCount = 1;
    saved.runUpgradeSelectionsRemaining = 1; saved.runUpgradeChoices[0] = RunUpgradeEffect::UnlockIceWand;
    saved.runUpgradeRarities.fill(RunUpgradeRarity::Common);
    restore(*run, saved);
    for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i)
        if (const auto weapon = runUpgradeWeapon(run->snapshot().runUpgradeChoices[i]))
            require(run->snapshot().unlockedWeapons[static_cast<std::size_t>(*weapon)],
                "resume replaces stale new-weapon offers when equipment is full");
    auto stale = saved;
    stale.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockBomb)] = 0;
    stale.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::UnlockBomb)] = 0;
    restore(*run, stale);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockBomb) &&
            !run->selectRunUpgrade(0) && owned(*run) == 4,
        "selection revalidates a fifth-weapon offer after another grant fills its last slot");

    saved.runUpgradeChoicePending = false; saved.runUpgradeChoiceCount = 0;
    saved.runUpgradeSelectionsRemaining = 0; saved.coins = 1000000;
    saved.runShop.pending = true; saved.runShop.wave = saved.wave;
    saved.runShop.generation = 1; saved.runShop.offers.fill({});
    saved.runShop.offers[0] = {.effect = RunUpgradeEffect::UnlockIceWand, .price = 1, .present = true, .locked = true};
    saved.runShop.offers[1] = {.effect = RunUpgradeEffect::UnlockRifle, .price = 1, .present = true};
    restore(*run, saved);
    const int coins = run->snapshot().coins;
    require(!run->snapshot().runShop.pending && !run->snapshot().runShopBuyable[0] &&
        !run->buyRunShopOffer(0) && !run->buyRunShopOffer(1) &&
        !run->rerollRunShop() && run->snapshot().coins == coins,
        "old pending shop saves resume closed and cannot spend coins or bypass equipment limits");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockRifle) && owned(*run) == 4 &&
        run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::Rifle)] == 2,
        "an owned weapon upgrades without consuming another slot");
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockIceWand, 1),
        "explicit bulk debug grant can exceed the gameplay cap");
    saved = run->saveSuspendedRunState(); restore(*run, saved);
    require(owned(*run) == 5 && run->snapshot().weaponSlotLimit == 4 &&
        !run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockFireWand),
        "legacy or debug over-cap equipment survives resume without opening further normal slots");

    for (int extra : {1, 2}) {
        auto expanded = fixture(extra);
        const std::array choices{RunUpgradeEffect::UnlockClub, RunUpgradeEffect::UnlockRifle,
            RunUpgradeEffect::UnlockBomb, RunUpgradeEffect::UnlockIceWand};
        for (int i = 0; i < 2 + extra; ++i)
            require(expanded->grantDebugRunUpgrade(choices[static_cast<std::size_t>(i)]),
                "purchased slots accept extra distinct equipment");
        require(owned(*expanded) == static_cast<std::size_t>(4 + extra) &&
            expanded->snapshot().weaponSlotLimit == 4 + extra &&
            !expanded->grantDebugRunUpgrade(RunUpgradeEffect::UnlockFireWand),
            "one or two meta ranks cap normal equipment at five or six");
        const auto checkpoint = expanded->saveSuspendedRunState(); restore(*expanded, checkpoint);
        require(expanded->snapshot().weaponSlotLimit == 4 + extra && owned(*expanded) == static_cast<std::size_t>(4 + extra),
            "resume retains both slot ranks and every owned weapon");
        expanded->restartRun();
        require(owned(*expanded) == 2 && expanded->snapshot().weaponSlotLimit == 4 + extra,
            "restart clears acquired weapons while retaining purchased capacity");
    }
    auto ranger = fixture(0, PlayerClass::Ranger);
    require(owned(*ranger) == 3 && ranger->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Rifle)],
        "the class starting weapon occupies a slot together with both gathering tools");
    MetaRunBonuses invalid; invalid.additionalWeaponSlots = 3;
    require(!ranger->configureNextRunBonuses(invalid), "invalid equipment-slot bonuses reject safely");
    invalid.additionalWeaponSlots = -1;
    require(!validMetaRunBonuses(invalid), "negative equipment slots reject safely");
}
