#include "TestHarness.hpp"
#include "presentation/PresentationTypes.hpp"
#include "presentation/PresentationTimeline.hpp"

#include <optional>
#include <vector>

namespace {

struct TimedVisual {
    int id{};
    double remaining{};
};

} // namespace

void runPresentationTimelineTests() {
    requireNear(
        ian::presentation::timelineProgress(0.48, 0.48),
        0.0, 1e-9,
        "point notification waits at timeline start");
    require(
        ian::presentation::timelineProgress(0.312, 0.48) >= 0.35,
        "point notification unlocks after insight reaches requirement");
    requireNear(
        ian::presentation::timelineProgress(0.0, 0.48),
        1.0, 1e-9,
        "completed point timeline cannot lose its notification");

    require(
        ian::actionModeUsesEquipment(ian::ActionMode::Equipment) &&
            !ian::actionModeUsesEquipment(ian::ActionMode::Buildings),
        "only the unified equipment mode exposes viewmodels");

    std::vector<TimedVisual> visuals{
        {.id = 1, .remaining = 0.5},
        {.id = 2, .remaining = 0.1},
    };
    ian::presentation::advanceTimeline(visuals, 0.2);
    require(
        visuals.size() == 1U && visuals.front().id == 1,
        "presentation timeline removes expired visuals");
    requireNear(
        visuals.front().remaining, 0.3, 1e-9,
        "presentation timeline advances surviving visuals");

    std::optional<TimedVisual> optional{
        TimedVisual{.id = 3, .remaining = 0.1}};
    ian::presentation::advanceTimeline(optional, 0.2);
    require(
        !optional.has_value(),
        "presentation timeline resets expired optional visual");

    std::vector<ian::PresentationEffect> delayed{{
        .type =
            ian::PresentationEffectType::BuildingPlaced,
        .position = {},
        .remaining = 0.7,
        .duration = 0.7,
        .startDelayRemaining = 0.15,
    }};
    ian::presentation::advanceTimeline(delayed, 0.1);
    require(
        delayed.size() == 1U,
        "delayed presentation effect waits before"
        " advancing its active animation");
    requireNear(
        delayed.front().startDelayRemaining,
        0.05, 1e-9,
        "presentation timeline advances effect delay");
    requireNear(
        delayed.front().remaining, 0.7, 1e-9,
        "delayed effect animation remains paused");
    ian::presentation::advanceTimeline(delayed, 0.1);
    requireNear(
        delayed.front().remaining, 0.65, 1e-9,
        "presentation timeline applies frame time"
        " remaining after delay");

    std::vector<ian::ArrowVisual> chainedProjectile{{
        .origin = {},
        .target = {1.0, 0.0, 0.0},
        .direction = {1.0, 0.0, 0.0},
        .remaining = 0.2,
        .duration = 0.2,
        .startDelayRemaining = 0.15,
    }};
    ian::presentation::advanceTimeline(chainedProjectile, 0.1);
    requireNear(
        chainedProjectile.front().remaining, 0.2, 1e-9,
        "ricochet segment waits for the previous segment");
    ian::presentation::advanceTimeline(chainedProjectile, 0.1);
    requireNear(
        chainedProjectile.front().remaining, 0.15, 1e-9,
        "ricochet segment uses frame time left after its delay");

    std::vector<ian::FloatingDamageNumber> delayedNumber{{
        .position = {},
        .damage = 4.0,
        .remaining = 0.5,
        .duration = 0.5,
        .startDelayRemaining = 0.15,
    }};
    ian::presentation::advanceTimeline(delayedNumber, 0.1);
    requireNear(
        delayedNumber.front().remaining, 0.5, 1e-9,
        "tower damage number waits for projectile impact");
    ian::presentation::advanceTimeline(delayedNumber, 0.1);
    requireNear(
        delayedNumber.front().remaining, 0.45, 1e-9,
        "tower damage number starts when projectile arrives");
}
