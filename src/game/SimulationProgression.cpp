#include "game/Simulation.hpp"
#include "progression/DefenseAttackModifiers.hpp"
#include "progression/RunUpgradeOfferSelection.hpp"
#include "progression/CoreCardUpgrades.hpp"

#include "buildings/CoreProgression.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {

bool Simulation::playerWeaponUnlocked(PlayerWeapon weapon) const {
    if (weapon == PlayerWeapon::Axe ||
        weapon == PlayerWeapon::Pickaxe) {
        return true;
    }
    if (weapon == PlayerWeapon::LegacyBareHands ||
        weapon == PlayerWeapon::LegacyHammer) {
        return false;
    }
    if (unlimitedResources_) {
        return true;
    }
    const auto* character = playerClassDefinition(playerClass_);
    if (character && character->startingWeapon == weapon) return true;
    const auto unlock = weaponRunUpgrade(weapon);
    return unlock && runUpgradeStacks_[runUpgradeIndex(*unlock)] > 0;
}
std::size_t Simulation::runWeaponArsenalCount() const {
    return static_cast<std::size_t>(std::ranges::count_if(PlayerWeaponHotbarOrder,
        [this](PlayerWeapon weapon) { return playerWeaponUnlocked(weapon); }));
}

namespace {

constexpr std::uint64_t InsightHashSeed = 0x9e3779b97f4a7c15ULL;
constexpr double BattlePotionAttackSpeedMultiplier = 1.35;

std::uint64_t insightEntityEvent(std::uint64_t tag, EntityId id) {
    std::uint64_t value = (static_cast<std::uint64_t>(id.generation) << 32U) |
                          static_cast<std::uint64_t>(id.index);
    value ^= tag + InsightHashSeed + (value << 6U) + (value >> 2U);
    return value == 0 ? tag + 1 : value;
}

std::uint64_t insightIntegerEvent(std::uint64_t tag, std::uint64_t value) {
    value ^= tag + InsightHashSeed + (value << 6U) + (value >> 2U);
    return value == 0 ? tag + 1 : value;
}

} // namespace

int Simulation::playerWeaponLevel(PlayerWeapon weapon) const {
    if (!playerWeaponUnlocked(weapon)) return 0;
    const auto card=weaponRunUpgrade(weapon);
    return card && runWeaponLevelCard(*card)
        ? std::clamp(runUpgradeStacks_[runUpgradeIndex(*card)],1,MaximumPlayerWeaponLevel) : 1;
}

const InsightSystem& Simulation::insightSystem() const { return insight_; }

std::uint64_t Simulation::structuralRevision() const {
    return structuralRevision_;
}

ProgressionRunState Simulation::saveProgressionState() const {
    return {.insight = insight_.saveState(),
            .objectives = objectives_.saveState()};
}

bool Simulation::loadProgressionState(const ProgressionRunState& state) {
    InsightSystem loadedInsight = insight_;
    ObjectiveSystem loadedObjectives = objectives_;
    if (!loadedInsight.loadState(state.insight) ||
        !loadedObjectives.loadState(state.objectives)) return false;
    const int previousLevels = insight_.progress().totalLevelsEarned;
    insight_ = std::move(loadedInsight);
    applyPlayerClassLevelGrowth(insight_.progress().totalLevelsEarned - previousLevels);
    objectives_ = std::move(loadedObjectives);
    std::fill(insightRewardedEnemyIds_.begin(),
              insightRewardedEnemyIds_.end(), EntityId{});
    invalidateSnapshotCache();
    refreshRunUpgradeRuntimeEffects();
    return true;
}

