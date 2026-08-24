#include "TestHarness.hpp"
#include "app/UserSettings.hpp"
#include "localization/Localization.hpp"
#include "progression/MetaProgression.hpp"

#include <filesystem>
#include <fstream>
#include <string>

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
    written.graphics.fullscreen = true;
    written.graphics.inkOutlines = true;
    written.graphics.outlineWidth = 3.0F;
    written.graphics.pixelSize = 6;
    written.graphics.frameRateLimit = 144;
    written.graphics.performanceLogging = true;
    written.graphics.quality = ian::GraphicsQuality::Medium;
    written.audio.masterVolume = 0.31F;
    written.audio.muted = true;
    written.motion.bobIntensity = 0.42F;
    written.controls.mouseSensitivity = 1.65F;
    written.controls.invertMouseY = true;
    ian::setControlKey(
        written.controls, ian::ControlAction::Interact, KEY_F);
    ian::setControlKey(
        written.controls, ian::ControlAction::Attack, KEY_T);
    written.accessibility.showFps = true;
    written.accessibility.reduceFlashes = true;
    written.language = ian::Language::Russian;

    require(ian::saveUserSettings(path.string(), written),
            "user settings save succeeds");
    ian::UserSettings loaded;
    require(ian::loadUserSettings(path.string(), loaded),
            "user settings load succeeds");
    require(loaded.graphics.inkOutlines,
            "style toggle survives settings round trip");
    require(loaded.graphics.fullscreen,
            "fullscreen setting survives settings round trip");
    requireNear(loaded.graphics.outlineWidth, 3.0, 1e-6,
                "style slider survives settings round trip");
    require(loaded.graphics.pixelSize == 6,
            "display setting survives settings round trip");
    require(loaded.graphics.frameRateLimit == 144,
            "frame rate limit survives settings round trip");
    require(loaded.graphics.performanceLogging,
            "performance logging toggle survives settings round trip");
    require(loaded.graphics.quality == ian::GraphicsQuality::Medium,
            "graphics quality survives settings round trip");
    requireNear(loaded.audio.masterVolume, 0.31, 1e-6,
                "audio setting survives settings round trip");
    require(loaded.audio.muted,
            "audio mute survives settings round trip");
    requireNear(loaded.motion.bobIntensity, 0.42, 1e-6,
                "motion setting survives settings round trip");
    requireNear(loaded.controls.mouseSensitivity, 1.65, 1e-6,
                "mouse sensitivity survives settings round trip");
    require(loaded.controls.invertMouseY,
            "mouse inversion survives settings round trip");
    require(ian::controlKey(
                loaded.controls, ian::ControlAction::Interact) == KEY_F &&
                ian::controlKey(
                    loaded.controls, ian::ControlAction::Attack) == KEY_T,
            "key bindings survive settings round trip");
    require(loaded.accessibility.showFps &&
                loaded.accessibility.reduceFlashes,
            "accessibility settings survive round trip");
    require(loaded.language == ian::Language::Russian,
            "language survives settings round trip");
    std::filesystem::remove(path, error);
}

void metaProgressionRoundTripAndUnlocks() {
    const auto path = std::filesystem::temp_directory_path() /
        "ian_meta_progression_test.json";
    std::error_code error;
    std::filesystem::remove(path, error);

    ian::MetaProgression written{
        .runsPlayed = 7,
        .stageClears = 1,
        .bestWave = 8,
        .enemiesDefeated = 251,
        .lootCollected = 22,
        .resourcesGathered = 900,
    };
    require(ian::saveMetaProgression(path.string(), written),
            "meta progression save succeeds");
    ian::MetaProgression loaded;
    require(ian::loadMetaProgression(path.string(), loaded) &&
                loaded == written,
            "meta progression survives a JSON round trip");
    require(
        ian::isPlayerClassUnlocked(
            ian::PlayerClass::Berserker, loaded) &&
        ian::isPlayerClassUnlocked(
            ian::PlayerClass::Vampire, loaded) &&
        ian::isPlayerClassUnlocked(
            ian::PlayerClass::Alchemist, loaded) &&
        ian::isPlayerClassUnlocked(
            ian::PlayerClass::Chronomancer, loaded),
        "meta milestones unlock all challenge classes");

    const ian::MetaProgression fresh;
    require(
        !ian::isPlayerClassUnlocked(
            ian::PlayerClass::Berserker, fresh) &&
        ian::isPlayerClassUnlocked(
            ian::PlayerClass::Prospector, fresh),
        "new profile locks challenge classes but keeps launch classes");
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
            "motion": {"shakeIntensity": -2.0},
            "controls": {"mouseSensitivity": 99.0}
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
    requireNear(loaded.controls.mouseSensitivity, 3.0, 1e-6,
                "mouse sensitivity is clamped");
    require(ian::controlKey(
                loaded.controls, ian::ControlAction::MoveForward) == KEY_W,
            "missing key bindings keep defaults");
    require(
        ian::controlKey(
            loaded.controls, ian::ControlAction::Dash) == KEY_NULL,
        "Dash defaults to the right mouse button sentinel");
    std::error_code error;
    std::filesystem::remove(path, error);
}

