#pragma once

#include <cstdint>

namespace ian {

struct EntityId {
    std::uint32_t index{};
    std::uint32_t generation{};

    friend bool operator==(const EntityId&, const EntityId&) = default;
};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

} // namespace ian
