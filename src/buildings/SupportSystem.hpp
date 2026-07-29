#pragma once

#include "buildings/PlacementValidator.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace ian {

struct SharedSupport {
    std::uint32_t id{};
    GridCoord corner;
    Vec3 top;
    Vec3 bottom;
    double length{};
    std::uint32_t referenceCount{};
    bool active{};
};

class SupportSystem {
  public:
    void reset();
    [[nodiscard]] std::array<std::uint32_t, 4>
    acquire(const PlatformFramePlacement& placement);
    void release(
        const std::array<std::uint32_t, 4>& supportIds);

    [[nodiscard]] std::span<const SharedSupport>
    supports() const;
    [[nodiscard]] std::size_t activeSupportCount() const;

  private:
    struct CornerHash {
        [[nodiscard]] std::size_t operator()(
            const GridCoord& corner) const;
    };

    std::vector<SharedSupport> supports_;
    std::unordered_map<
        GridCoord, std::size_t, CornerHash>
        supportByCorner_;
    std::uint32_t nextId_{1U};
};

} // namespace ian
