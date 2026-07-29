#include "buildings/StructuralSupportGraph.hpp"

#include <algorithm>

namespace ian {

std::uint64_t StructuralSupportGraph::key(EntityId id) {
    return
        (static_cast<std::uint64_t>(id.generation) << 32U) |
        id.index;
}

void StructuralSupportGraph::reset() {
    nodes_.clear();
    dirty_.clear();
    dirtySet_.clear();
    unsupported_.clear();
}

bool StructuralSupportGraph::add(
    EntityId id, bool grounded,
    std::span<const EntityId> supports) {
    const std::uint64_t nodeKey = key(id);
    if (nodes_.contains(nodeKey)) {
        return false;
    }
    Node node{
        .id = id,
        .grounded = grounded,
    };
    node.supports.reserve(supports.size());
    for (const EntityId support : supports) {
        const std::uint64_t supportKey = key(support);
        if (!nodes_.contains(supportKey) ||
            std::ranges::find(
                node.supports, supportKey) !=
                node.supports.end()) {
            continue;
        }
        node.supports.push_back(supportKey);
    }
    nodes_.emplace(nodeKey, std::move(node));
    for (const std::uint64_t supportKey :
         nodes_.at(nodeKey).supports) {
        nodes_.at(supportKey).dependents.push_back(nodeKey);
    }
    markDirty(nodeKey);
    resolveDirty();
    return true;
}

bool StructuralSupportGraph::remove(EntityId id) {
    const std::uint64_t nodeKey = key(id);
    const auto iterator = nodes_.find(nodeKey);
    if (iterator == nodes_.end()) {
        return false;
    }
    const std::vector<std::uint64_t> supports =
        iterator->second.supports;
    const std::vector<std::uint64_t> dependents =
        iterator->second.dependents;
    for (const std::uint64_t supportKey : supports) {
        const auto support = nodes_.find(supportKey);
        if (support != nodes_.end()) {
            std::erase(
                support->second.dependents, nodeKey);
        }
    }
    for (const std::uint64_t dependentKey : dependents) {
        const auto dependent = nodes_.find(dependentKey);
        if (dependent == nodes_.end()) {
            continue;
        }
        std::erase(
            dependent->second.supports, nodeKey);
        markDirty(dependentKey);
    }
    unsupported_.erase(nodeKey);
    dirtySet_.erase(nodeKey);
    nodes_.erase(iterator);
    resolveDirty();
    return true;
}

std::vector<EntityId> StructuralSupportGraph::update(
    double deltaSeconds, bool collapseEnabled,
    double collapseDelaySeconds) {
    std::vector<EntityId> collapsed;
    if (!collapseEnabled) {
        for (const std::uint64_t nodeKey : unsupported_) {
            nodes_.at(nodeKey).unsupportedSeconds = 0.0;
        }
        return collapsed;
    }
    const double safeDelta = std::max(0.0, deltaSeconds);
    const double safeDelay =
        std::max(0.0, collapseDelaySeconds);
    for (const std::uint64_t nodeKey : unsupported_) {
        Node& node = nodes_.at(nodeKey);
        node.unsupportedSeconds += safeDelta;
        if (node.unsupportedSeconds >= safeDelay) {
            collapsed.push_back(node.id);
        }
    }
    for (const EntityId id : collapsed) {
        static_cast<void>(remove(id));
    }
    return collapsed;
}

StructuralSupportState StructuralSupportGraph::state(
    EntityId id) const {
    const auto iterator = nodes_.find(key(id));
    return iterator == nodes_.end()
               ? StructuralSupportState::Unsupported
               : iterator->second.state;
}

bool StructuralSupportGraph::contains(EntityId id) const {
    return nodes_.contains(key(id));
}

std::size_t StructuralSupportGraph::nodeCount() const {
    return nodes_.size();
}

std::size_t
StructuralSupportGraph::unsupportedCount() const {
    return unsupported_.size();
}

std::size_t StructuralSupportGraph::dependentCount(
    EntityId id, bool recursive) const {
    const auto root = nodes_.find(key(id));
    if (root == nodes_.end()) {
        return 0U;
    }
    if (!recursive) {
        return root->second.dependents.size();
    }

    std::unordered_set<std::uint64_t> visited;
    std::vector<std::uint64_t> pending =
        root->second.dependents;
    while (!pending.empty()) {
        const std::uint64_t current = pending.back();
        pending.pop_back();
        if (!visited.insert(current).second) {
            continue;
        }
        const auto node = nodes_.find(current);
        if (node == nodes_.end()) {
            continue;
        }
        pending.insert(
            pending.end(),
            node->second.dependents.begin(),
            node->second.dependents.end());
    }
    return visited.size();
}

void StructuralSupportGraph::markDirty(
    std::uint64_t node) {
    if (nodes_.contains(node) &&
        dirtySet_.insert(node).second) {
        dirty_.push_back(node);
    }
}

void StructuralSupportGraph::resolveDirty() {
    while (!dirty_.empty()) {
        const std::uint64_t nodeKey = dirty_.back();
        dirty_.pop_back();
        if (dirtySet_.erase(nodeKey) == 0U) {
            continue;
        }
        const auto iterator = nodes_.find(nodeKey);
        if (iterator == nodes_.end()) {
            continue;
        }
        Node& node = iterator->second;
        bool supported = node.grounded;
        if (!supported) {
            supported = std::ranges::any_of(
                node.supports,
                [this](std::uint64_t supportKey) {
                    const auto support =
                        nodes_.find(supportKey);
                    return support != nodes_.end() &&
                           support->second.state ==
                               StructuralSupportState::
                                   Supported;
                });
        }
        const StructuralSupportState newState =
            supported
                ? StructuralSupportState::Supported
                : StructuralSupportState::Unsupported;
        if (node.state == newState) {
            if (newState ==
                StructuralSupportState::Unsupported) {
                unsupported_.insert(nodeKey);
            } else {
                unsupported_.erase(nodeKey);
            }
            continue;
        }
        node.state = newState;
        node.unsupportedSeconds = 0.0;
        if (newState ==
            StructuralSupportState::Unsupported) {
            unsupported_.insert(nodeKey);
        } else {
            unsupported_.erase(nodeKey);
        }
        for (const std::uint64_t dependent :
             node.dependents) {
            markDirty(dependent);
        }
    }
}

} // namespace ian
