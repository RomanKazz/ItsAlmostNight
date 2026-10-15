#include "game/Simulation.hpp"

#include "buildings/CoreProgression.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

bool Simulation::playerWeaponUnlocked(PlayerWeapon weapon) const {
    if (weapon == PlayerWeapon::Axe ||
        weapon == PlayerWeapon::Pickaxe) {
        return true;
    }
    if (weapon == PlayerWeapon::LegacyBareHands) {
        return false;
    }
    if (unlimitedResources_) {
        return true;
    }
    const bool classUnlock =
        (playerClass_ == PlayerClass::Alchemist &&
         weapon == PlayerWeapon::FireWand);
    if (classUnlock) return true;
    const auto unlock = weaponRunUpgrade(weapon);
    return unlock && runUpgradeStacks_[runUpgradeIndex(*unlock)] > 0;
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

std::uint64_t insightStringEvent(std::uint64_t tag, std::string_view text, int cycle) {
    std::uint64_t value = tag ^ static_cast<std::uint64_t>(std::max(0, cycle));
    for (const char character : text)
        value = (value ^ static_cast<unsigned char>(character)) * 1099511628211ULL;
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
const ObjectiveSystem& Simulation::objectiveSystem() const { return objectives_; }

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
    insight_ = std::move(loadedInsight);
    objectives_ = std::move(loadedObjectives);
    std::fill(insightRewardedEnemyIds_.begin(),
              insightRewardedEnemyIds_.end(), EntityId{});
    invalidateSnapshotCache();
    refreshRunUpgradeRuntimeEffects();
    return true;
}

void Simulation::refreshRunUpgradeRuntimeEffects() {
    const bool cardProgression = !sandboxMode_ && !unlimitedResources_;
    buildings_.setCombatCardProgressionEnabled(cardProgression);
    bool defenseLevelsChanged = false;
    if (cardProgression) {
        for (const auto type : RunBuildingTypes) {
            const auto card = buildingRunUpgrade(type);
            const auto level = static_cast<std::uint8_t>(std::clamp(
                runUpgradeStacks_[runUpgradeIndex(*card)], 1, static_cast<int>(MaxBuildingLevel)));
            defenseLevelsChanged = buildings_.setCombatBlueprintLevel(type, level) || defenseLevelsChanged;
        }
    }
    resources_.setGatheringYieldMultiplier(
        activeMetaBonuses_.resourceYield *
        (playerClass_ == PlayerClass::Prospector ? 1.35 : 1.0));
    resources_.setNodeCapacityMultiplier(
        1.0 + .20 * runUpgradePower(RunUpgradeEffect::ResourceCapacity));
    crystalMines_.setProductionSpeedMultiplier(
        productionSpeedMultiplier_ * runProductionSpeedMultiplier_);
    crystalMines_.setTypeProductionSpeedMultipliers(
        1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::LumberMillProduction),
        1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::QuarryProduction),
        1.0 + 0.20 * runUpgradePower(RunUpgradeEffect::CrystalMineProduction));
    lootChests_.setCoinCostMultiplier(
        chestOpeningCostMultiplier_);
    buildings_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_);
    foundations_.setMaxHealthMultiplier(
        buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_);

    const auto* defenseClass = playerClassDefinition(playerClass_);
    const double classDamage = defenseClass ? defenseClass->defenseDamageMultiplier : 1.0;
    defenseDamageMultiplier_ = activeMetaBonuses_.defenseDamage * classDamage *
        (1.0 + 0.12 * runUpgradePower(RunUpgradeEffect::DefenseDamage));
    defenseFireRateMultiplier_ = activeMetaBonuses_.defenseFireRate *
        (1.0 + 0.10 * runUpgradePower(RunUpgradeEffect::DefenseFireRate));
    const double defenseDamage = defenseDamageMultiplier_;
    towers_.setModifiers(
        defenseDamage, 1.0, defenseFireRateMultiplier_, 1.0,
        runUpgradeCount(RunUpgradeEffect::TwinBatteries),
        runUpgradeCount(RunUpgradeEffect::GunPiercing));
    cannons_.setAttackStyle(runCannonAttackStyle(runUpgradeStacks_));
    cannons_.setModifiers(
        defenseDamage, 1.0, defenseFireRateMultiplier_, 1.0,
        runUpgradeCount(RunUpgradeEffect::ClusterPayload));
    traps_.setModifiers(
        defenseDamage, 1.0, defenseFireRateMultiplier_, 1.0,
        6.0 * runUpgradePower(RunUpgradeEffect::ReactiveTraps));
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
            temporaryAttackSpeedMultiplier(),
        0);
    iceWand_.setModifiers(
        playerWeaponLevelDamage(PlayerWeapon::IceWand, playerWeaponLevel(PlayerWeapon::IceWand)),
        1.0, 1.0, 1.0, 0.0);
    fireWand_.setModifiers(
        playerWeaponLevelDamage(PlayerWeapon::FireWand, playerWeaponLevel(PlayerWeapon::FireWand)),
        1.0, 1.0, playerWeaponLevelDamage(PlayerWeapon::FireWand, playerWeaponLevel(PlayerWeapon::FireWand)), 0.0);
    const double castSpeed = playerAttackSpeedMultiplier_ *
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
    runUpgradeSelectionsRemaining_ += selections +
        bonusSelectionsNextReward_;
    bonusSelectionsNextReward_ = 0;
    generateRunUpgradeChoices();
    runUpgradeChoicePending_ = runUpgradeChoiceCount_ > 0;
    invalidateSnapshotCache();
}

