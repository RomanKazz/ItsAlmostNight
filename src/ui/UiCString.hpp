#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace ian {

template <typename Function>
decltype(auto) withNullTerminatedUiText(
    std::string_view text, Function&& function) {
    constexpr std::size_t StackCapacity = 256;
    if (text.size() < StackCapacity) {
        std::array<char, StackCapacity> buffer{};
        std::copy(text.begin(), text.end(), buffer.begin());
        return std::forward<Function>(function)(buffer.data());
    }
    const std::string owned{text};
    return std::forward<Function>(function)(owned.c_str());
}

} // namespace ian
