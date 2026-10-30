#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"
#include <algorithm>
#include <limits>
#include <vector>

namespace ian {
namespace {
void startShopVisit(RunShopState& shop,int wave) {
    shop.wave=wave;
    shop.rerolls=0;
    for(auto& offer:shop.offers) if(offer.present && offer.locked)
        offer.price=runShopPrice(offer.effect,offer.rarity,wave);
}
}
void Simulation::prepareRunShop() {
    if (sandboxMode_) return;
    startShopVisit(runShop_,wave_);
    runShop_.pending = true;
    generateRunShopOffers();
    invalidateSnapshotCache();
}

void Simulation::generateRunShopOffers() {
    const auto previous = runShop_.offers;
    std::vector<RunUpgradeEffect> pool;
    pool.reserve(RunUpgradeEffectCount);
    for (auto& offer : runShop_.offers)
        if (offer.locked && (!offer.present || !runUpgradeAvailable(offer.effect))) offer.locked = false;
    for (const auto& definition : RunUpgradeDefinitions) {
        if (!runUpgradeAvailable(definition.effect)) continue;
        const bool reserved = std::ranges::any_of(runShop_.offers, [&](const auto& offer) {
            return offer.present && offer.locked && offer.effect == definition.effect;
        });
        if (!reserved) pool.push_back(definition.effect);
    }
    std::uint64_t random = mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^
        static_cast<std::uint64_t>(runShop_.wave) * 0x9e3779b97f4a7c15ULL ^
        static_cast<std::uint64_t>(++runShop_.generation) * 0xd1b54a32d192ed03ULL ^ 0x53484f50ULL);
    for (auto& offer : runShop_.offers) {
        if (offer.present && offer.locked) continue;
        offer = {};
        if (pool.empty()) continue;
        // Avoid repeating the previous display when eligible alternatives exist.
        const auto isNew = [&](RunUpgradeEffect effect) {
            return !std::ranges::any_of(previous, [&](const auto& old) {
                return old.price > 0 && old.effect == effect;
            });
        };
        const bool hasNew = std::ranges::any_of(pool, isNew);
        int weight = 0;
        for (const auto effect : pool) if (!hasNew || isNew(effect)) weight += runUpgradeOfferWeight(effect);
        random = mixBits64(random + 0x9e3779b97f4a7c15ULL);
        int roll = static_cast<int>(random % static_cast<std::uint64_t>(weight));
        std::size_t selected = 0;
        for (std::size_t i = 0; i < pool.size(); ++i) {
            if (hasNew && !isNew(pool[i])) continue;
            roll -= runUpgradeOfferWeight(pool[i]);
            if (roll < 0) { selected = i; break; }
        }
        offer.effect = pool[selected];
        random = mixBits64(random + 0xa0761d6478bd642fULL);
        const double rarityRoll = static_cast<double>(random >> 11U) / 9007199254740992.0;
        const auto building = runUpgradeBuilding(offer.effect);
        const bool upgradingWeaponBuilding = building && runCombatBuilding(*building) &&
            runUpgradeStacks_[runUpgradeIndex(offer.effect)] > 0;
        offer.rarity = fixedRunUpgradePower(offer.effect) && offer.effect != RunUpgradeEffect::UpgradeCore && !upgradingWeaponBuilding ? fixedRunUpgradeRarity(offer.effect)
            : rollRunUpgradeRarity(activeMetaBonuses_.luck + .10 * playerPassivePower(RunUpgradeEffect::Luck), rarityRoll);
        offer.rollSeed = mixBits64(random ^
            (static_cast<std::uint64_t>(offer.effect) << 32U) ^
            static_cast<std::uint64_t>(&offer - runShop_.offers.data()));
        offer.price = runShopPrice(offer.effect, offer.rarity, runShop_.wave);
        offer.present = true;
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(selected));
    }
}

bool Simulation::openRunShop(bool debugAccess) {
    // The run shop is temporarily unavailable. Keep its state and offers
    // intact so it can be restored without a save-format change.
    (void)debugAccess;
    return false;
}
bool Simulation::closeRunShop() {
    if (!runShop_.pending || runUpgradeChoicePending_) return false;
    runShop_.pending = false;
    invalidateSnapshotCache();
    return true;
}
bool Simulation::buyRunShopOffer(std::size_t index) {
    if (!runShop_.pending || runUpgradeChoicePending_ || index >= RunShopSlotCount) return false;
    auto& offer = runShop_.offers[index];
    if (!offer.present || (!unlimitedResources_ && offer.price > coins_) || !runUpgradeAvailable(offer.effect)) return false;
    invalidateSnapshotCache();
    if (!applyRunUpgradeEffect(offer.effect, offer.rarity, 1, false, offer.rollSeed)) return false;
    if (!unlimitedResources_) coins_ -= offer.price;
    offer.present = false;
    offer.locked = false;
    if (std::ranges::none_of(runShop_.offers, [](const auto& slot) { return slot.present; }))
        generateRunShopOffers(); // Buy-out refresh does not advance the paid reroll ladder.
    return true;
}
bool Simulation::rerollRunShop() {
    if (!runShop_.pending || runUpgradeChoicePending_ || runShop_.rerolls == std::numeric_limits<int>::max() ||
        std::ranges::all_of(runShop_.offers, [this](const auto& slot) {
            return slot.present && slot.locked && runUpgradeAvailable(slot.effect);
        })) return false;
    const int price = runShopRerollPrice(runShop_);
    if (!unlimitedResources_ && coins_ < price) return false;
    if (!unlimitedResources_) coins_ -= price;
    ++runShop_.rerolls;
    generateRunShopOffers();
    invalidateSnapshotCache();
    return true;
}
bool Simulation::toggleRunShopLock(std::size_t index) {
    if (!runShop_.pending || runUpgradeChoicePending_ || index >= RunShopSlotCount) return false;
    auto& offer = runShop_.offers[index];
    if (!offer.present || (!offer.locked && !runUpgradeAvailable(offer.effect))) return false;
    offer.locked = !offer.locked;
    invalidateSnapshotCache();
    return true;
}
} // namespace ian