bool Simulation::runUpgradeAvailable(RunUpgradeEffect effect) const {
    if (retiredRunUpgrade(effect) ||
        (runTomeDefinition(effect) ? runTomeLevel(effect, runUpgradeStacks_) :
            runUpgradeStacks_[runUpgradeIndex(effect)]) >= runUpgradeMaximumLevel(effect) ||
        runUpgradePower(effect) >= runUpgradePowerLimit(effect)) return false;
    if (!sandboxMode_ && !unlimitedResources_ && runTomeDefinition(effect) &&
        runTomeLevel(effect, runUpgradeStacks_) == 0 &&
        runTomeCount(runUpgradeStacks_) >= runTomeSlotLimit(activeMetaBonuses_)) return false;
    if (const auto weapon = runUpgradeWeapon(effect)) {
        if (sandboxMode_ || unlimitedResources_) return false;
        const auto core=buildings_.core();
        return runWeaponLevelCard(effect)
            ? playerWeaponLevel(*weapon) < runDefenseLevelLimit(core?core->level:0U)
            : !playerWeaponUnlocked(*weapon);
    }
    if (const auto building = runUpgradeBuilding(effect)) {
        if (sandboxMode_ || unlimitedResources_) return false;
        const auto core = buildings_.core();
        if (runCombatBuilding(*building) && runUpgradeStacks_[runUpgradeIndex(effect)] >=
            runDefenseLevelLimit(core ? core->level : 0U)) return false;
        return runCombatBuilding(*building)
            ? ownsRunBuilding(*building) || runBuildingArsenalCount() < RunBuildingSlotCount
            : !ownsRunBuilding(*building);
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
            [this](BuildingType type) { return ownsRunBuilding(type); });
    case RunUpgradeEffect::NightlyBomb:
        return playerWeaponUnlocked(PlayerWeapon::Bomb);
    case RunUpgradeEffect::ClusterPayload:
        return ownsRunBuilding(BuildingType::Cannon) || ownsRunBuilding(BuildingType::Catapult);
    case RunUpgradeEffect::ReactiveTraps:
        return ownsRunBuilding(BuildingType::SpikeTrap) || ownsRunBuilding(BuildingType::SlowTrap);
    case RunUpgradeEffect::TwinBatteries:
        return ownsRunBuilding(BuildingType::GunTurret) || ownsRunBuilding(BuildingType::Turret);
    case RunUpgradeEffect::GunPiercing:
    case RunUpgradeEffect::GunShrapnel:
        return ownsRunBuilding(BuildingType::GunTurret);
    default:
        return true;
    }
}

