#pragma once

#include <cstdint>

namespace ian {

[[nodiscard]] constexpr std::uint64_t mixBits64(
    std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] constexpr double unitRandom(
    std::uint64_t seed) {
    constexpr double Scale =
        1.0 / 9007199254740992.0;
    return static_cast<double>(mixBits64(seed) >> 11U) *
           Scale;
}

} // namespace ian