void Simulation::refreshRunUpgradeRuntimeEffects() {
    buildings_.setBuildingLimitBonus(runUpgradeCount(RunUpgradeEffect::BuildingCapacity) +
        runUpgradeCount(RunUpgradeEffect::ExtraBuilding) +
        static_cast<int>(std::floor(shrineBonusPower(shrines_, ShrineKind::Buildings, RunUpgradeEffect::BuildingCapacity) + 1e-9)));
    runNightlyBombBonus_ = runUpgradeCount(RunUpgradeEffect::NightlyBomb);
    const bool lens = lootStacks_[lootUpgradeIndex(LootUpgradeEffect::TargetingLens)] > 0;
    towers_.setPreferMarkedTargets(lens);
    cannons_.setPreferMarkedTargets(lens);
    const bool cardProgression = !sandboxMode_ && !unlimitedResources_;
    buildings_.setCombatCardProgressionEnabled(cardProgression);
    bool defenseLevelsChanged = false;
    if (cardProgression) {
        for (const auto type : RunBuildingTypes) {
            if (type == BuildingType::Wall && !ownsRunBuilding(type)) continue;
            const auto card = buildingRunUpgrade(type);
            const auto level = static_cast<std::uint8_t>(std::clamp(
                runUpgradeStacks_[runUpgradeIndex(*card)], 1, static_cast<int>(MaxStructureLevel)));
            defenseLevelsChanged = buildings_.setCombatBlueprintLevel(type, level,
                type == BuildingType::Wall ? runUpgradePower(*card) : 0.0) || defenseLevelsChanged;
        }
    }
    resources_.setWoodYieldMultiplier(woodYieldMultiplier_ * classLevelBonuses().woodYield);
    resources_.setGatheringYieldMultiplier(
        activeMetaBonuses_.resourceYield *
        classLevelBonuses().gatheringYield);
    resources_.setNodeCapacityMultiplier(
        1.0 + .20 * playerPassivePower(RunUpgradeEffect::ResourceCapacity));
    crystalMines_.setProductionSpeedMultiplier(
        productionSpeedMultiplier_ * runProductionSpeedMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::ProductionSpeed) * classLevelBonuses().productionSpeed);
    crystalMines_.setTypeProductionSpeedMultipliers(
        (1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::LumberMillProduction)) * relicProductionMultiplier(ResourceType::Wood),
        (1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::QuarryProduction)) * relicProductionMultiplier(ResourceType::Stone),
        (1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::CrystalMineProduction)) * relicProductionMultiplier(ResourceType::Crystal));
    lootChests_.setCoinCostMultiplier(
        chestOpeningCostMultiplier_);
    buildings_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::BuildingHealth));
    foundations_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::BuildingHealth));

    // Rebuild defense stats from persisted powers and base meta/class data.
    // The legacy aggregate fields remain in saves for backwards compatibility.
    const auto* defenseClass = playerClassDefinition(playerClass_);
    const double classDamage = defenseClass ? defenseClass->defenseDamageMultiplier : 1.0;
    auto attackModifiers = shrineDefenseAttackModifiers(runUpgradePower_,shrines_);
    attackModifiers[AttackStat::Damage].multiplier *= activeMetaBonuses_.defenseDamage *
        classDamage * classLevelBonuses().defenseDamage * powerups_.defenseDamageMultiplier();
    attackModifiers[AttackStat::AttackSpeed].multiplier *= activeMetaBonuses_.defenseFireRate * powerups_.defenseSpeedMultiplier();
    std::array<AttackModifiers, GameBalance::BuildingTypeCount> buildingModifiers{};
    for (std::size_t index = 0; index < buildingModifiers.size(); ++index) {
        const auto type = static_cast<BuildingType>(index);
        buildingModifiers[index] = buildingStatUpgradeModifiers(type, buildingUpgradePowers_);
        // Pre-retirement shrine blessings belong only to weapons that can use
        // the stat. Preserve suspended runs without offering them again.
        for (const auto [effect, stat] : {std::pair{RunUpgradeEffect::Bounces, AttackStat::Bounces},
                std::pair{RunUpgradeEffect::Pierce, AttackStat::Pierce}}) {
            if (supportsAttackStat(type, stat))
                buildingModifiers[index][stat].flat +=
                    shrineBonusPower(shrines_, ShrineKind::Buildings, effect);
        }
    }
    defenseDamageMultiplier_ = activeMetaBonuses_.defenseDamage * classDamage *
        (1.0 + 0.12 * runUpgradePower(RunUpgradeEffect::DefenseDamage)) * shrineMultiplier(shrines_, RunUpgradeEffect::DefenseDamage);
    defenseFireRateMultiplier_ = activeMetaBonuses_.defenseFireRate *
        (1.0 + 0.10 * runUpgradePower(RunUpgradeEffect::DefenseFireRate)) * shrineMultiplier(shrines_, RunUpgradeEffect::DefenseFireRate);
    attackModifiers[AttackStat::Damage].multiplier *= 1.0 + 0.10 * relics_.safeDeposits * relicStacks(LootUpgradeEffect::CrystalSafe);
    towers_.setSirenStacks(relicStacks(LootUpgradeEffect::Siren));
    cannons_.setSirenStacks(relicStacks(LootUpgradeEffect::Siren));
    towers_.setAttackModifiers(attackModifiers);
    towers_.setBuildingUpgradeModifiers(buildingModifiers);
    towers_.setShrapnelCount(runUpgradeCount(RunUpgradeEffect::GunShrapnel));
    cannons_.setAttackStyle(runCannonAttackStyle(runUpgradeStacks_));
    cannons_.setModifiers(1.0, 1.0, 1.0, 1.0,
        runUpgradeCount(RunUpgradeEffect::ClusterPayload));
    cannons_.setAttackModifiers(attackModifiers);
    cannons_.setBuildingUpgradeModifiers(buildingModifiers);
    traps_.setModifiers(1.0, 1.0, 1.0, 1.0,
        6.0 * runUpgradePower(RunUpgradeEffect::ReactiveTraps));
    traps_.setAttackModifiers(attackModifiers);
    traps_.setBuildingUpgradeModifiers(buildingModifiers);
    const auto* character = playerClassDefinition(playerClass_);
    const auto starting = character ? character->startingBuilding : BuildingType::GunTurret;
    const double startingDamage = character ? character->startingBuildingDamageMultiplier : 1.0;
    const double startingRate = character ? character->startingBuildingFireRateMultiplier : 1.0;
    towers_.setTypeBonuses(starting, startingDamage, startingRate);
    cannons_.setTypeBonuses(starting, startingDamage, startingRate);
    traps_.setTypeBonuses(starting, startingDamage, startingRate);
    playerWeapons_.setRifleLevel(playerWeaponLevel(PlayerWeapon::Rifle));
    playerWeapons_.setRifleModifiers(
        1.0, 1.0,
        playerAttackSpeedMultiplier_ *
            temporaryAttackSpeedMultiplier() * classLevelBonuses().rangedSpeed,
        0);
    const auto* currentClass = playerClassDefinition(playerClass_);
    const double rangedClassDamage = classLevelBonuses().rangedDamage *
        (isSelectablePlayerClass(playerClass_) && currentClass ? currentClass->damageMultiplier : 1.0);
    bombs_.setModifiers(rangedClassDamage, classLevelBonuses().rangedSpeed);
    iceWand_.setModifiers(
        rangedClassDamage * playerWeaponLevelDamage(PlayerWeapon::IceWand,playerWeaponLevel(PlayerWeapon::IceWand)),
        1.0, 1.0, 1.0, 0.0);
    fireWand_.setModifiers(
        rangedClassDamage * playerWeaponLevelDamage(PlayerWeapon::FireWand,playerWeaponLevel(PlayerWeapon::FireWand)),
        1.0, 1.0, rangedClassDamage * playerWeaponLevelDamage(PlayerWeapon::FireWand,playerWeaponLevel(PlayerWeapon::FireWand)), 0.0);
    const double castSpeed = classLevelBonuses().rangedSpeed * playerAttackSpeedMultiplier_ *
        temporaryAttackSpeedMultiplier() *
        (battlePotionBerserkRemaining_ > 0.0
             ? BattlePotionAttackSpeedMultiplier
             : 1.0);
    iceWand_.setCastSpeedMultiplier(castSpeed * playerWeaponLevelSpeed(playerWeaponLevel(PlayerWeapon::IceWand)));
    fireWand_.setCastSpeedMultiplier(castSpeed * playerWeaponLevelSpeed(playerWeaponLevel(PlayerWeapon::FireWand)));
    if (defenseLevelsChanged) syncBuildingRuntimeSystems();
}