void Simulation::generateRunUpgradeChoices() {
    const std::size_t extraChoices = std::min<std::size_t>(
        MaximumRunUpgradeChoices - MinimumRunUpgradeChoices,
        static_cast<std::size_t>(std::max(
            0, runUpgradeStacks_[runUpgradeIndex(
                   RunUpgradeEffect::WiderChoice)])));
    runUpgradeChoiceCount_ = MinimumRunUpgradeChoices + extraChoices;

    if (lockedRunUpgrade_ &&
        (retiredRunUpgrade(*lockedRunUpgrade_) ||
         runUpgradeStacks_[runUpgradeIndex(*lockedRunUpgrade_)] >=
             runUpgradeMaximumLevel(*lockedRunUpgrade_) ||
         (*lockedRunUpgrade_ == RunUpgradeEffect::WiderChoice &&
          runUpgradeChoiceCount_ >= MaximumRunUpgradeChoices))) {
        lockedRunUpgrade_.reset();
    }
    std::vector<RunUpgradeEffect> pool;
    pool.reserve(RunUpgradeEffectCount);
    for (const RunUpgradeDefinition& definition : RunUpgradeDefinitions) {
        if (!runUpgradeAvailable(definition.effect)) continue;
        if (definition.effect == RunUpgradeEffect::WiderChoice &&
            runUpgradeChoiceCount_ >= MaximumRunUpgradeChoices) continue;
        if (lockedRunUpgrade_ && definition.effect == *lockedRunUpgrade_) continue;
        pool.push_back(definition.effect);
    }
    if (pool.empty()) {
        runUpgradeChoiceCount_ = 0;
        runUpgradeChoicePending_ = false;
        runUpgradeSelectionsRemaining_ = 0;
        return;
    }
    std::uint64_t randomState = mixBits64(
        static_cast<std::uint64_t>(terrain_.seed()) ^
        (static_cast<std::uint64_t>(wave_) *
         0x9e3779b97f4a7c15ULL) ^
        (static_cast<std::uint64_t>(++runUpgradeOfferGeneration_) *
         0xd1b54a32d192ed03ULL));
    const auto cardWeight = runUpgradeOfferWeight;
    const auto chooseWeighted =
        [&randomState, &cardWeight](
            const std::vector<RunUpgradeEffect>& candidates) {
            int totalWeight = 0;
            for (const RunUpgradeEffect effect : candidates) {
                totalWeight += std::max(1, cardWeight(effect));
            }
            randomState = mixBits64(
                randomState + 0x9e3779b97f4a7c15ULL);
            int roll = static_cast<int>(
                randomState % static_cast<std::uint64_t>(
                                  std::max(1, totalWeight)));
            for (std::size_t index = 0;
                 index < candidates.size(); ++index) {
                roll -= std::max(1, cardWeight(candidates[index]));
                if (roll < 0) return index;
            }
            return candidates.size() - 1U;
        };
    std::size_t firstRandomIndex = 0U;
    if (lockedRunUpgrade_) {
        runUpgradeChoices_[0] = *lockedRunUpgrade_;
        std::erase(pool, runUpgradeChoices_[0]);
        firstRandomIndex = 1U;
    }
    runUpgradeChoiceCount_ = std::min(
        runUpgradeChoiceCount_, firstRandomIndex + pool.size());
    for (std::size_t index = firstRandomIndex;
         index < runUpgradeChoiceCount_; ++index) {
        std::size_t selected = chooseWeighted(pool);
        // Offer combat and economic blueprints early, independently of combat slots.
        if (index < 2U) {
            std::vector<RunUpgradeEffect> blueprints;
            for (const auto effect : pool) {
                if (const auto building = runUpgradeBuilding(effect);
                    building && runCombatBuilding(*building) == (index == 0U)) blueprints.push_back(effect);
            }
            if (!blueprints.empty()) {
                const auto effect = blueprints[chooseWeighted(blueprints)];
                selected = static_cast<std::size_t>(std::find(pool.begin(), pool.end(), effect) - pool.begin());
            }
        }
        runUpgradeChoices_[index] = pool[selected];
        randomState = mixBits64(randomState + 0xa0761d6478bd642fULL);
        const double roll = static_cast<double>(randomState >> 11U) *
            (1.0 / 9007199254740992.0);
        runUpgradeRarities_[index] = fixedRunUpgradePower(pool[selected])
            ? fixedRunUpgradeRarity(pool[selected])
            : rollRunUpgradeRarity(activeMetaBonuses_.luck + 0.10 * runUpgradePower(RunUpgradeEffect::Luck), roll);
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(selected));
    }
}

double Simulation::runUpgradePower(RunUpgradeEffect effect) const {
    return runUpgradePower_[runUpgradeIndex(effect)];
}

