#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

enum class SkillBranch {
    Root,
    Construction,
    Defenses,
    Weapons,
    Survival,
};

enum class SkillNodeState {
    Hidden,
    Locked,
    Available,
    Unlocked,
};

struct SkillTreePoint {
    float x{};
    float y{};
};

struct SkillNodeDefinition {
    std::string id;
    std::string title;
    std::string description;
    SkillBranch branch{SkillBranch::Root};
    SkillTreePoint position;
    int cost{1};
    std::vector<std::string> prerequisites;
};

class SkillTree {
  public:
    SkillTree();

    [[nodiscard]] const std::vector<SkillNodeDefinition>&
    nodes() const;
    [[nodiscard]] SkillNodeState state(std::size_t index) const;
    [[nodiscard]] SkillNodeState state(std::string_view id) const;
    [[nodiscard]] bool unlock(std::size_t index);
    [[nodiscard]] bool isUnlocked(std::string_view id) const;
    [[nodiscard]] std::optional<std::size_t> indexOf(
        std::string_view id) const;
    [[nodiscard]] std::vector<std::size_t> childrenOf(
        std::string_view id) const;
    [[nodiscard]] int unlockedCount() const;
    void reset();

  private:
    [[nodiscard]] bool prerequisitesUnlocked(
        const SkillNodeDefinition& node) const;
    [[nodiscard]] bool shouldBeVisible(
        const SkillNodeDefinition& node) const;

    std::vector<SkillNodeDefinition> nodes_;
    std::vector<bool> unlocked_;
};

[[nodiscard]] std::string_view skillBranchName(SkillBranch branch);

} // namespace ian