void Simulation::prepareRunUpgradeChoices(int selections) {
    if (selections <= 0) return;
    if (shrines_.offer != ShrineKind::None) {
        runUpgradeSelectionsRemaining_ += selections+bonusSelectionsNextReward_;
        bonusSelectionsNextReward_=0;
        invalidateSnapshotCache();
        return;
    }
    runUpgradeSelectionsRemaining_ += selections +
        bonusSelectionsNextReward_;
    bonusSelectionsNextReward_ = 0;
    generateRunUpgradeChoices();
    runUpgradeChoicePending_ = runUpgradeChoiceCount_ > 0;
    invalidateSnapshotCache();
}

bool Simulation::runUpgradeAvailable(RunUpgradeEffect effect) const {
    if(banishedRunUpgrades_[runUpgradeIndex(effect)]) return false;
    if (!sandboxMode_ && !unlimitedResources_ &&
        !activeRunContent_.cards[runUpgradeIndex(effect)] &&
        runUpgradeStacks_[runUpgradeIndex(effect)] == 0) {
        // A purchased character keeps its starting equipment and can level it;
        // it does not unlock that equipment for other classes or future pools.
        const auto weapon = runUpgradeWeapon(effect);
        const auto building = runUpgradeBuilding(effect);
        if (!(weapon && playerWeaponUnlocked(*weapon)) &&
            !(building && ownsRunBuilding(*building))) return false;
    }
    if (effect == RunUpgradeEffect::UpgradeCore) {
        const auto core = buildings_.core();
        if (!core || core->level >= MaxStructureLevel) return false;
    }
    if (retiredRunUpgrade(effect) ||
        (runTomeDefinition(effect) ? runTomeLevel(effect, runUpgradeStacks_) :
            runUpgradeStacks_[runUpgradeIndex(effect)]) >= runUpgradeMaximumLevel(effect) ||
        runUpgradePower(effect) >= runUpgradePowerLimit(effect)) return false;
    if (!sandboxMode_ && !unlimitedResources_ && runTomeDefinition(effect) &&
        runTomeLevel(effect, runUpgradeStacks_) == 0 &&
        runTomeCount(runUpgradeStacks_) >= runTomeSlotLimit(activeMetaBonuses_)) return false;
    if (const auto weapon = runUpgradeWeapon(effect)) {
        if (sandboxMode_ || unlimitedResources_) return false;
        if (!playerWeaponUnlocked(*weapon) && runWeaponArsenalCount() >=
            static_cast<std::size_t>(runWeaponSlotLimit(activeMetaBonuses_))) return false;
        const auto core=buildings_.core();
        return runWeaponLevelCard(effect)
            ? playerWeaponLevel(*weapon) < runWeaponLevelLimit(core?core->level:0U)
            : !playerWeaponUnlocked(*weapon);
    }
    if (const auto building = runUpgradeBuilding(effect)) {
        if (sandboxMode_ || unlimitedResources_) return false;
        if (std::ranges::find(RunEconomyBuildingTypes, *building) != RunEconomyBuildingTypes.end())
            return runUpgradeStacks_[runUpgradeIndex(effect)] > 0;
        return runSlottedBuilding(*building)
            ? ownsRunBuilding(*building) || runBuildingArsenalCount() < static_cast<std::size_t>(runBuildingSlotLimit(activeMetaBonuses_))
            : ownsRunBuilding(*building);
    }
    if (runUpgradeCannonStyle(effect)) {
        return runCannonAttackStyle(runUpgradeStacks_) == CannonAttackStyle::Standard &&
            (sandboxMode_ || unlimitedResources_ || ownsRunBuilding(BuildingType::Cannon));
    }
    if (sandboxMode_ || unlimitedResources_) return true;
    if (const auto producer = runUpgradeProductionBuilding(effect)) return ownsRunBuilding(*producer);
    switch (effect) {
    case RunUpgradeEffect::ProductionSpeed:
        return std::ranges::any_of(RunEconomyBuildingTypes,
            [this](BuildingType type) {
                return runUpgradeStacks_[runUpgradeIndex(*buildingRunUpgrade(type))] > 0;
            });
    case RunUpgradeEffect::NightlyBomb:
        return playerWeaponUnlocked(PlayerWeapon::Bomb);
    case RunUpgradeEffect::ClusterPayload:
        return ownsRunBuilding(BuildingType::Cannon) || ownsRunBuilding(BuildingType::Catapult);
    case RunUpgradeEffect::ReactiveTraps:
        return ownsRunBuilding(BuildingType::SpikeTrap) || ownsRunBuilding(BuildingType::SlowTrap);
    case RunUpgradeEffect::ProjectileCount:
        return std::ranges::any_of(RunBuildingTypes, [this](BuildingType type) { return runCombatBuilding(type) && ownsRunBuilding(type); });
    case RunUpgradeEffect::Pierce:
        return std::ranges::any_of(RunBuildingTypes, [this](BuildingType type) { return runCombatBuilding(type) && ownsRunBuilding(type); });
    case RunUpgradeEffect::GunShrapnel:
        return ownsRunBuilding(BuildingType::GunTurret);
    default:
        return true;
    }
}

