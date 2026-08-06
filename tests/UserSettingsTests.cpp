#include "TestHarness.hpp"
#include "app/UserSettings.hpp"

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path settingsTestPath() {
    return std::filesystem::temp_directory_path() /
           "ian_user_settings_test.json";
}

void settingsRoundTrip() {
    const auto path = settingsTestPath();
    std::error_code error;
    std::filesystem::remove(path, error);

    ian::UserSettings written;
    written.graphics.inkOutlines = true;
    written.graphics.outlineWidth = 3.0F;
    written.graphics.pixelSize = 6;
    written.graphics.frameRateLimit = 144;
    written.graphics.quality = ian::GraphicsQuality::Medium;
    written.audio.masterVolume = 0.31F;
    written.audio.muted = true;
    written.motion.bobIntensity = 0.42F;

    require(ian::saveUserSettings(path.string(), written),
            "user settings save succeeds");
    ian::UserSettings loaded;
    require(ian::loadUserSettings(path.string(), loaded),
            "user settings load succeeds");
    require(loaded.graphics.inkOutlines,
            "style toggle survives settings round trip");
    requireNear(loaded.graphics.outlineWidth, 3.0, 1e-6,
                "style slider survives settings round trip");
    require(loaded.graphics.pixelSize == 6,
            "display setting survives settings round trip");
    require(loaded.graphics.frameRateLimit == 144,
            "frame rate limit survives settings round trip");
    require(loaded.graphics.quality == ian::GraphicsQuality::Medium,
            "graphics quality survives settings round trip");
    requireNear(loaded.audio.masterVolume, 0.31, 1e-6,
                "audio setting survives settings round trip");
    require(loaded.audio.muted,
            "audio mute survives settings round trip");
    requireNear(loaded.motion.bobIntensity, 0.42, 1e-6,
                "motion setting survives settings round trip");
    std::filesystem::remove(path, error);
}

void loadedValuesAreValidated() {
    const auto path = settingsTestPath();
    {
        std::ofstream stream(path);
        stream << R"({
            "graphics": {
                "pixelSize": 5,
                "frameRateLimit": 119,
                "outlineWidth": 99.0,
                "contrast": -10.0
            },
            "audio": {"masterVolume": 4.0},
            "motion": {"shakeIntensity": -2.0}
        })";
    }
    ian::UserSettings loaded;
    require(ian::loadUserSettings(path.string(), loaded),
            "valid settings document loads");
    require(loaded.graphics.pixelSize == 4,
            "unsupported pixel size selects nearest valid size");
    require(loaded.graphics.frameRateLimit == 120,
            "unsupported frame rate selects nearest valid limit");
    requireNear(loaded.graphics.outlineWidth, 4.0, 1e-6,
                "outline width is clamped");
    requireNear(loaded.graphics.contrast, 0.5, 1e-6,
                "contrast is clamped");
    requireNear(loaded.audio.masterVolume, 1.0, 1e-6,
                "audio volume is clamped");
    requireNear(loaded.motion.shakeIntensity, 0.0, 1e-6,
                "motion intensity is clamped");
    std::error_code error;
    std::filesystem::remove(path, error);
}

void tabResetsPreserveOtherTabs() {
    ian::GraphicsSettings settings;
    settings.shadows = false;
    settings.pixelSize = 8;
    settings.frameRateLimit = 144;
    settings.brightness = 0.4F;
    settings.inkOutlines = true;

    ian::resetDisplaySettings(settings);
    require(settings.shadows && settings.pixelSize == 3 &&
                settings.frameRateLimit == 60,
            "display reset restores display defaults");
    requireNear(settings.brightness, 0.4, 1e-6,
                "display reset preserves color tab");
    require(settings.inkOutlines,
            "display reset preserves style tab");

    ian::resetColorSettings(settings);
    requireNear(settings.brightness, 0.0, 1e-6,
                "color reset restores color defaults");
    require(settings.inkOutlines,
            "color reset preserves style tab");

    ian::resetStyleSettings(settings);
    require(!settings.inkOutlines,
            "style reset restores style defaults");
}

} // namespace

void runUserSettingsTests() {
    settingsRoundTrip();
    loadedValuesAreValidated();
    tabResetsPreserveOtherTabs();
}
