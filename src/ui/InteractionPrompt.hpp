#pragma once

#include "app/UserSettings.hpp"
#include "core/Types.hpp"
#include "economy/ResourceCost.hpp"

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ian {

class GameUi;

enum class InteractionPromptTargetKind : std::uint8_t {
    Resource,
    Chest,
    Loot,
    Building,
    ModularBuilding,
    Enemy,
    Challenge,
    Landmark,
};

enum class InteractionState : std::uint8_t {
    Available,
    Warning,
    Unavailable,
    Success,
};

struct InteractionPrompt {
    InteractionPromptTargetKind targetKind{
        InteractionPromptTargetKind::Resource};
    EntityId targetId{};
    Vector3 worldAnchor{};
    std::string objectName;
    std::string actionText;
    ControlAction input{ControlAction::Interact};
    InteractionState state{InteractionState::Available};
    std::optional<ResourceCost> cost;
    std::optional<int> availableCurrency;
    std::optional<std::string> hint;
    float progress{};
    bool showProgress{};
    bool recentFailure{};
    bool recentSuccess{};
    Color accentColor{245, 231, 198, 255};
    bool occluded{};
};

class InteractionPromptRenderer {
  public:
    void draw(const std::optional<InteractionPrompt>& prompt,
              const Camera3D& camera, const GameUi& ui,
              const ControlSettings& controls);
    void reset();

  private:
    struct TargetKey {
        InteractionPromptTargetKind kind;
        EntityId id;

        bool operator==(const TargetKey&) const = default;
    };

    [[nodiscard]] static TargetKey keyFor(
        const InteractionPrompt& prompt);
    [[nodiscard]] static bool projectable(
        const InteractionPrompt& prompt,
        const Camera3D& camera);
    void activate(const InteractionPrompt& prompt);
    void updateAnimation(float deltaSeconds,
                         bool wantsVisible,
                         const InteractionPrompt* prompt);
    void drawPrompt(const InteractionPrompt& prompt,
                    const Camera3D& camera, const GameUi& ui,
                    const ControlSettings& controls,
                    float opacity, float scale,
                    float liftPixels);

    std::optional<TargetKey> activeTarget_;
    std::optional<TargetKey> candidateTarget_;
    std::optional<InteractionPrompt> activePrompt_;
    double candidateSeconds_{};
    double missingSeconds_{};
    float opacity_{};
    float scale_{0.96F};
    float liftPixels_{5.0F};
    float progress_{}, progressOpacity_{};
    float failureRemaining_{};
    float successRemaining_{};
    bool failureSignalActive_{};
};

} // namespace ian