void Simulation::generateRunUpgradeChoices() {
    if (shrines_.offer != ShrineKind::None) return;
    runUpgradePaidChoicePrice_ = 0;
    runUpgradeChoiceCount_ = LevelUpRunUpgradeChoices;
    std::vector<RunUpgradeEffect> pool;
    pool.reserve(RunUpgradeEffectCount);
    for (const RunUpgradeDefinition& definition : RunUpgradeDefinitions) {
        // Extra capacity is a slot-free shop purchase, not another level-up card.
        const auto effect = definition.effect;
        if (effect != RunUpgradeEffect::ExtraBuilding && effect != RunUpgradeEffect::UpgradeCore &&
            runUpgradeAvailable(effect))
            pool.push_back(effect);
    }
    std::uint64_t randomState = mixBits64(
        static_cast<std::uint64_t>(terrain_.seed()) ^
        (static_cast<std::uint64_t>(wave_) * 0x9e3779b97f4a7c15ULL) ^
        (static_cast<std::uint64_t>(++runUpgradeOfferGeneration_) * 0xd1b54a32d192ed03ULL));
    randomState = mixBits64(randomState + 0x4918ecdd7f5a1701ULL);
    const bool coreEligible = runUpgradeAvailable(RunUpgradeEffect::UpgradeCore);
    const bool coreSelected = coreEligible && (pool.empty() || randomState % 100ULL < 25ULL);
    if (pool.empty() && !coreSelected) {
        runUpgradeChoiceCount_ = 0;
        runUpgradeChoicePending_ = false;
        runUpgradeSelectionsRemaining_ = 0;
        return;
    }
    auto ownedLevels = runUpgradeStacks_;
    for (const auto effect : pool) {
        if (const auto weapon = runUpgradeWeapon(effect);
            weapon && playerWeaponUnlocked(*weapon))
            ownedLevels[runUpgradeIndex(effect)] = std::max(1, ownedLevels[runUpgradeIndex(effect)]);
        if (const auto building = runUpgradeBuilding(effect);
            building && ownsRunBuilding(*building))
            ownedLevels[runUpgradeIndex(effect)] = std::max(1, ownedLevels[runUpgradeIndex(effect)]);
    }
    const auto chooseWeighted = [&ownedLevels, &randomState](const std::vector<RunUpgradeEffect>& candidates) {
        return chooseRunUpgradeOffer(candidates, ownedLevels, randomState);
    };
    runUpgradeChoiceCount_ = std::min(runUpgradeChoiceCount_, pool.size() + (coreSelected ? 1U : 0U));
    randomState = mixBits64(randomState + 0xced4e824f2941b37ULL);
    const std::size_t coreSlot = coreSelected
        ? static_cast<std::size_t>(randomState % runUpgradeChoiceCount_) : runUpgradeChoiceCount_;
    for (std::size_t index = 0U;
         index < runUpgradeChoiceCount_; ++index) {
        const bool coreHere = index == coreSlot;
        const std::size_t selected = coreHere ? 0 : chooseWeighted(pool);
        const auto effect = coreHere ? RunUpgradeEffect::UpgradeCore : pool[selected];
        runUpgradeChoices_[index] = effect;
        randomState = mixBits64(randomState + 0xa0761d6478bd642fULL);
        const double roll = static_cast<double>(randomState >> 11U) *
            (1.0 / 9007199254740992.0);
        const auto building = runUpgradeBuilding(effect);
        const bool upgradingWeaponBuilding = building && runCombatBuilding(*building) &&
            runUpgradeStacks_[runUpgradeIndex(effect)] > 0;
        runUpgradeRarities_[index] = fixedRunUpgradePower(effect) && effect != RunUpgradeEffect::UpgradeCore && !upgradingWeaponBuilding
            ? fixedRunUpgradeRarity(effect)
            : rollRunUpgradeRarity(activeMetaBonuses_.luck + 0.10 * playerPassivePower(RunUpgradeEffect::Luck), roll);
        runUpgradeRollSeeds_[index] = mixBits64(randomState ^
            (static_cast<std::uint64_t>(effect) << 32U) ^
            (static_cast<std::uint64_t>(index) * 0xe7037ed1a0b428dbULL));
        if (!coreHere) pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(selected));
    }
    if (runUpgradeChoiceCount_ > DefaultRunUpgradeChoices)
        runUpgradePaidChoicePrice_ = PaidRunUpgradeChoicePrice;
}

double Simulation::runUpgradePower(RunUpgradeEffect effect) const {
    return runUpgradePower_[runUpgradeIndex(effect)];
}

int Simulation::runUpgradeCount(RunUpgradeEffect effect) const {
    return runUpgradeDiscreteAmount(effect, runUpgradePower(effect));
}

