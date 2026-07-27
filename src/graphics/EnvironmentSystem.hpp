#pragma once

#include <raylib.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

enum class EnvironmentProfileId {
    Dawn,
    Day,
    Dusk,
    Night,
};

struct EnvironmentProfile {
    Color skyTop;
    Color skyHorizon;
    Color lowerSky;
    Color fogColor;

    Vector3 celestialDirection;
    Vector3 celestialColor;
    Vector3 sunColor;
    Vector3 skyAmbientColor;
    Vector3 groundAmbientColor;
    Vector3 dayNightTint;

    float sunIntensity;
    float ambientIntensity;
    float fogStart;
    float fogEnd;
    float exposure;
    float saturation;
    float nightFactor;
};

struct EnvironmentState : EnvironmentProfile {
    float timeOfDay{};
};

struct EnvironmentLoadResult {
    std::array<EnvironmentProfile, 4> profiles;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const { return errors.empty(); }
};

class EnvironmentSystem {
  public:
    explicit EnvironmentSystem(
        std::array<EnvironmentProfile, 4> profiles = defaults());

    static std::array<EnvironmentProfile, 4> defaults();

    void setAutomaticTime(float normalizedTime);
    void toggleFrozen();
    void adjustTime(float amount);
    void cycleProfile();
    void useAutomaticTime();

    [[nodiscard]] EnvironmentState state() const;
    [[nodiscard]] float timeOfDay() const;
    [[nodiscard]] bool frozen() const;
    [[nodiscard]] bool manualOverride() const;
    [[nodiscard]] const char* nearestProfileName() const;

  private:
    std::array<EnvironmentProfile, 4> profiles_;
    float timeOfDay_{0.25F};
    bool frozen_{};
    bool manualOverride_{};
    int debugProfileIndex_{1};
};

[[nodiscard]] EnvironmentLoadResult
loadEnvironmentProfiles(std::string_view path);

} // namespace ian
