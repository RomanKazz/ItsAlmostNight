#pragma once

#include "core/Types.hpp"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ian {

enum class StructuralSupportState {
    Supported,
    Unsupported,
};

class StructuralSupportGraph {
  public:
    void reset();
    [[nodiscard]] bool add(
        EntityId id, bool grounded,
        std::span<const EntityId> supports = {});
    [[nodiscard]] bool remove(EntityId id);

    // Returns nodes whose collapse delay expired. Returned
    // nodes and their links have already been removed.
    [[nodiscard]] std::vector<EntityId> update(
        double deltaSeconds, bool collapseEnabled,
        double collapseDelaySeconds);

    [[nodiscard]] StructuralSupportState state(
        EntityId id) const;
    [[nodiscard]] bool contains(EntityId id) const;
    [[nodiscard]] std::size_t nodeCount() const;
    [[nodiscard]] std::size_t unsupportedCount() const;
    [[nodiscard]] std::size_t dependentCount(
        EntityId id, bool recursive = true) const;
    [[nodiscard]] std::vector<EntityId> dependentIds(
        EntityId id, bool recursive = true) const;
    [[nodiscard]] std::vector<EntityId> collapseRiskIds(
        EntityId removedSupport) const;
    [[nodiscard]] std::vector<EntityId> collapseRiskIds(
        std::span<const EntityId> removedSupports) const;

  private:
    struct Node {
        EntityId id;
        bool grounded{};
        StructuralSupportState state{
            StructuralSupportState::Unsupported};
        double unsupportedSeconds{};
        std::vector<std::uint64_t> supports;
        std::vector<std::uint64_t> dependents;
    };

    [[nodiscard]] static std::uint64_t key(EntityId id);
    void markDirty(std::uint64_t node);
    void resolveDirty();

    std::unordered_map<std::uint64_t, Node> nodes_;
    std::vector<std::uint64_t> dirty_;
    std::unordered_set<std::uint64_t> dirtySet_;
    std::unordered_set<std::uint64_t> unsupported_;
    mutable std::unordered_map<std::uint64_t, std::size_t>
        recursiveDependentCountCache_;
};

} // namespace ian