bool Simulation::applyRunUpgradeEffect(RunUpgradeEffect effect, RunUpgradeRarity rarity,
    int copies, bool debug, std::uint64_t rollSeed) {
    const double previousPlayerMaxHealth = playerPermanentMaxHealth();
    if (effect == RunUpgradeEffect::RiskyInvestment) return false;
    if (effect == RunUpgradeEffect::UpgradeCore &&
        (!buildings_.core() || buildings_.core()->level >= MaxStructureLevel)) return false;
    if (const auto building = runUpgradeBuilding(effect)) {
        if (runSlottedBuilding(*building) &&
            std::ranges::find(RunEconomyBuildingTypes, *building) == RunEconomyBuildingTypes.end()) {
            if (!ownsRunBuilding(*building) && !debug &&
                runBuildingArsenalCount() >= static_cast<std::size_t>(runBuildingSlotLimit(activeMetaBonuses_))) return false;
        } else if (!runSlottedBuilding(*building) && ownsRunBuilding(*building)) return false;
    }
    if (const auto weapon=runUpgradeWeapon(effect)) {
        if (!debug && !playerWeaponUnlocked(*weapon) && runWeaponArsenalCount() >=
            static_cast<std::size_t>(runWeaponSlotLimit(activeMetaBonuses_))) return false;
        if (!runWeaponLevelCard(effect) && playerWeaponUnlocked(*weapon)) return false;
        const auto core=buildings_.core();
        if (runWeaponLevelCard(effect) && !debug &&
            playerWeaponLevel(*weapon)>=runWeaponLevelLimit(core?core->level:0U)) return false;
    }
    // Explicit legacy grants keep their ownership/exclusivity rules, while
    // normal card eligibility rejects these retired building branches.
    if (!debug && runUpgradeCannonStyle(effect) &&
        (runCannonAttackStyle(runUpgradeStacks_) != CannonAttackStyle::Standard ||
         !(sandboxMode_ || unlimitedResources_ || ownsRunBuilding(BuildingType::Cannon)))) return false;
    const bool unique = (runUpgradeBuilding(effect) && !runDefenseCard(effect)) ||
        (runUpgradeWeapon(effect) && !runWeaponLevelCard(effect)) || runUpgradeCannonStyle(effect);
    int& stacks = runUpgradeStacks_[runUpgradeIndex(effect)];
    if (unique && stacks > 0) return false;
    const int amount = effect == RunUpgradeEffect::UpgradeCore
        ? std::min({copies, runUpgradeMaximumLevel(effect) - stacks,
            static_cast<int>(MaxStructureLevel) - buildings_.core()->level})
        : unique ? 1 : (runDefenseCard(effect) || runWeaponLevelCard(effect))
        ? std::min(copies, runUpgradeMaximumLevel(effect) - stacks) : copies;
    if (amount <= 0 || amount > std::numeric_limits<int>::max() - stacks) return false;
    const double power = debug ? runUpgradePowerGain(effect, rarity) * amount
        : runUpgradeAppliedPowerGain(effect, rarity, runUpgradePower(effect));
    if ((!debug && stacks >= runUpgradeMaximumLevel(effect)) || power <= 0.0) return false;
    // Protect numeric storage, not gameplay-level caps. Bulk application remains
    // O(1) regardless of copy count and refreshes runtime effects once.
    if (!std::isfinite(power) || runUpgradePower(effect) + power >
        static_cast<double>(std::numeric_limits<int>::max()) / 16.0) return false;
    if (debug && runUpgradeCannonStyle(effect)) {
        for (const auto& definition : RunUpgradeDefinitions) {
            if (definition.effect != effect && runUpgradeCannonStyle(definition.effect)) {
                runUpgradeStacks_[runUpgradeIndex(definition.effect)] = 0;
                runUpgradePower_[runUpgradeIndex(definition.effect)] = 0.0;
            }
        }
    }
    if (effect == RunUpgradeEffect::Damage || effect == RunUpgradeEffect::AttackSpeed) {
        const auto defenseHalf = effect == RunUpgradeEffect::Damage ? RunUpgradeEffect::DefenseDamage : RunUpgradeEffect::DefenseFireRate;
        double& defensePower = runUpgradePower_[runUpgradeIndex(defenseHalf)];
        if (defensePower + power > static_cast<double>(std::numeric_limits<int>::max()) / 16.0) return false;
        defensePower += power;
    }
    const int oldLevel = stacks;
    stacks += amount;
    runUpgradePower_[runUpgradeIndex(effect)] += power;
    if (const auto building = runUpgradeBuilding(effect);
        building && runCombatBuilding(*building)) {
        for (int level = std::max(2, oldLevel + 1); level <= stacks; ++level) {
            const auto seed = !debug && level == oldLevel + 1
                ? rollSeed
                : mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^
                    (static_cast<std::uint64_t>(*building) << 32U) ^
                    (static_cast<std::uint64_t>(level) * 0x9e3779b97f4a7c15ULL));
            applyBuildingStatUpgrade(buildingUpgradePowers_, *building,
                rollBuildingStatUpgrade(*building, rarity, seed), rarity);
        }
    }
    const auto addCount = [](int value, double increment) {
        return static_cast<int>(std::min(static_cast<double>(std::numeric_limits<int>::max()),
            static_cast<double>(value) + increment));
    };
    switch (effect) {
    case RunUpgradeEffect::UpgradeCore:
        for (int copy = 0; copy < amount; ++copy) {
            const auto seed = !debug && copy == 0 ? rollSeed :
                mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^
                    (static_cast<std::uint64_t>(oldLevel + copy + 1) * 0xd1b54a32d192ed03ULL));
            const auto roll = rollCoreCardUpgrade(rarity, seed);
            for (std::size_t statIndex = 0; statIndex < roll.count; ++statIndex) {
                const auto stat = static_cast<std::size_t>(roll.stats[statIndex]);
                const double oldPower = coreCardPowers_[stat];
                coreCardPowers_[stat] += roll.power;
                if (roll.stats[statIndex] == CoreCardStat::BuildRadius) {
                    const double gained = std::floor(coreCardPowers_[stat]) - std::floor(oldPower);
                    buildings_.setCoreBuildRadius(addCount(buildings_.coreBuildRadius(), gained));
                }
            }
            static_cast<void>(buildings_.upgradeCoreWithCard());
        }
        buildings_.setCoreCardBonuses(coreHealthBaseLevel_, coreCardPowers_[0],
            coreBuildingBaseLevel_, coreCardPowers_[2]);
        break;
    case RunUpgradeEffect::Damage:
        runPlayerDamageMultiplier_ += 0.10 * power;
        break;
    case RunUpgradeEffect::AttackSpeed:
    case RunUpgradeEffect::ExperienceGain:
        break;
    case RunUpgradeEffect::MoveSpeed:
        runPlayerMoveSpeedMultiplier_ += 0.08 * power;
        break;
    case RunUpgradeEffect::MaximumHealth:
        playerBonusMaxHealth_ += 12.0 * power;
        playerHealth_ = std::min(
            playerPermanentMaxHealth() + playerTemporaryHealth_,
            playerHealth_ + playerPermanentMaxHealth() - previousPlayerMaxHealth);
        break;
    case RunUpgradeEffect::RecoverableArmor:
        playerMaxRecoverableArmor_ += 8.0 * power;
        playerRecoverableArmor_ += 8.0 * power;
        break;
    case RunUpgradeEffect::BuildingCapacity:
    case RunUpgradeEffect::ExtraBuilding:
        break;
    case RunUpgradeEffect::BuildingHealth:
        runBuildingMaxHealthMultiplier_ += 0.15 * power;
        playerHealth_ = std::min(playerPermanentMaxHealth() + playerTemporaryHealth_,
            playerHealth_ + playerPermanentMaxHealth() - previousPlayerMaxHealth);
        break;
    case RunUpgradeEffect::BuildRadius:
        buildings_.setCoreBuildRadius(
            addCount(buildings_.coreBuildRadius(), std::floor(3.0 * runUpgradePower(effect)) -
                std::floor(3.0 * (runUpgradePower(effect) - power))));
        break;
    case RunUpgradeEffect::DefenseDamage:
        break;
    case RunUpgradeEffect::DefenseFireRate:
        break;
    case RunUpgradeEffect::ProductionSpeed:
        runProductionSpeedMultiplier_ += 0.12 * power;
        break;
    case RunUpgradeEffect::NightlyBomb:
        runNightlyBombBonus_ = runUpgradeCount(RunUpgradeEffect::NightlyBomb);
        break;
    case RunUpgradeEffect::WiderChoice:
    case RunUpgradeEffect::LockChoice:
        break;
    case RunUpgradeEffect::BloodHarvest:
    case RunUpgradeEffect::Overkill:
    case RunUpgradeEffect::Bounces:
    case RunUpgradeEffect::ProjectileSpeed:
    case RunUpgradeEffect::AttackDuration:
    case RunUpgradeEffect::AttackSize:
    case RunUpgradeEffect::CriticalChance:
    case RunUpgradeEffect::CriticalDamage:
    case RunUpgradeEffect::AttackKnockback:
    case RunUpgradeEffect::PoisonChance:
    case RunUpgradeEffect::Salvager:
    case RunUpgradeEffect::ProjectileCount:
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps:
    case RunUpgradeEffect::Pierce:
    case RunUpgradeEffect::GunShrapnel:
    case RunUpgradeEffect::GatheringSpeed:
    case RunUpgradeEffect::RichResources:
    case RunUpgradeEffect::Luck:
    case RunUpgradeEffect::UnlockGunTurret:
    case RunUpgradeEffect::UnlockTurret:
    case RunUpgradeEffect::UnlockCannon:
    case RunUpgradeEffect::UnlockCatapult:
    case RunUpgradeEffect::UnlockTeslaTower:
    case RunUpgradeEffect::UnlockLaserTower:
    case RunUpgradeEffect::UnlockMortar:
    case RunUpgradeEffect::UnlockRepairStation:
    case RunUpgradeEffect::UnlockWindBlower:
    case RunUpgradeEffect::UnlockDecoyBeacon:
    case RunUpgradeEffect::UnlockMissileLauncher:
    case RunUpgradeEffect::UnlockFlamethrower:
    case RunUpgradeEffect::UnlockSpikeTrap:
    case RunUpgradeEffect::UnlockSlowTrap:
    case RunUpgradeEffect::UnlockWall:
    case RunUpgradeEffect::CannonHeavyShell:
    case RunUpgradeEffect::CannonScattershot:
    case RunUpgradeEffect::CannonClusterShell:
    case RunUpgradeEffect::UnlockLumberMill:
    case RunUpgradeEffect::UnlockQuarry:
    case RunUpgradeEffect::UnlockCrystalMine:
    case RunUpgradeEffect::LumberMillProduction:
    case RunUpgradeEffect::QuarryProduction:
    case RunUpgradeEffect::CrystalMineProduction:
    case RunUpgradeEffect::Regeneration:
    case RunUpgradeEffect::UnlockClub:
    case RunUpgradeEffect::UnlockRifle:
    case RunUpgradeEffect::UnlockIceWand:
    case RunUpgradeEffect::UnlockFireWand:
    case RunUpgradeEffect::UpgradeAxe:
    case RunUpgradeEffect::UpgradePickaxe:
        break;
    case RunUpgradeEffect::UnlockBomb:
        break;
    case RunUpgradeEffect::LongerDay:
        if (state_ == RunState::BuildPhase) {
            phaseTimeRemaining_ += 5.0 * power;
            phaseDuration_ += 5.0 * power;
        }
        break;
    case RunUpgradeEffect::DoubleDown:
        bonusSelectionsNextReward_ = addCount(bonusSelectionsNextReward_, 2.0 * amount);
        break;
    case RunUpgradeEffect::RerollToken:
        runUpgradeRerollTokens_ = addCount(runUpgradeRerollTokens_, amount);
        break;
    case RunUpgradeEffect::RiskyInvestment:
        break; // Reserved save ID; this card is retired.
    }
    refreshRunUpgradeRuntimeEffects();
    if (effect == RunUpgradeEffect::UpgradeCore) {
        const auto core = buildings_.core();
        if (core) events_.push_back({.type = GameEventType::BuildingUpgraded,
            .entityId = core->id, .buildingType = BuildingType::Core,
            .position = buildingWorldPosition(*core)});
    }
    if (const auto weapon=runUpgradeWeapon(effect); weapon && runWeaponLevelCard(effect) && playerWeaponLevel(*weapon)>1) {
        events_.push_back({.type=GameEventType::WeaponUpgraded,.position=playerPosition_,.amount=playerWeaponLevel(*weapon)});
    }
    if (runDefenseCard(effect)) {
        const auto type = *runUpgradeBuilding(effect);
        for (const auto& building : buildings_.buildings()) {
            if (building.type == type) events_.push_back({
                .type = GameEventType::BuildingUpgraded,
                .entityId = building.id,
                .buildingType = type,
                .position = buildingWorldPosition(building),
            });
        }
    }
    return true;
}

