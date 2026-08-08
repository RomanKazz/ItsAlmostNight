#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ian {

// The language is deliberately a small, data-driven enum for now. Adding a
// language only requires another catalog in assets/data/localization.json.
enum class Language : std::uint8_t {
    English,
    Russian,
};

void initializeLocalization(
    std::string_view catalogPath =
        "assets/data/localization.json");
void setLanguage(Language language);
[[nodiscard]] Language currentLanguage();
[[nodiscard]] std::string_view languageCode(Language language);
[[nodiscard]] std::string_view languageName(Language language);

// Returns the source text unchanged for English. For Russian, exact catalog
// entries are preferred and common dynamic HUD phrases are translated by the
// same function, so callers can pass both fixed labels and runtime values.
[[nodiscard]] std::string localizeText(std::string_view text);

} // namespace ian