int Simulation::runUpgradeCount(RunUpgradeEffect effect) const {
    return runUpgradeDiscreteAmount(effect, runUpgradePower(effect));
}

bool Simulation::applyRunUpgradeEffect(RunUpgradeEffect effect, RunUpgradeRarity rarity) {
    if (effect == RunUpgradeEffect::RiskyInvestment) return false;
    if (const auto building = runUpgradeBuilding(effect)) {
        if (runCombatBuilding(*building)) {
            const auto core = buildings_.core();
            if (runUpgradeStacks_[runUpgradeIndex(effect)] >=
                runDefenseLevelLimit(core ? core->level : 0U)) return false;
            if (!ownsRunBuilding(*building) &&
                runBuildingArsenalCount() >= RunBuildingSlotCount) return false;
        } else if (ownsRunBuilding(*building)) return false;
    }
    if (const auto weapon=runUpgradeWeapon(effect)) {
        if (!runWeaponLevelCard(effect) && playerWeaponUnlocked(*weapon)) return false;
        const auto core=buildings_.core();
        if (runWeaponLevelCard(effect) &&
            playerWeaponLevel(*weapon)>=runDefenseLevelLimit(core?core->level:0U)) return false;
    }
    if (runUpgradeCannonStyle(effect) && !runUpgradeAvailable(effect)) return false;
    int& stacks = runUpgradeStacks_[runUpgradeIndex(effect)];
    const double power = runUpgradeAppliedPowerGain(effect, rarity, runUpgradePower(effect));
    if (stacks >= runUpgradeMaximumLevel(effect) || power <= 0.0) return false;
    if (effect == RunUpgradeEffect::Damage || effect == RunUpgradeEffect::AttackSpeed) {
        const auto defenseHalf = effect == RunUpgradeEffect::Damage ? RunUpgradeEffect::DefenseDamage : RunUpgradeEffect::DefenseFireRate;
        double& defensePower = runUpgradePower_[runUpgradeIndex(defenseHalf)];
        if (defensePower + power > static_cast<double>(std::numeric_limits<int>::max()) / 16.0) return false;
        defensePower += power;
    }
    ++stacks;
    runUpgradePower_[runUpgradeIndex(effect)] += power;
    switch (effect) {
    case RunUpgradeEffect::Damage:
        runPlayerDamageMultiplier_ += 0.10 * power;
        break;
    case RunUpgradeEffect::AttackSpeed:
        playerAttackSpeedMultiplier_ += 0.15 * power;
        break;
    case RunUpgradeEffect::MoveSpeed:
        runPlayerMoveSpeedMultiplier_ += 0.07 * power;
        break;
    case RunUpgradeEffect::MaximumHealth:
        playerBonusMaxHealth_ += 12.0 * power;
        playerHealth_ = std::min(
            playerPermanentMaxHealth() + playerTemporaryHealth_,
            playerHealth_ + 12.0 * power);
        break;
    case RunUpgradeEffect::RecoverableArmor:
        playerMaxRecoverableArmor_ += 8.0 * power;
        playerRecoverableArmor_ += 8.0 * power;
        break;
    case RunUpgradeEffect::BuildingHealth:
        runBuildingMaxHealthMultiplier_ += 0.12 * power;
        break;
    case RunUpgradeEffect::BuildRadius:
        buildings_.setCoreBuildRadius(
            buildings_.coreBuildRadius() + static_cast<int>(std::round(3.0 * power)));
        break;
    case RunUpgradeEffect::DefenseDamage:
        break;
    case RunUpgradeEffect::DefenseFireRate:
        break;
    case RunUpgradeEffect::ProductionSpeed:
        runProductionSpeedMultiplier_ += 0.15 * power;
        break;
    case RunUpgradeEffect::NightlyBomb:
        runNightlyBombBonus_ += static_cast<int>(power);
        break;
    case RunUpgradeEffect::WiderChoice:
        break;
    case RunUpgradeEffect::PoisonChance:
    case RunUpgradeEffect::BloodHarvest:
    case RunUpgradeEffect::Overkill:
    case RunUpgradeEffect::Ricochet:
    case RunUpgradeEffect::Salvager:
    case RunUpgradeEffect::TwinBatteries:
    case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps:
    case RunUpgradeEffect::GunPiercing:
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
    case RunUpgradeEffect::UnlockMissileLauncher:
    case RunUpgradeEffect::UnlockFlamethrower:
    case RunUpgradeEffect::UnlockSpikeTrap:
    case RunUpgradeEffect::UnlockSlowTrap:
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
        bombs_.addBombs(economy_.bombPurchaseAmount);
        break;
    case RunUpgradeEffect::LongerDay:
        if (state_ == RunState::BuildPhase) {
            phaseTimeRemaining_ += 5.0 * power;
            phaseDuration_ += 5.0 * power;
        }
        break;
    case RunUpgradeEffect::DoubleDown:
        bonusSelectionsNextReward_ += 2;
        break;
    case RunUpgradeEffect::LockChoice:
        runUpgradeLockUnlocked_ = true;
        break;
    case RunUpgradeEffect::RerollToken:
        ++runUpgradeRerollTokens_;
        break;
    case RunUpgradeEffect::RiskyInvestment:
        break; // Reserved save ID; this card is retired.
    }
    refreshRunUpgradeRuntimeEffects();
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
        (retiredRunUpgrade(effect) && !legacyRunTomeUpgrade(effect)) ||
        static_cast<std::size_t>(rarity) >= RunUpgradeRarityCount) {
        return false;
    }
    invalidateSnapshotCache();
    return applyRunUpgradeEffect(effect, rarity);
}