bool Simulation::grantDebugRunUpgrade(
    RunUpgradeEffect effect, RunUpgradeRarity rarity) {
    const int rawEffect = static_cast<int>(effect);
    if (rawEffect < 0 ||
        rawEffect >= static_cast<int>(RunUpgradeEffectCount) ||
        (retiredRunUpgrade(effect) && !legacyRunTomeUpgrade(effect) && !legacyBuildingBranchUpgrade(effect)) || static_cast<std::size_t>(rarity) >= RunUpgradeRarityCount) {
        return false;
    }
    invalidateSnapshotCache();
    std::uint64_t rollSeed = 0;
    if (const auto building = runUpgradeBuilding(effect); building && runCombatBuilding(*building)) {
        const auto nextLevel = runUpgradeStacks_[runUpgradeIndex(effect)] + 1;
        rollSeed = mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^
            (static_cast<std::uint64_t>(*building) << 32U) ^
            (static_cast<std::uint64_t>(nextLevel) * 0x9e3779b97f4a7c15ULL));
    }
    return applyRunUpgradeEffect(effect, rarity, 1, false, rollSeed);
}

bool Simulation::grantDebugRunUpgradeBatch(RunUpgradeEffect effect, int count, RunUpgradeRarity rarity) {
    const int index = static_cast<int>(effect);
    if (index < 0 || index >= static_cast<int>(RunUpgradeEffectCount) || count <= 0 ||
        (retiredRunUpgrade(effect) && !legacyRunTomeUpgrade(effect) && !legacyBuildingBranchUpgrade(effect)) || static_cast<std::size_t>(rarity) >= RunUpgradeRarityCount) return false;
    const bool granted = applyRunUpgradeEffect(effect, rarity, count, true);
    if (granted) invalidateSnapshotCache();
    return granted;
}

