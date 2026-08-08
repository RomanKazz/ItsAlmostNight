#include "game/Simulation.hpp"

#include <algorithm>

namespace ian {
const SkillTree& Simulation::skillTree() const { return skillTree_; }

std::uint64_t Simulation::structuralRevision() const {
    return structuralRevision_;
}

SkillPurchaseError Simulation::purchaseSkill(std::size_t index) {
    invalidateSnapshotCache();
    const SkillPurchaseError result = skillTree_.purchase(
        index, !unlimitedResources_);
    if (result != SkillPurchaseError::None) return result;
    const SkillEffect effect = skillTree_.nodes()[index].effect;
    switch (effect) {
    case SkillEffect::UnlockAxe: playerWeapons_.selectWeapon(PlayerWeapon::Axe); break;
    case SkillEffect::UnlockPickaxe: playerWeapons_.selectWeapon(PlayerWeapon::Pickaxe); break;
    case SkillEffect::UnlockClub: playerWeapons_.selectWeapon(PlayerWeapon::Club); break;
    case SkillEffect::UnlockIceWand: playerWeapons_.selectWeapon(PlayerWeapon::IceWand); break;
    case SkillEffect::UnlockHammer: playerWeapons_.selectWeapon(PlayerWeapon::Hammer); break;
    case SkillEffect::UnlockRifle: playerWeapons_.selectWeapon(PlayerWeapon::Rifle); break;
    case SkillEffect::AutoSwitchTools: break;
    case SkillEffect::BareHands: break;
    }
    selectedBuilding_.reset();
    buildingPreview_.reset();
    events_.push_back({.type = GameEventType::SkillUnlocked,
                       .amount = static_cast<int>(index)});
    return result;
}

void Simulation::grantSkillPoints(int amount, SkillPointSource source) {
    if (amount <= 0) return;
    invalidateSnapshotCache();
    skillTree_.grantPoints(amount);
    events_.push_back({.type = GameEventType::SkillPointsGranted,
                       .amount = amount,
                       .intensity = static_cast<double>(source)});
}

SkillTreeRunState Simulation::saveSkillTreeState() const { return skillTree_.saveState(); }

bool Simulation::loadSkillTreeState(const SkillTreeRunState& state) {
    if (!skillTree_.loadState(state)) return false;
    invalidateSnapshotCache();
    playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
    return true;
}

void Simulation::cycleUnlockedTool() {
    std::vector<PlayerWeapon> tools{PlayerWeapon::BareHands};
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockAxe))
        tools.push_back(PlayerWeapon::Axe);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockPickaxe))
        tools.push_back(PlayerWeapon::Pickaxe);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockClub))
        tools.push_back(PlayerWeapon::Club);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockIceWand))
        tools.push_back(PlayerWeapon::IceWand);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockHammer))
        tools.push_back(PlayerWeapon::Hammer);
    if (unlimitedResources_ || skillTree_.hasEffect(SkillEffect::UnlockRifle))
        tools.push_back(PlayerWeapon::Rifle);
    const auto current = std::ranges::find(tools, playerWeapons_.selectedWeapon());
    const std::size_t next = current == tools.end()
        ? 0 : (static_cast<std::size_t>(std::distance(tools.begin(), current)) + 1) % tools.size();
    playerWeapons_.selectWeapon(tools[next]);
    selectedBuilding_.reset();
    buildingPreview_.reset();
}

} // namespace ian
