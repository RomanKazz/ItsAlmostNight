#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/QuestSystem.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>

int main() {
    using namespace ian;
    {
        const auto hintsPath=std::filesystem::temp_directory_path()/"ian-placement-hint-progress.json";
        MetaProgression firstRun; firstRun.runsPlayed=1; firstRun.placementControlHintsUsed=1;
        require(saveMetaProgression(hintsPath.string(),firstRun),"first placement hint progress saves");
        MetaProgression resumed;
        require(loadMetaProgression(hintsPath.string(),resumed) && resumed==firstRun,
            "resuming the first run preserves one remaining placement hint");
        resumed.runsPlayed=2;resumed.placementControlHintsUsed=2;
        require(saveMetaProgression(hintsPath.string(),resumed),"completed placement introduction saves");
        MetaProgression veteran;
        require(loadMetaProgression(hintsPath.string(),veteran) && veteran.placementControlHintsUsed==2,
            "placement hints stay consumed across later runs and app launches");
        { std::ofstream file(hintsPath);file<<R"({"version":1,"runsPlayed":4})"; }
        require(loadMetaProgression(hintsPath.string(),veteran) && veteran.placementControlHintsUsed==2,
            "existing players do not receive placement hints again after migration");
        { std::ofstream file(hintsPath);file<<R"({"version":1,"runsPlayed":0})"; }
        require(loadMetaProgression(hintsPath.string(),veteran) && veteran.placementControlHintsUsed==0,
            "unused profiles retain both placement hints");
        std::filesystem::remove(hintsPath);
    }
    MetaProgression profile;
    profile.silver=100;
    require(purchaseMetaShopUpgrade(profile,MetaShopUpgrade::Count)==MetaShopPurchaseResult::InvalidUpgrade,"unknown upgrade cannot spend silver");
    require(purchaseMetaShopUpgrade(profile,MetaShopUpgrade::Rerolls)==MetaShopPurchaseResult::Locked && profile.silver==100,"unlock prerequisite prevents purchasing rerolls");
    const int first=metaShopCost(profile,MetaShopUpgrade::DefenseDamage);
    require(purchaseMetaShopUpgrade(profile,MetaShopUpgrade::DefenseDamage)==MetaShopPurchaseResult::Purchased && profile.silver==100-first,"purchase debits real price");
    require(metaShopCost(profile,MetaShopUpgrade::DefenseDamage)>first,"next level costs more");
    profile.silver=0;
    const auto before=profile;
    require(purchaseMetaShopUpgrade(profile,MetaShopUpgrade::DefenseDamage)==MetaShopPurchaseResult::InsufficientSilver && profile==before,"failed purchase leaves profile intact");
    profile.silver=100000;
    for(const auto& d:MetaShopDefinitions) {
        if(!d.requiredUnlock.empty()) profile.quests.ownedUnlocks.insert(std::string(d.requiredUnlock));
        while(metaShopLevel(profile,d.upgrade)<d.maximumLevel)
            require(purchaseMetaShopUpgrade(profile,d.upgrade)==MetaShopPurchaseResult::Purchased,"all defined levels can be purchased");
        const int balance=profile.silver;
        require(purchaseMetaShopUpgrade(profile,d.upgrade)==MetaShopPurchaseResult::MaximumLevel && profile.silver==balance,"level cap cannot charge again");
    }
    require(metaRunBonuses(profile).additionalWeaponSlots == 2, "two permanent ranks expand equipment capacity to six");
    require(metaRunBonuses(profile).additionalTomeSlots == 2, "two purchased ranks expand tome capacity to six");
    require(metaRunBonuses(profile).additionalBuildingSlots == 2 &&
        runBuildingSlotLimit(metaRunBonuses(profile)) == 6, "two ranks expand shared structure capacity to six");
    require(validMetaRunBonuses(metaRunBonuses(profile)),"catalog caps yield valid runtime bonuses");
    profile.silver=0;
    profile.shopLevels[0]=1;
    int earned=0;
    for(int i=0;i<20;++i) earned+=creditMetaSilver(profile,1);
    require(earned==21 && profile.silver==21 && profile.silverRewardRemainder==0,"five percent bonus carries across tiny rewards without rounding inflation");
    profile.shopLevels[0]=20;
    profile.silver=std::numeric_limits<int>::max()-1;
    require(creditMetaSilver(profile,100)==1 && profile.silver==std::numeric_limits<int>::max(),"reward credit saturates safely");
    const auto path=std::filesystem::temp_directory_path()/"ian_meta_shop_tests.json";
    std::error_code error;
    std::filesystem::remove(path,error);
    profile.silver=37;profile.silverRewardRemainder=35;
    require(saveMetaProgression(path.string(),profile),"shop profile saves");
    MetaProgression loaded;
    require(loadMetaProgression(path.string(),loaded) && loaded==profile,"shop levels wallet ownership and fractional rewards survive reload");
    nlohmann::json saved;
    {std::ifstream input(path);input>>saved;}
    auto oldShop = saved;
    oldShop["shop"]["levels"].erase("tome_slots");
    {std::ofstream output(path);output << oldShop;}
    require(loadMetaProgression(path.string(), loaded) && metaRunBonuses(loaded).additionalTomeSlots == 0,
        "pre-tome shop profiles default to four slots without disturbing other purchases");
    auto oldStructures = saved;
    oldStructures["shop"]["levels"].erase("building_slots");
    {std::ofstream output(path);output << oldStructures;}
    require(loadMetaProgression(path.string(), loaded) && metaRunBonuses(loaded).additionalBuildingSlots == 0 &&
        metaRunBonuses(loaded).additionalTomeSlots == 2,
        "old profiles default structure capacity to four without losing existing purchases");
    auto oldEquipment = saved;
    oldEquipment["shop"]["levels"].erase("weapon_slots");
    {std::ofstream output(path);output << oldEquipment;}
    require(loadMetaProgression(path.string(), loaded) && metaRunBonuses(loaded).additionalWeaponSlots == 0 &&
        metaRunBonuses(loaded).additionalTomeSlots == 2,
        "pre-equipment shop profiles keep existing purchases and default equipment capacity to four");
    {std::ofstream output(path);output << saved;}
    require(loadMetaProgression(path.string(), loaded) && loaded == profile, "new slot purchases reload by stable string ID");
    saved["shop"]["levels"]["silver_gain"]=21;
    {std::ofstream output(path);output<<saved;}
    const auto unchanged=loaded;
    require(!loadMetaProgression(path.string(),loaded) && loaded==unchanged,"invalid level rejects whole save without corrupting current profile");
    {std::ofstream output(path);output<<R"({"version":3,"silver":17,"quests":{"version":2,"initialized":false,"runActive":false,"ownedUnlocks":[],"purchasedUnlocks":[],"progress":{}}})";}
    require(loadMetaProgression(path.string(),loaded) && loaded.silver==17 && metaRunBonuses(loaded)==MetaRunBonuses{},"old profiles migrate with neutral shop bonuses");
    std::filesystem::remove(path,error);

    QuestSystem quests({QuestDefinition{"bonus","Bonus","Bonus",QuestCategory::General,
        {{QuestMetric::EnemiesDefeated,QuestScope::Lifetime,1,QuestComparison::AtLeast,{},{}}}, {}, {20,{},0}}});
    MetaProgression rewardProfile;rewardProfile.shopLevels[0]=1;
    quests.initialize(rewardProfile);
    const QuestSignal signal{QuestMetric::EnemiesDefeated,1};
    static_cast<void>(quests.record(rewardProfile,std::span{&signal,1}));
    require(quests.claimReward(rewardProfile,"bonus").silverGranted==21,"quest claim uses actual shop silver bonus");
    require(!quests.claimReward(rewardProfile,"bonus").claimed && rewardProfile.silver==21,"quest bonus is paid once");

    MetaProgression runProfile;
    runProfile.shopLevels.fill(2);runProfile.quests.ownedUnlocks.insert("meta.refresh");
    const auto bonuses=metaRunBonuses(runProfile);
    auto balance=GameBalance::defaults();
    auto& core=balance.buildings[static_cast<std::size_t>(BuildingType::Core)];
    core.wood=0;core.stone=0;core.crystals=0;
    auto map=MapDefinition::defaults();map.resources.clear();map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    auto sim=std::make_unique<Simulation>(balance,map,world);
    sim->startRunFromSeed(PlayerClass::Engineer,145U);
    const double oldDamage=sim->snapshot().upgradeStats.defenseDamage;
    require(sim->configureNextRunBonuses(bonuses),"valid bonuses configure next run");
    requireNear(sim->snapshot().upgradeStats.defenseDamage,oldDamage,1e-9,"buying does not alter an ongoing run");
    require(sim->snapshot().weaponSlotLimit == 4, "equipment purchases do not alter an ongoing run");
    require(sim->snapshot().tomeSlotLimit == 4, "shop slots do not alter an ongoing run");
    sim->restartRun();
    const auto boosted=sim->snapshot();
    require(boosted.weaponSlotLimit == 6, "equipment slot ranks activate on the next run");
    require(boosted.tomeSlotLimit == 6, "shop slot ranks activate on the next run");
    requireNear(boosted.upgradeStats.defenseDamage,1.25*1.1,1e-9,"defense bonus combines with class");
    requireNear(boosted.upgradeStats.buildingHealth,1.35*1.1,1e-9,"building health bonus combines with class");
    requireNear(boosted.upgradeStats.defenseFireRate,1.06,1e-9,"fire rate bonus is active");
    requireNear(boosted.upgradeStats.productionSpeed,1.1,1e-9,"production bonus is active");
    requireNear(boosted.upgradeStats.resourceYield,1.1,1e-9,"resource bonus is active");
    requireNear(boosted.upgradeStats.luck,.04,1e-9,"permanent luck appears in stats");
    require(boosted.runUpgradeRerollTokens==3,"refresh unlock and shop rerolls combine");
    PlayerCommand placeCore;
    placeCore.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};
    sim->tick(1.0/60.0,placeCore);
    require(sim->snapshot().coreMaxHealth>0,"resume fixture places Core before saving a build-phase checkpoint");
    auto checkpoint=sim->saveSuspendedRunState();
    checkpoint.runUpgradeRerollTokens=1;
    auto resumed=std::make_unique<Simulation>(balance,map,world);
    require(resumed->loadSuspendedRunState(checkpoint),"boosted run checkpoint restores");
    const auto restored=resumed->snapshot();
    require(restored.weaponSlotLimit == 6, "resume retains purchased equipment capacity");
    require(restored.tomeSlotLimit == 6, "resume retains active tome slot purchases");
    requireNear(restored.upgradeStats.resourceYield,1.1,1e-9,"resume retains resource bonus");
    requireNear(restored.upgradeStats.luck,.04,1e-9,"resume retains luck");
    requireNear(restored.upgradeStats.defenseDamage,boosted.upgradeStats.defenseDamage,1e-9,"resume does not apply permanent damage twice");
    require(restored.runUpgradeRerollTokens==1,"resume does not replenish spent rerolls");
    auto invalidStructures = bonuses;
    invalidStructures.additionalBuildingSlots = 3;
    require(!validMetaRunBonuses(invalidStructures), "structure ranks above two are invalid");
    invalidStructures.additionalBuildingSlots = -1;
    require(!validMetaRunBonuses(invalidStructures), "negative structure ranks are invalid");
    auto invalidSlots=bonuses; invalidSlots.additionalTomeSlots=3;
    require(!sim->configureNextRunBonuses(invalidSlots), "invalid slot bonus rejects safely");
    invalidSlots.additionalTomeSlots=-1;
    require(!validMetaRunBonuses(invalidSlots), "negative tome capacity rejects safely");
    auto invalid=bonuses;invalid.luck=std::numeric_limits<double>::quiet_NaN();
    require(!sim->configureNextRunBonuses(invalid),"invalid bonuses reject safely");
    return 0;
}