bool Simulation::grantDebugPowerup(PowerupType type) {
    if (type >= PowerupType::Count || state_ == RunState::MainMenu ||
        state_ == RunState::Victory || state_ == RunState::Defeat) return false;
    powerups_.activate(type);
    if (type == PowerupType::Fury) refreshRunUpgradeRuntimeEffects();
    invalidateSnapshotCache();
    return true;
}

bool Simulation::selectRunUpgrade(std::size_t choiceIndex) {
    if (!runUpgradeChoicePending_ ||
        choiceIndex >= runUpgradeChoiceCount_) {
        return false;
    }
    if (shrines_.offer != ShrineKind::None) return selectShrineBlessing(choiceIndex);
    const int price = runUpgradeChoicePrice(choiceIndex, runUpgradePaidChoicePrice_);
    if (!unlimitedResources_ && coins_ < price) return false;
    invalidateSnapshotCache();
    const RunUpgradeEffect effect = runUpgradeChoices_[choiceIndex];
    if (!runUpgradeAvailable(effect) ||
        !applyRunUpgradeEffect(effect, runUpgradeRarities_[choiceIndex], 1, false,
            runUpgradeRollSeeds_[choiceIndex])) return false;
    if (mirrorStatCard(effect)) {
        static_cast<void>(applyRunUpgradeEffect(effect, runUpgradeRarities_[choiceIndex], 1, false, runUpgradeRollSeeds_[choiceIndex]));
    }
    if (!unlimitedResources_) coins_ -= price;
    --runUpgradeSelectionsRemaining_;
    if (runUpgradeSelectionsRemaining_ > 0) {
        generateRunUpgradeChoices();
    } else {
        runUpgradeChoicePending_ = false;
        runUpgradePaidChoicePrice_ = 0;
    }
    if (!runUpgradeChoicePending_ && runShop_.pending &&
        std::ranges::any_of(runShop_.offers, [this](const auto& offer) {
            return offer.present && !runUpgradeAvailable(offer.effect);
        })) generateRunShopOffers();
    return true;
}

bool Simulation::rerollRunUpgrades() {
    if (shrines_.offer != ShrineKind::None || !runUpgradeChoicePending_ || runUpgradeRerollTokens_ <= 0) {
        return false;
    }
    --runUpgradeRerollTokens_;
    generateRunUpgradeChoices();
    invalidateSnapshotCache();
    return true;
}

bool Simulation::skipRunUpgrade() {
    if(!runUpgradeChoicePending_ || shrines_.offer!=ShrineKind::None || runUpgradeSkipTokens_<=0) return false;
    invalidateSnapshotCache();
    --runUpgradeSkipTokens_;
    --runUpgradeSelectionsRemaining_;
    if(runUpgradeSelectionsRemaining_>0) generateRunUpgradeChoices();
    else {runUpgradeChoicePending_=false;runUpgradeChoiceCount_=0;runUpgradePaidChoicePrice_=0;}
    return true;
}

bool Simulation::banishRunUpgrade(std::size_t index) {
    if(!runUpgradeChoicePending_ || shrines_.offer!=ShrineKind::None ||
        runUpgradeBanishTokens_<=0 || index>=runUpgradeChoiceCount_) return false;
    invalidateSnapshotCache();
    banishedRunUpgrades_[runUpgradeIndex(runUpgradeChoices_[index])]=true;
    --runUpgradeBanishTokens_;
    --runUpgradeSelectionsRemaining_;
    if(runUpgradeSelectionsRemaining_>0) generateRunUpgradeChoices();
    else {runUpgradeChoicePending_=false;runUpgradeChoiceCount_=0;runUpgradePaidChoicePrice_=0;}
    return true;
}

void Simulation::salvageDestroyedBuilding(
    BuildingType type, Vec3 position) {
    if (type == BuildingType::Core) return;
    salvageDestroyedCost(buildings_.configuredCost(type), position);
}

void Simulation::salvageDestroyedModularBuilding(
    const ModularBuildingDamageResult& result, Vec3 position) {
    ModularBuildPiece piece = ModularBuildPiece::Foundation;
    if (result.wall) {
        piece = ModularBuildPiece::Wall;
    } else if (result.ramp) {
        piece = ModularBuildPiece::Ramp;
    } else if (result.platformFrame && result.platformFrame->storey > 0) {
        piece = ModularBuildPiece::FloorPlatform;
    }
    salvageDestroyedCost(
        modularBuildingCosts_[static_cast<std::size_t>(piece)], position);
}

void Simulation::salvageDestroyedCost(
    ResourceCost cost, Vec3 position) {
    const double stacks = runUpgradePower(RunUpgradeEffect::Salvager);
    if (stacks <= 0) return;
    const double fraction = std::min(
        0.90, 0.30 * static_cast<double>(stacks));
    const auto grant = [&](ResourceType resource, int amount) {
        if (amount <= 0) return;
        int before = 0;
        if (resource == ResourceType::Wood) {
            before = wood_;
            addWood(amount);
            amount = wood_ - before;
        } else if (resource == ResourceType::Stone) {
            before = stone_;
            addStone(amount);
            amount = stone_ - before;
        } else if (resource == ResourceType::Crystal) {
            before = crystals_;
            addCrystals(amount);
            amount = crystals_ - before;
        }
        if (amount > 0) {
            events_.push_back({
                .type = GameEventType::ResourceGranted,
                .resourceType = resource,
                .position = position,
                .amount = amount,
            });
        }
    };
    grant(ResourceType::Wood,
          static_cast<int>(std::lround(cost.wood * fraction)));
    grant(ResourceType::Stone,
          static_cast<int>(std::lround(cost.stone * fraction)));
    grant(ResourceType::Crystal,
          static_cast<int>(std::lround(cost.crystals * fraction)));
}

