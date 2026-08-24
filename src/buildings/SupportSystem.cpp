#include "buildings/SupportSystem.hpp"

#include <algorithm>
#include <functional>

namespace ian {

std::size_t SupportSystem::CornerHash::operator()(
    const GridCoord& corner) const {
    std::size_t seed = std::hash<int>{}(corner.x);
    seed ^= std::hash<int>{}(corner.yLevel) +
            0x9e3779b9U + (seed << 6U) +
            (seed >> 2U);
    seed ^= std::hash<int>{}(corner.z) +
            0x9e3779b9U + (seed << 6U) +
            (seed >> 2U);
    return seed;
}

void SupportSystem::reset() {
    supports_.clear();
    supportByCorner_.clear();
    nextId_ = 1U;
}

std::array<std::uint32_t, 4>
SupportSystem::acquire(
    const PlatformFramePlacement& placement) {
    constexpr int width = PlatformFrameWidthCells;
    const std::array<GridCoord, 4> corners{{
        {placement.anchor.x, placement.anchor.yLevel,
         placement.anchor.z},
        {placement.anchor.x + width,
         placement.anchor.yLevel, placement.anchor.z},
        {placement.anchor.x, placement.anchor.yLevel,
         placement.anchor.z + width},
        {placement.anchor.x + width,
         placement.anchor.yLevel,
         placement.anchor.z + width},
    }};
    std::array<std::uint32_t, 4> ids{};
    for (std::size_t index = 0;
         index < corners.size(); ++index) {
        const auto existing =
            supportByCorner_.find(corners[index]);
        if (existing != supportByCorner_.end()) {
            SharedSupport& support =
                supports_[existing->second];
            ++support.referenceCount;
            ids[index] = support.id;
            continue;
        }
        const std::size_t supportIndex = supports_.size();
        const SharedSupport support{
            .id = nextId_++,
            .corner = corners[index],
            .top = placement.supports[index].top,
            .bottom = placement.supports[index].bottom,
            .length = placement.supports[index].length,
            .referenceCount = 1U,
            .active = true,
        };
        ids[index] = support.id;
        supports_.push_back(support);
        supportByCorner_.emplace(
            corners[index], supportIndex);
    }
    return ids;
}

void SupportSystem::release(
    const std::array<std::uint32_t, 4>& supportIds) {
    for (const std::uint32_t id : supportIds) {
        if (id == 0U || id > supports_.size()) {
            continue;
        }
        SharedSupport& support = supports_[id - 1U];
        if (!support.active || support.id != id) {
            continue;
        }
        if (support.referenceCount > 1U) {
            --support.referenceCount;
            continue;
        }
        supportByCorner_.erase(support.corner);
        support.referenceCount = 0U;
        support.active = false;
    }
}

std::span<const SharedSupport>
SupportSystem::supports() const {
    return supports_;
}

std::size_t SupportSystem::activeSupportCount() const {
    return static_cast<std::size_t>(
        std::count_if(
            supports_.begin(), supports_.end(),
            [](const SharedSupport& support) {
                return support.active;
            }));
}

} // namespace ian