void graphicsPresetsAreCompleteAndDetectable() {
    ian::GraphicsSettings settings;
    settings.brightness = 0.31F;
    settings.inkOutlines = true;
    settings.frameRateLimit = 144;
    settings.performanceLogging = true;

    ian::applyGraphicsPreset(settings, ian::GraphicsQuality::Low);
    require(ian::detectGraphicsPreset(settings) ==
                ian::GraphicsQuality::Low,
            "low graphics preset is detectable");
    require(!settings.shadows && !settings.grass &&
                !settings.ssao && settings.pixelSize == 1,
            "low preset applies performance settings together");
    requireNear(settings.brightness, 0.31, 1e-6,
                "graphics preset preserves color calibration");
    require(settings.inkOutlines && settings.frameRateLimit == 144,
            "graphics preset preserves style and FPS choices");
    require(settings.performanceLogging,
            "graphics preset preserves diagnostics choice");

    settings.shadows = true;
    require(!ian::detectGraphicsPreset(settings),
            "manual tuning is reported as custom");

    ian::applyGraphicsPreset(settings, ian::GraphicsQuality::Medium);
    require(settings.ssao && settings.pixelSize == 1,
            "medium preset enables SSAO without pixelization");
    ian::applyGraphicsPreset(settings, ian::GraphicsQuality::High);
    require(settings.ssao && settings.pixelSize == 1,
            "high preset enables SSAO without pixelization");
}

void tabResetsPreserveOtherTabs() {
    ian::GraphicsSettings settings;
    settings.shadows = false;
    settings.pixelSize = 8;
    settings.frameRateLimit = 144;
    settings.performanceLogging = true;
    settings.brightness = 0.4F;
    settings.inkOutlines = true;

    ian::resetDisplaySettings(settings);
    require(settings.shadows && settings.pixelSize == 1 &&
                settings.frameRateLimit == 60 &&
                !settings.performanceLogging,
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

void localizationCatalogTranslatesFixedAndDynamicText() {
    ian::initializeLocalization(
        std::string(IAN_SOURCE_DIR) +
        "/assets/data/localization.json");
    ian::setLanguage(ian::Language::Russian);
    require(ian::localizeText("SETTINGS") == "НАСТРОЙКИ",
            "localization translates fixed labels");
    require(ian::localizeText("WAVE 3   •   BEST 2") ==
                "ВОЛНА 3   •   ЛУЧШИЙ 2",
            "localization translates dynamic HUD labels");
    require(ian::localizeText("Core upgraded") == "Ядро улучшено",
            "localization translates event messages");
    require(
        ian::localizeText("LIGHT FOOTWORK") ==
            "ЛЁГКАЯ ПОХОДКА" &&
        ian::localizeText("Select Wall") ==
            "Выбрать стену" &&
        ian::localizeText("Attack Flying") ==
            "Атаковать летающего врага",
        "localization covers movement skills and interaction prompts");
    require(
        ian::localizeText(
            "Q  COPY    F  REPAIR    U  UPGRADE") ==
            "Q  КОПИРОВАТЬ    F  РЕМОНТ    U  УЛУЧШИТЬ",
        "localization translates labels assembled with key bindings");
    require(
        ian::localizeText("BUILDINGS") == "ЗДАНИЯ" &&
        ian::localizeText("3 BUILDINGS") == "3 ЗДАНИЙ" &&
        ian::localizeText("PIECES") == "ДЕТАЛИ" &&
        ian::localizeText("12 PIECES") == "12 ДЕТАЛЕЙ",
        "localization keeps correct forms for titles and counters");
    ian::setLanguage(ian::Language::English);
    require(ian::localizeText("SETTINGS") == "SETTINGS",
            "english localization keeps source text");
}

} // namespace

void runUserSettingsTests() {
    settingsRoundTrip();
    metaProgressionRoundTripAndUnlocks();
    loadedValuesAreValidated();
    tabResetsPreserveOtherTabs();
    graphicsPresetsAreCompleteAndDetectable();
    localizationCatalogTranslatesFixedAndDynamicText();
}
