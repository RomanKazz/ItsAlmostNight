#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

namespace ian {

// Per-program cache for float/vec uniforms owned exclusively by the caller.
// Reset on relink/reload, including when OpenGL reuses a program ID.
class UniformValueCache {
public:
    void reset() { values_.clear(); }

    [[nodiscard]] bool update(int location, std::span<const float> value) {
        if (location < 0 || value.empty()) return false;
        if (value.size() > 4U) return true;
        const auto index = static_cast<std::size_t>(location);
        if (index >= values_.size()) values_.resize(index + 1U);
        Entry& entry = values_[index];
        const std::size_t bytes = value.size_bytes();
        if (entry.size == value.size() &&
            std::memcmp(entry.value.data(), value.data(), bytes) == 0) {
            return false;
        }
        std::memcpy(entry.value.data(), value.data(), bytes);
        entry.size = value.size();
        return true;
    }

private:
    struct Entry {
        std::array<float, 4> value{};
        std::size_t size{};
    };
    std::vector<Entry> values_;
};

} // namespace ian