bool Simulation::selectRunUpgrade(std::size_t choiceIndex) {
    if (!runUpgradeChoicePending_ ||
        choiceIndex >= runUpgradeChoiceCount_) {
        return false;
    }
    invalidateSnapshotCache();
    const RunUpgradeEffect effect = runUpgradeChoices_[choiceIndex];
    if (lockedRunUpgrade_ == effect) lockedRunUpgrade_.reset();
    if (!runUpgradeAvailable(effect) ||
        !applyRunUpgradeEffect(effect, runUpgradeRarities_[choiceIndex])) return false;
    --runUpgradeSelectionsRemaining_;
    if (runUpgradeSelectionsRemaining_ > 0) {
        generateRunUpgradeChoices();
    } else {
        runUpgradeChoicePending_ = false;
    }
    if (!runUpgradeChoicePending_ && runShop_.pending &&
        std::ranges::any_of(runShop_.offers, [this](const auto& offer) {
            return offer.present && !runUpgradeAvailable(offer.effect);
        })) generateRunShopOffers();
    return true;
}

bool Simulation::rerollRunUpgrades() {
    if (!runUpgradeChoicePending_ || runUpgradeRerollTokens_ <= 0) {
        return false;
    }
    --runUpgradeRerollTokens_;
    generateRunUpgradeChoices();
    invalidateSnapshotCache();
    return true;
}

