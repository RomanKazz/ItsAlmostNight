#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

enum class SkillBranch { Root, Gathering, Weapons, Construction };
enum class SkillEffect {
    BareHands,
    UnlockAxe,
    UnlockPickaxe,
    UnlockClub,
    UnlockIceWand,
    UnlockHammer,
    UnlockRifle,
    AutoSwitchTools,
};
enum class SkillNodeState { Hidden, Locked, Available, Unlocked };
enum class SkillPurchaseError {
    None,
    InvalidNode,
    AlreadyUnlocked,
    DependenciesLocked,
    InsufficientPoints,
};

struct SkillTreePoint { float x{}; float y{}; };

struct SkillNodeDefinition {
    std::string id;
    std::string title;
    std::string description;
    std::string icon;
    SkillBranch branch{SkillBranch::Root};
    SkillTreePoint position;
    int cost{1};
    std::vector<std::string> prerequisites;
    SkillEffect effect{SkillEffect::BareHands};
};

struct SkillTreeRunState {
    int points{};
    std::vector<std::string> unlockedNodeIds;
};

class SkillTree {
  public:
    SkillTree();
    explicit SkillTree(std::vector<SkillNodeDefinition> nodes);

    [[nodiscard]] static std::vector<SkillNodeDefinition> defaultDefinitions();
    [[nodiscard]] const std::vector<SkillNodeDefinition>& nodes() const;
    [[nodiscard]] SkillNodeState state(std::size_t index) const;
    [[nodiscard]] SkillNodeState state(std::string_view id) const;
    [[nodiscard]] SkillPurchaseError purchase(
        std::size_t index, bool spendPoints = true);
    [[nodiscard]] bool unlock(std::size_t index);
    void grantPoints(int amount);
    [[nodiscard]] int points() const;
    [[nodiscard]] bool isUnlocked(std::string_view id) const;
    [[nodiscard]] bool hasEffect(SkillEffect effect) const;
    [[nodiscard]] std::optional<std::size_t> indexOf(std::string_view id) const;
    [[nodiscard]] std::vector<std::size_t> childrenOf(std::string_view id) const;
    [[nodiscard]] int unlockedCount() const;
    [[nodiscard]] SkillTreeRunState saveState() const;
    [[nodiscard]] bool loadState(const SkillTreeRunState& state);
    void reset();

  private:
    [[nodiscard]] bool prerequisitesUnlocked(const SkillNodeDefinition& node) const;
    std::vector<SkillNodeDefinition> nodes_;
    std::vector<bool> unlocked_;
    int points_{};
};

[[nodiscard]] std::vector<SkillNodeDefinition> loadSkillTreeDefinitions(
    const std::filesystem::path& path);
[[nodiscard]] std::string_view skillBranchName(SkillBranch branch);
[[nodiscard]] std::string_view skillEffectName(SkillEffect effect);

} // namespace ian
