#include "TestHarness.hpp"
#include "presentation/PresentationTypes.hpp"
#include "presentation/PresentationTimeline.hpp"
#include "presentation/PlayerArrival.hpp"

#include <limits>
#include <optional>
#include <vector>

namespace {

struct TimedVisual {
    int id{};
    double remaining{};
};

} // namespace

void runPresentationTimelineTests() {
    ian::presentation::PlayerArrival arrival;
    require(!arrival.active(), "arrival is inactive when loading an existing run");
    requireNear(arrival.mountainGrowth(), 1.0, 1e-9, "existing runs show full-size mountains");
    arrival.start();
    requireNear(arrival.mountainGrowth(), 0.0, 1e-9, "new world's mountains begin below the horizon");
    float previousGrowth = 0.0F;
    require(arrival.active(), "new run hides HUD and locks actions during arrival");
    requireNear(arrival.hudOpacity(), 0.0, 1e-9, "arrival begins with HUD hidden");
    double previousHeight = arrival.cameraMotion().height;
    int impacts = 0;
    for (int frame = 0; frame < 240; ++frame) {
        impacts += arrival.advance(1.0 / 120.0) ? 1 : 0;
        const float growth = arrival.mountainGrowth();
        require(growth >= previousGrowth && growth <= 1.0F,
                "mountains rise monotonically without overshooting");
        previousGrowth = growth;
        const auto camera = arrival.cameraMotion();
        require(camera.height >= 0.0 && camera.height <= previousHeight,
                "arrival falls monotonically without sinking below its spawn");
        require(std::isfinite(camera.pitch) && std::isfinite(camera.roll),
                "arrival camera stays finite throughout landing");
        previousHeight = camera.height;
    }
    require(impacts == 1 && !arrival.active(),
            "landing emits clouds once and always releases HUD and input");
    requireNear(arrival.cameraMotion().compression, 0.0, 1e-9,
                "landing restores camera height after its crouch");
    requireNear(arrival.cameraMotion().pitch, 0.0, 1e-9,
                "landing restores the player's aim");
    requireNear(arrival.hudOpacity(), 1.0, 1e-9, "completed arrival restores full HUD opacity");
    arrival.start();
    static_cast<void>(arrival.advance(ian::presentation::PlayerArrival::MotionSeconds));
    requireNear(arrival.hudOpacity(), 0.0, 1e-6,
                "HUD remains hidden until the camera finishes settling");
    requireNear(arrival.mountainGrowth(), 1.0, 1e-9,
                "mountains reach full height before HUD becomes visible");
    static_cast<void>(arrival.advance(ian::presentation::PlayerArrival::HudRevealSeconds * 0.5));
    requireNear(arrival.hudOpacity(), 0.5, 1e-6,
                "HUD passes through half opacity instead of popping in");
    requireNear(arrival.cameraMotion().pitch, 0.0, 1e-9,
                "HUD fade does not restart the landing camera animation");
    arrival.start();
    static_cast<void>(arrival.advance(ian::presentation::PlayerArrival::LowerMountainSeconds * .5));
    requireNear(arrival.lowerMountainGrowth(), .5, 1e-6, "lower mountains rise first");
    requireNear(arrival.mountainGrowth(), 0, 1e-9, "upper mountains wait for the lower layer");
    static_cast<void>(arrival.advance(ian::presentation::PlayerArrival::LowerMountainSeconds * .5));
    requireNear(arrival.lowerMountainGrowth(), 1, 1e-9, "lower mountains finish before the upper layer begins");
    requireNear(arrival.mountainGrowth(), 0, 1e-9, "upper layer does not overlap the first growth stage");
    static_cast<void>(arrival.advance((ian::presentation::PlayerArrival::MotionSeconds -
        ian::presentation::PlayerArrival::LowerMountainSeconds) * .5));
    requireNear(arrival.lowerMountainGrowth(), 1, 1e-9, "lower mountains remain settled during the second stage");
    requireNear(arrival.mountainGrowth(), .5, 1e-6, "upper mountains rise after the lower layer");
    arrival.start();
    require(arrival.advance(3.0) && !arrival.advance(0.5),
            "long frames cannot miss or repeat the landing effect");
    arrival.start();
    require(!arrival.advance(0.0), "paused arrival does not emit landing clouds");
    requireNear(arrival.cameraMotion().height, 4.6, 1e-9,
                "paused arrival retains its camera position");
    arrival.reset();
    require(!arrival.active(), "restart and loading reset stale arrival state");

    const auto basicBullet = ian::presentation::bulletAppearance(1.0);
    const auto doubleBullet = ian::presentation::bulletAppearance(2.0);
    const auto heavyBullet = ian::presentation::bulletAppearance(4.0);
    const auto powerfulBullet = ian::presentation::bulletAppearance(8.0);
    const auto maximumBullet = ian::presentation::bulletAppearance(16.0);
    requireNear(basicBullet.scale, 1.0, 1e-6,
                "level-one gun bullet retains its normal size");
    require(basicBullet.scale < doubleBullet.scale &&
                doubleBullet.scale < heavyBullet.scale &&
                heavyBullet.scale < powerfulBullet.scale &&
                powerfulBullet.scale < maximumBullet.scale,
            "higher outgoing damage visibly grows bullets");
    require(basicBullet.color.g > doubleBullet.color.g &&
                doubleBullet.color.g > heavyBullet.color.g &&
                powerfulBullet.color.b > heavyBullet.color.b &&
                maximumBullet.color.g > powerfulBullet.color.g,
            "bullet palette progresses from amber through red and violet to blue");
    const auto intermediateBullet = ian::presentation::bulletAppearance(1.12);
    require(intermediateBullet.scale > basicBullet.scale &&
                intermediateBullet.scale < doubleBullet.scale &&
                intermediateBullet.color.g < basicBullet.color.g &&
                intermediateBullet.color.g > doubleBullet.color.g,
            "percentage upgrades interpolate bullet color and size smoothly");
    const auto extremeBullet = ian::presentation::bulletAppearance(1e12);
    requireNear(extremeBullet.scale, 2.4, 1e-6,
                "very high damage cannot cover the screen with an unbounded bullet");
    require(extremeBullet.color.r == maximumBullet.color.r &&
                extremeBullet.color.g == maximumBullet.color.g &&
                extremeBullet.color.b == maximumBullet.color.b,
            "extreme damage safely saturates the final palette color");
    const auto shardBullet = ian::presentation::bulletAppearance(0.45);
    require(shardBullet.scale < basicBullet.scale && shardBullet.scale >= 0.72F,
            "weaker ricochets and shards stay smaller and readable");
    for (const double invalidDamage : {0.0, -1.0,
             std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN()}) {
        requireNear(ian::presentation::bulletAppearance(invalidDamage).scale,
                    1.0, 1e-6, "invalid bullet damage keeps a finite default appearance");
    }
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
