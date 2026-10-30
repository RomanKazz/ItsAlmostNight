#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
bool Simulation::activateShrine(WorldLandmarkInstance& landmark) {
    const auto kind=landmarkShrineKind(landmark.type);
    if (kind==ShrineKind::None || landmark.activated || runUpgradeChoicePending_ || runShop_.pending ||
        shrines_.activations>=MaximumShrineActivations ||
        (kind==ShrineKind::Buildings && !runUpgradeAvailable(RunUpgradeEffect::UpgradeCore)) ||
        std::hypot(playerPosition_.x-landmark.position.x,playerPosition_.z-landmark.position.z)>10) return false;
    const auto cost=shrineActivationCost(kind,shrines_.activations);
    if (!unlimitedResources_ && !canAfford(cost,wood_,stone_,crystals_)) {
        events_.push_back({.type=GameEventType::EconomyPurchaseRejected,.entityId=landmark.id,.position=landmark.position});
        return false;
    }
    if (!unlimitedResources_) {wood_-=cost.wood;stone_-=cost.stone;crystals_-=cost.crystals;}
    landmark.activated=true;
    ++shrines_.activations;
    shrines_.offer=kind;
    shrines_.source=landmark.id;
    generateShrineChoices();
    runUpgradeSelectionsRemaining_=1;
    runUpgradeChoicePending_=true;
    ++runUpgradeOfferGeneration_;
    events_.push_back({.type=GameEventType::WorldLandmarkActivated,.entityId=landmark.id,.position=landmark.position});
    invalidateSnapshotCache();
    return true;
}

void Simulation::generateShrineChoices() {
    runUpgradePaidChoicePrice_ = 0;
    const auto kind=shrines_.offer;
    std::array<RunUpgradeEffect,ShrineBlessings.size()> pool{};
    std::size_t count=0;
    for (const auto& blessing:ShrineBlessings)
        if (blessing.kind==kind && availableShrineBlessing(blessing.effect) &&
            (sandboxMode_ || unlimitedResources_ || activeRunContent_.cards[runUpgradeIndex(blessing.icon)] ||
             runTomeLevel(blessing.icon, runUpgradeStacks_) > 0)) pool[count++]=blessing.effect;
    std::uint64_t random=mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^
        (static_cast<std::uint64_t>(shrines_.source->index)<<16) ^
        static_cast<std::uint64_t>(shrines_.activations)*0x9e3779b97f4a7c15ULL);
    const bool coreCard=kind==ShrineKind::Buildings && runUpgradeAvailable(RunUpgradeEffect::UpgradeCore);
    const std::size_t coreSlot=coreCard ? static_cast<std::size_t>(random%3U) : 3U;
    std::size_t drawn=0;
    for (std::size_t i=0;i<3;++i) {
        random=mixBits64(random);
        if (i==coreSlot) {
            runUpgradeChoices_[i]=RunUpgradeEffect::UpgradeCore;
        } else {
            const std::size_t selected=drawn+static_cast<std::size_t>(random%(count-drawn));
            std::swap(pool[drawn],pool[selected]);
            runUpgradeChoices_[i]=pool[drawn++];
        }
        const double roll=static_cast<double>(random>>11U)*(1.0/9007199254740992.0);
        runUpgradeRarities_[i]=rollRunUpgradeRarity(
            activeMetaBonuses_.luck+0.10*playerPassivePower(RunUpgradeEffect::Luck),roll);
        runUpgradeRollSeeds_[i]=mixBits64(random ^
            (static_cast<std::uint64_t>(runUpgradeChoices_[i])<<32U) ^
            (static_cast<std::uint64_t>(i)*0xe7037ed1a0b428dbULL));
    }
    runUpgradeChoiceCount_=3;
}

bool Simulation::selectShrineBlessing(std::size_t choice) {
    if (!runUpgradeChoicePending_ || choice>=runUpgradeChoiceCount_ || !shrines_.source) return false;
    const auto effect=runUpgradeChoices_[choice];
    const auto* blessing=shrineBlessing(shrines_.offer,effect);
    if (!blessing || !availableShrineBlessing(effect)) return false;
    const bool doubled=mirrorStatCard(effect);
    if (effect==RunUpgradeEffect::UpgradeCore &&
        !applyRunUpgradeEffect(effect,runUpgradeRarities_[choice],1,false,
            runUpgradeRollSeeds_[choice])) return false;
    if (effect==RunUpgradeEffect::UpgradeCore && doubled)
        static_cast<void>(applyRunUpgradeEffect(effect,runUpgradeRarities_[choice],1,false,runUpgradeRollSeeds_[choice]));
    ++shrineBonusCounts(shrines_,shrines_.offer)[runUpgradeIndex(effect)];
    if (effect!=RunUpgradeEffect::UpgradeCore)
        shrineBonusPowers(shrines_,shrines_.offer)[runUpgradeIndex(effect)] +=
            runUpgradeRarityMultiplier(runUpgradeRarities_[choice])*(doubled?2:1);
    if (effect==RunUpgradeEffect::MaximumHealth) {
        const double bonus=blessing->increment*runUpgradeRarityMultiplier(runUpgradeRarities_[choice])*(doubled?2:1);
        playerBonusMaxHealth_+=bonus;
        playerHealth_=std::min(playerPermanentMaxHealth()+playerTemporaryHealth_,playerHealth_+bonus);
    } else if (effect==RunUpgradeEffect::RecoverableArmor) {
        const double bonus=blessing->increment*runUpgradeRarityMultiplier(runUpgradeRarities_[choice])*(doubled?2:1);
        playerMaxRecoverableArmor_+=bonus;
        playerRecoverableArmor_+=bonus;
    }
    shrines_.offer=ShrineKind::None;
    shrines_.source.reset();
    --runUpgradeSelectionsRemaining_;
    if (runUpgradeSelectionsRemaining_>0) generateRunUpgradeChoices();
    else runUpgradeChoicePending_=false;
    refreshRunUpgradeRuntimeEffects();
    invalidateSnapshotCache();
    return true;
}
} // namespace ian
