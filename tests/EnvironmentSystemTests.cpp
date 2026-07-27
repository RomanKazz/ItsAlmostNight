#include "TestHarness.hpp"
#include "graphics/EnvironmentSystem.hpp"

#include <string>

namespace {

void automaticTimeUsesProfileAnchors() {
    ian::EnvironmentSystem environment;

    environment.setAutomaticTime(0.25F);
    const auto day = environment.state();
    requireNear(day.timeOfDay, 0.25, 1e-6,
                "day anchor uses normalized visual time");
    requireNear(day.nightFactor, 0.0, 1e-6,
                "day anchor has no night contribution");

    environment.setAutomaticTime(0.75F);
    const auto night = environment.state();
    requireNear(night.timeOfDay, 0.75, 1e-6,
                "night anchor uses normalized visual time");
    requireNear(night.nightFactor, 1.0, 1e-6,
                "night anchor uses night contribution");
}

void interpolationIsSmoothAndCyclic() {
    ian::EnvironmentSystem environment;

    environment.setAutomaticTime(0.625F);
    const auto duskToNight = environment.state();
    require(duskToNight.nightFactor > 0.55F &&
                duskToNight.nightFactor < 1.0F,
            "dusk-to-night state interpolates profile values");

    environment.setAutomaticTime(1.0F);
    requireNear(environment.timeOfDay(), 0.0, 1e-6,
                "visual time wraps after night");
    require(std::string(environment.nearestProfileName()) == "dawn",
            "wrapped visual time reaches dawn");
}

void debugOverridesControlAutomaticTime() {
    ian::EnvironmentSystem environment;
    environment.setAutomaticTime(0.25F);
    environment.toggleFrozen();
    environment.setAutomaticTime(0.75F);
    requireNear(environment.timeOfDay(), 0.25, 1e-6,
                "frozen environment ignores automatic time");

    environment.adjustTime(0.1F);
    require(environment.manualOverride(),
            "manual adjustment enables override");
    environment.setAutomaticTime(0.75F);
    requireNear(environment.timeOfDay(), 0.35, 1e-6,
                "manual override ignores automatic time");

    environment.useAutomaticTime();
    environment.setAutomaticTime(0.75F);
    requireNear(environment.timeOfDay(), 0.75, 1e-6,
                "automatic mode resumes after debug override");
}

void assetProfilesLoad() {
    const auto loaded =
        ian::loadEnvironmentProfiles("assets/data/environment.json");
    require(loaded.valid(), "environment profile JSON loads from runtime path");
}

} // namespace

void runEnvironmentSystemTests() {
    automaticTimeUsesProfileAnchors();
    interpolationIsSmoothAndCyclic();
    debugOverridesControlAutomaticTime();
    assetProfilesLoad();
}
