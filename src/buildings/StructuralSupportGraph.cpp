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
    recursiveDependentCountCache_.clear();
}

bool StructuralSupportGraph::add(
    EntityId id, bool grounded,
    std::span<const EntityId> supports) {
    const std::uint64_t nodeKey = key(id);
    if (nodes_.contains(nodeKey)) {
        return false;
    }
    recursiveDependentCountCache_.clear();
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
    recursiveDependentCountCache_.clear();
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
    const std::uint64_t nodeKey = key(id);
    if (const auto cached =
            recursiveDependentCountCache_.find(nodeKey);
        cached != recursiveDependentCountCache_.end()) {
        return cached->second;
    }
    const std::size_t count = dependentIds(id, true).size();
    recursiveDependentCountCache_.emplace(nodeKey, count);
    return count;
}

std::vector<EntityId> StructuralSupportGraph::dependentIds(
    EntityId id, bool recursive) const {
    const auto root = nodes_.find(key(id));
    if (root == nodes_.end()) {
        return {};
    }
    if (!recursive) {
        std::vector<EntityId> result;
        result.reserve(root->second.dependents.size());
        for (const std::uint64_t dependent :
             root->second.dependents) {
            if (const auto node = nodes_.find(dependent);
                node != nodes_.end()) {
                result.push_back(node->second.id);
            }
        }
        return result;
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
    std::vector<EntityId> result;
    result.reserve(visited.size());
    for (const std::uint64_t dependent : visited) {
        if (const auto node = nodes_.find(dependent);
            node != nodes_.end()) {
            result.push_back(node->second.id);
        }
    }
    std::ranges::sort(
        result, [](EntityId left, EntityId right) {
            return key(left) < key(right);
        });
    return result;
}

std::vector<EntityId> StructuralSupportGraph::collapseRiskIds(
    EntityId removedSupport) const {
    return collapseRiskIds(
        std::span<const EntityId>{&removedSupport, 1U});
}

std::vector<EntityId> StructuralSupportGraph::collapseRiskIds(
    std::span<const EntityId> removedSupports) const {
    std::unordered_set<std::uint64_t> removedKeys;
    for (const EntityId support : removedSupports) {
        const std::uint64_t supportKey = key(support);
        if (nodes_.contains(supportKey)) {
            removedKeys.insert(supportKey);
        }
    }
    if (removedKeys.empty()) {
        return {};
    }

    std::unordered_set<std::uint64_t> reachable;
    std::vector<std::uint64_t> pending;
    for (const std::uint64_t removedKey : removedKeys) {
        const auto& dependents = nodes_.at(removedKey).dependents;
        pending.insert(
            pending.end(), dependents.begin(), dependents.end());
    }
    while (!pending.empty()) {
        const std::uint64_t current = pending.back();
        pending.pop_back();
        if (!reachable.insert(current).second) {
            continue;
        }
        if (const auto node = nodes_.find(current);
            node != nodes_.end()) {
            pending.insert(
                pending.end(), node->second.dependents.begin(),
                node->second.dependents.end());
        }
    }

    std::unordered_set<std::uint64_t> supported;
    std::vector<std::uint64_t> supportQueue;
    supportQueue.reserve(nodes_.size());
    for (const auto& [nodeKey, node] : nodes_) {
        if (!removedKeys.contains(nodeKey) && node.grounded) {
            supported.insert(nodeKey);
            supportQueue.push_back(nodeKey);
        }
    }
    for (std::size_t queueIndex = 0;
         queueIndex < supportQueue.size(); ++queueIndex) {
        const auto support = nodes_.find(
            supportQueue[queueIndex]);
        if (support == nodes_.end()) {
            continue;
        }
        for (const std::uint64_t dependentKey :
             support->second.dependents) {
            if (removedKeys.contains(dependentKey) ||
                supported.contains(dependentKey)) {
                continue;
            }
            const auto dependent = nodes_.find(dependentKey);
            if (dependent == nodes_.end()) {
                continue;
            }
            if (std::ranges::any_of(
                    dependent->second.supports,
                    [&supported](std::uint64_t supportKey) {
                        return supported.contains(supportKey);
                    })) {
                if (supported.insert(dependentKey).second) {
                    supportQueue.push_back(dependentKey);
                }
            }
        }
    }

    std::vector<EntityId> result;
    result.reserve(reachable.size());
    for (const std::uint64_t nodeKey : reachable) {
        if (!removedKeys.contains(nodeKey) &&
            !supported.contains(nodeKey)) {
            result.push_back(nodes_.at(nodeKey).id);
        }
    }
    std::ranges::sort(
        result, [](EntityId left, EntityId right) {
            return key(left) < key(right);
        });
    return result;
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