bool Simulation::lockRunUpgrade(std::size_t choiceIndex) {
    if (!runUpgradeChoicePending_ || !runUpgradeLockUnlocked_ ||
        choiceIndex >= runUpgradeChoiceCount_) {
        return false;
    }
    const RunUpgradeEffect effect = runUpgradeChoices_[choiceIndex];
    if (lockedRunUpgrade_ == effect) {
        lockedRunUpgrade_.reset();
    } else {
        lockedRunUpgrade_ = effect;
    }
    invalidateSnapshotCache();
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
    const InsightGrantResult result = insight_.grantInsight(
        amount, source, category, context);
    if (!result.accepted) return;
    invalidateSnapshotCache();
    if (result.levelsGranted > 0) {
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
    case GameEventType::EnemyKilled:
        if (event.entityId) {
            auto found = enemies_.enemies().end();
            if (!event.enemyType) {
                found = std::ranges::find_if(
                    enemies_.enemies(), [&event](const EnemyInstance& enemy) {
                        return enemy.id == *event.entityId;
                    });
            }
            const EnemyType type = event.enemyType.value_or(
                found != enemies_.enemies().end()
                    ? found->type : EnemyType::Basic);
            const bool boss = type == EnemyType::Boss;
            const bool elite = event.enemyType
                ? event.enemyEliteAffixes != 0U
                : found != enemies_.enemies().end() &&
                    found->eliteAffixes != 0U;
            const double eliteMultiplier = elite ? 1.7 : 1.0;
            grantConfiguredInsight(
                config.enemy[static_cast<std::size_t>(type)] *
                    eliteMultiplier,
                boss ? InsightSource::BossKilled : InsightSource::EnemyKilled,
                InsightCategory::Combat,
                {.eventId = insightEntityEvent(0x300U, *event.entityId),
                 .oneTime = false, .bypassDiminishing = boss});
        }
        break;
    case GameEventType::ResourceHit:
    case GameEventType::ResourceCollected:
        if (!unlimitedResources_ && event.resourceType &&
            isHarvestableResource(*event.resourceType) && event.amount > 0) {
            const auto type = static_cast<std::size_t>(*event.resourceType);
            grantConfiguredInsight(config.resourcePerUnit[type] * event.amount,
                InsightSource::ResourceGathered, InsightCategory::Gathering, {});
            if (event.type == GameEventType::ResourceCollected && event.entityId) {
                grantConfiguredInsight(config.resourceDepleted[type],
                    InsightSource::ResourceDepleted, InsightCategory::Gathering,
                    {.eventId = insightEntityEvent(0x400U, *event.entityId),
                     .oneTime = true});
            }
        }
        break;
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
            if (*event.buildingType == BuildingType::Core) {
                grantConfiguredInsight(config.introCoreObjective, InsightSource::Objective,
                    InsightCategory::Exploration,
                    {.eventId = insightIntegerEvent(0x502U, 1U), .oneTime = true,
                     .bypassDiminishing = true});
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
    case GameEventType::ChestOpened:
        if (!unlimitedResources_ && event.entityId) {
            const auto chest = std::ranges::find_if(lootChests_.chests(),
                [&event](const LootChestInstance& value) { return value.id == *event.entityId; });
            const std::size_t type = chest != lootChests_.chests().end()
                ? static_cast<std::size_t>(chest->type) : 0U;
            grantConfiguredInsight(config.chest[type], InsightSource::ChestOpened,
                InsightCategory::Exploration,
                {.eventId = insightEntityEvent(0x600U, *event.entityId),
                 .oneTime = true, .bypassDiminishing = true});
        }
        break;
    case GameEventType::ObjectiveCompleted:
        if (event.objectiveId && event.intensity > 0.0) {
            grantConfiguredInsight(
                event.intensity, InsightSource::Objective,
                InsightCategory::Exploration,
                {.eventId = insightStringEvent(0x700U, *event.objectiveId, event.amount),
                 .oneTime = true, .bypassDiminishing = true});
        }
        break;
    default:
        break;
    }
}

void Simulation::processObjectiveEvents(std::size_t firstEvent) {
    if (unlimitedResources_) return;
    const std::size_t lastGameplayEvent = events_.size();
    const bool nightNow = state_ == RunState::Sunset || state_ == RunState::Wave;
    const auto core = buildings_.core();
    const Vec3 corePosition = core ? buildingWorldPosition(*core) : Vec3{};
    const auto emitCompletions = [this](std::vector<ObjectiveCompletion> completions) {
        for (const auto& completion : completions) {
            events_.push_back({
                .type = GameEventType::ObjectiveCompleted,
                .position = playerPosition_,
                .amount = completion.cycle,
                .intensity = completion.insightReward,
                .objectiveId = completion.id,
            });
        }
    };
    const auto recordGameplayObjective =
        [this, &emitCompletions](ObjectiveMetric metric, int amount = 1) {
            emitCompletions(objectives_.onGameplayEvent(
                metric, amount, elapsedSeconds_));
        };
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent& event = events_[index];
        if ((event.type == GameEventType::ResourceHit ||
             event.type == GameEventType::ResourceCollected) &&
            event.resourceType &&
            isHarvestableResource(*event.resourceType)) {
            if (*event.resourceType == ResourceType::Crystal) {
                emitCompletions(objectives_.onCrystalsGathered(
                    std::max(0, event.amount), elapsedSeconds_,
                    event.night || nightNow));
                continue;
            }
            const double distance = core
                ? std::hypot(event.position.x - corePosition.x,
                             event.position.z - corePosition.z) : 0.0;
            emitCompletions(objectives_.onResourceEvent({
                .wood = *event.resourceType == ResourceType::Wood,
                .amount = std::max(0, event.amount),
                .depleted = event.type == GameEventType::ResourceCollected,
                .largeDeposit = event.largeDeposit,
                .matchingTool = event.matchingTool,
                .night = event.night || nightNow,
                .hasCore = core.has_value(),
                .distanceFromCore = distance,
                .elapsedSeconds = elapsedSeconds_,
            }));
        } else if (event.type == GameEventType::CrystalProduced) {
            emitCompletions(objectives_.onCrystalsGathered(
                std::max(0, event.amount), elapsedSeconds_, event.night || nightNow));
        } else if (event.type == GameEventType::ResourceGatherMissed) {
            objectives_.onGatheringMiss();
        } else if (event.type == GameEventType::WaveCompleted) {
            recordGameplayObjective(ObjectiveMetric::WavesCompleted);
            static_cast<void>(objectives_.beginNewDay());
        } else {
            switch (event.type) {
            case GameEventType::EnemyKilled:
                recordGameplayObjective(ObjectiveMetric::EnemiesKilled);
                break;
            case GameEventType::BuildingPlaced:
                recordGameplayObjective(ObjectiveMetric::BuildingsPlaced);
                break;
            case GameEventType::ModularBuildingPlaced:
                recordGameplayObjective(ObjectiveMetric::ModularPiecesPlaced);
                break;
            case GameEventType::BuildingUpgraded:
                recordGameplayObjective(ObjectiveMetric::BuildingsUpgraded);
                break;
            case GameEventType::BuildingRepaired:
            case GameEventType::ModularBuildingRepaired:
                recordGameplayObjective(ObjectiveMetric::StructuresRepaired);
                break;
            case GameEventType::CoinCollected:
                recordGameplayObjective(
                    ObjectiveMetric::CoinsCollected,
                    std::max(0, event.amount));
                break;
            case GameEventType::ChestOpened:
                recordGameplayObjective(ObjectiveMetric::ChestsOpened);
                break;
            case GameEventType::LootCollected:
                recordGameplayObjective(ObjectiveMetric::LootCollected);
                break;
            case GameEventType::PlayerDashed:
                recordGameplayObjective(ObjectiveMetric::PlayerDashes);
                break;
            case GameEventType::WeaponFired:
                recordGameplayObjective(ObjectiveMetric::RifleShots);
                break;
            case GameEventType::IceWandHit:
            case GameEventType::FireWandHit:
                recordGameplayObjective(ObjectiveMetric::ElementalHits);
                break;
            case GameEventType::TrapHit:
                recordGameplayObjective(ObjectiveMetric::TrapHits);
                break;
            case GameEventType::CannonFired:
                if (event.buildingType == BuildingType::Cannon) {
                    recordGameplayObjective(ObjectiveMetric::CannonShots);
                }
                break;
            case GameEventType::ConsumableUsed:
                recordGameplayObjective(ObjectiveMetric::BombsThrown);
                break;
            case GameEventType::EarlyWaveBonusGranted:
                recordGameplayObjective(ObjectiveMetric::EarlyWavesStarted);
                break;
            case GameEventType::BuildingFortified:
                recordGameplayObjective(ObjectiveMetric::StructuresFortified);
                break;
            case GameEventType::GateToggled:
                recordGameplayObjective(ObjectiveMetric::GatesToggled);
                break;
            case GameEventType::BuildingSold:
                recordGameplayObjective(ObjectiveMetric::BuildingsSold);
                break;
            case GameEventType::RopeFallSaved:
                recordGameplayObjective(ObjectiveMetric::FallsSaved);
                break;
            default:
                break;
            }
        }
    }
}

void Simulation::processInsightEvents(
    std::size_t firstEvent, bool suppressEnemyRewards) {
    const std::size_t lastGameplayEvent = events_.size();
    for (std::size_t index = firstEvent; index < lastGameplayEvent; ++index) {
        const GameEvent event = events_[index];
        if (event.type == GameEventType::EnemyKilled && event.entityId) {
            if (!markEnemyRewarded(
                    insightRewardedEnemyIds_, *event.entityId)) {
                continue;
            }
            if (suppressEnemyRewards) {
                continue;
            }
        }
        processInsightEvent(event);
    }
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) continue;
        if (!markEnemyRewarded(
                insightRewardedEnemyIds_, enemy.id)) continue;
        if (suppressEnemyRewards) {
            continue;
        }
        processInsightEvent({
            .type = GameEventType::EnemyKilled,
            .entityId = enemy.id,
            .enemyType = enemy.type,
            .enemyEliteAffixes = enemy.eliteAffixes,
            .position = enemy.position,
        });
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