void Simulation::grantConfiguredInsight(
    double amount, InsightSource source, InsightCategory category,
    const InsightGrantContext& context) {
    auto boostedContext = context;
    boostedContext.rewardMultiplier *= runExperienceGainMultiplier(
        playerPassivePower(RunUpgradeEffect::ExperienceGain));
    const InsightGrantResult result = insight_.grantInsight(
        amount, source, category, boostedContext);
    if (!result.accepted) return;
    invalidateSnapshotCache();
    if (result.levelsGranted > 0) {
        applyPlayerClassLevelGrowth(result.levelsGranted);
        refreshRunUpgradeRuntimeEffects();
        prepareRunUpgradeChoices(result.levelsGranted);
    }
    events_.push_back({
        .type = GameEventType::InsightGranted,
        .amount = static_cast<int>(std::lround(result.finalAmount)),
        .intensity = result.finalAmount,
        .insightSource = source,
        .insightAmount = result.finalAmount,
        .insightBefore = result.insightBefore,
        .insightAfter = result.insightAfter,
        .insightRequirement = result.requirement,
        .insightDiminishingMultiplier = result.diminishingMultiplier,
        .levelsGranted = result.levelsGranted,
    });
}

void Simulation::grantBlueprintInsightForType(
    BuildingType type, int blueprintStackOrdinal) {
    if (unlimitedResources_ || blueprintStackOrdinal <= 0) {
        return;
    }
    // Keep existing checkpoint reward keys stable when the building catalog grows.
    constexpr std::uint64_t LegacyBuildingStride = 16U;
    const std::uint64_t typeIndex = static_cast<std::uint64_t>(type);
    const bool legacyType = typeIndex < LegacyBuildingStride;
    const std::uint64_t rewardKey = legacyType
        ? static_cast<std::uint64_t>(blueprintStackOrdinal - 1) * LegacyBuildingStride + typeIndex + 1U
        : static_cast<std::uint64_t>(blueprintStackOrdinal);
    grantConfiguredInsight(
        insight_.config().firstBuildingTypeBonus,
        InsightSource::StructureBuilt,
        InsightCategory::Building,
        {.eventId = insightIntegerEvent(legacyType ? 0x501U : 0x600U + typeIndex, rewardKey),
         .oneTime = true});
}

void Simulation::grantBlueprintInsightForExistingBuildings(
    int blueprintStackOrdinal) {
    std::array<bool, GameBalance::BuildingTypeCount> present{};
    for (const BuildingInstance& building : buildings_.buildings()) {
        present[static_cast<std::size_t>(building.type)] = true;
    }
    for (std::size_t type = 0; type < present.size(); ++type) {
        if (present[type]) {
            grantBlueprintInsightForType(
                static_cast<BuildingType>(type),
                blueprintStackOrdinal);
        }
    }
}

void Simulation::processInsightEvent(const GameEvent& event) {
    const InsightConfig& config = insight_.config();
    switch (event.type) {
    case GameEventType::WaveCompleted: {
        const bool boss = event.critical;
        const bool milestone = !boss && config.milestoneWaveInterval > 0 &&
            event.amount % config.milestoneWaveInterval == 0;
        grantConfiguredInsight(boss ? config.bossWave
                                      : milestone ? config.milestoneWave : config.normalWave,
            InsightSource::WaveCompleted, InsightCategory::Combat,
            {.eventId = insightIntegerEvent(0x200U,
                 static_cast<std::uint64_t>(std::max(0, event.amount))),
             .oneTime = true, .bypassDiminishing = true});
        insight_.beginNewDiminishingCycle();
        break;
    }
    case GameEventType::BuildingPlaced:
        if (!unlimitedResources_ && event.entityId && event.buildingType) {
            const auto type = static_cast<std::size_t>(*event.buildingType);
            grantConfiguredInsight(config.building[type], InsightSource::StructureBuilt,
                InsightCategory::Building,
                {.eventId = insightEntityEvent(0x500U, *event.entityId), .oneTime = true});
            const int blueprintStacks = lootStacks_[
                lootUpgradeIndex(LootUpgradeEffect::Blueprint)];
            for (int stack = 1; stack <= blueprintStacks; ++stack) {
                grantBlueprintInsightForType(
                    *event.buildingType, stack);
            }
        }
        break;
    case GameEventType::ModularBuildingPlaced:
        if (!unlimitedResources_ && event.entityId && event.amount >= 0 && event.amount < 4) {
            grantConfiguredInsight(config.modularBuilding[static_cast<std::size_t>(event.amount)],
                InsightSource::StructureBuilt, InsightCategory::Building,
                {.eventId = insightEntityEvent(0x510U, *event.entityId), .oneTime = true});
        }
        break;
    case GameEventType::BuildingRepaired:
    case GameEventType::ModularBuildingRepaired:
        if (!unlimitedResources_ && event.amount > 0)
            grantConfiguredInsight(config.repairPerHealth * event.amount,
                InsightSource::StructureRepaired, InsightCategory::Repair, {});
        break;
    default:
        break;
    }
}

void Simulation::processInsightEvents(std::size_t firstEvent) {
    const std::size_t lastGameplayEvent = events_.size();
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent event = events_[index];
        processInsightEvent(event);
    }
}

void Simulation::cycleUnlockedTool() {
    std::array<PlayerWeapon, PlayerWeaponCount> tools{};
    std::size_t toolCount = 0U;
    for (const PlayerWeapon weapon : PlayerWeaponHotbarOrder) {
        if (playerWeaponUnlocked(weapon)) {
            tools[toolCount++] = weapon;
        }
    }
    const auto end = tools.begin() + static_cast<std::ptrdiff_t>(toolCount);
    const auto current = std::find(
        tools.begin(), end, playerWeapons_.selectedWeapon());
    const std::size_t next = current == end
        ? 0U
        : (static_cast<std::size_t>(
               std::distance(tools.begin(), current)) + 1U) %
              toolCount;
    playerWeapons_.selectWeapon(tools[next]);
    selectedBuilding_.reset();
    buildingPreview_.reset();
}

} // namespace ian
