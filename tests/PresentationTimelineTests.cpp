#include "TestHarness.hpp"
#include "graphics/AtmosphereParticles.hpp"
#include "app/RestartHold.hpp"
#include "presentation/PresentationTypes.hpp"
#include "presentation/PresentationTimeline.hpp"
#include "presentation/PlayerArrival.hpp"
#include "ui/MenuInputBarrier.hpp"

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
    {
        ian::atmosphere::Field field;
        field.configure(421, 384);
        const Vector3 center{120, 2, -90};
        const auto first = field.leaf(0, 15, center);
        field.configure(421, 384);
        const auto same = field.leaf(0, 15, center);
        requireNear(first.position.x, same.position.x, 1e-6, "unchanged worlds retain deterministic particle seeds");
        for (const float time : {0.0F, 0.016F, 15.0F, 6000.0F}) {
            for (std::size_t i = 0; i < ian::atmosphere::LeafCount + ian::atmosphere::DustCount; ++i) {
                const bool leaf = i < ian::atmosphere::LeafCount;
                const auto p = leaf ? field.leaf(i, time, center)
                    : field.dust(i - ian::atmosphere::LeafCount, time, center);
                const float range = leaf ? ian::atmosphere::LeafRange : ian::atmosphere::DustRange;
                require(std::isfinite(p.position.y) && p.position.y > 0 &&
                        std::abs(p.position.x - center.x) <= range + 1e-4F &&
                        std::abs(p.position.z - center.z) <= range + 1e-4F,
                        "particle field stays bounded around distant cameras during long runs");
                require(p.opacity >= 0 && p.opacity <= 1, "particle opacity stays bounded");
                const float dx = p.position.x - center.x, dz = p.position.z - center.z;
                if (dx * dx + dz * dz >= range * range) {
                    requireNear(p.opacity, 0, 1e-6, "particles disappear before their wrap boundary");
                }
            }
        }
        const auto nearby = field.leaf(0, 15, {center.x + .01F, 2, center.z});
        requireNear(first.position.x, nearby.position.x, 1e-4, "camera motion does not drag visible leaves through the world");
        field.configure(842, 384);
        require(std::abs(first.position.x - field.leaf(0, 15, center).position.x) > 1e-3F,
                "new world seeds change atmospheric particle placements");
        const auto biomes = ian::visual_biomes::generate(842, 384);
        for (std::size_t i = 0; i < ian::visual_biomes::Count; ++i) {
            ian::atmosphere::Particle p;
            p.position = {biomes.centers[i][0], 1, biomes.centers[i][1]};
            p.opacity = .6F;
            const Color color = field.leafColor(p);
            const Color expected = ian::atmosphere::tint(
                ian::visual_biomes::shiftPalette(biomes.leaves[i], 0, .05F, .10F), 1, .6F);
            require(color.r == expected.r && color.g == expected.g && color.b == expected.b,
                    "leaves use the discrete palette of their world-space biome");
            require(color.a == expected.a && field.dustColor(p).a == expected.a,
                    "biome tint preserves leaf and dust lifetime opacity");
        }
        ian::atmosphere::Particle p;
        p.size = 1; p.opacity = 1;
        const auto vertices = ian::atmosphere::leafVertices(p, {80, 160, 80, 255});
        require(vertices.size() == 72, "leaf geometry has a fixed vertex budget including vein and stem");
        for (std::size_t i = 0; i < 48; i += 6) {
            const auto normal = [&](std::size_t offset) {
                return Vector3CrossProduct(Vector3Subtract(vertices[offset+1].position, vertices[offset].position),
                    Vector3Subtract(vertices[offset+2].position, vertices[offset].position));
            };
            require(normal(i).z > 0 && normal(i + 3).z < 0,
                    "folded leaf faces keep correct opposite winding while tumbling");
        }
        const auto dust = ian::atmosphere::dustVertices(p, {200, 180, 160, 120}, {1,0,0}, {0,1,0});
        for (std::size_t i = 0; i < dust.size(); i += 3) {
            require(dust[i].color.a == 120 && dust[i+1].color.a == 0 && dust[i+2].color.a == 0,
                    "dust discs fade to transparent edges without a square sprite silhouette");
        }
    }

    for (const int fps : {30, 60, 144, 240}) {
        ian::RestartHold restart;
        const double dt = 1.0 / fps;
        for (int frame = 0; frame < fps - 1; ++frame) {
            require(!restart.update(true, true, dt), "restart waits for a full second at every frame rate");
        }
        require(restart.progress() > 0.9, "restart indicator approaches full before activation");
        require(restart.update(true, true, dt), "one-second hold restarts the run");
        for (int repeat = 0; repeat < 3; ++repeat) {
            restart.resetProgress(); // The new run resets client presentation.
            for (int frame = 0; frame < fps - 1; ++frame) {
                require(!restart.update(true, true, dt), "repeated restarts still wait a full second");
            }
            require(restart.update(true, true, dt), "held R restarts successive worlds without release");
        }
        requireNear(restart.progress(), 0.0, 1e-9, "completed hold hides its indicator");
        require(!restart.update(false, true, dt), "release stops repeated restarts");
        require(!restart.update(true, true, 0.1), "a fresh hold starts from zero");
        require(!restart.update(false, true, dt), "early release cancels a partial hold");
        requireNear(restart.progress(), 0.0, 1e-9, "cancelled hold clears its indicator");
        require(!restart.update(true, false, dt), "settings and upgrade menus do not restart");
        restart.resetProgress();
        for (int frame = 0; frame < fps * 2; ++frame) {
            require(!restart.update(true, true, dt), "a key held in a menu must be released before restarting");
        }
        restart.cancel(false);
        require(!restart.update(true, true, 5.0), "a stalled frame cannot instantly complete a hold");
        requireNear(restart.progress(), 0.1, 1e-9, "frame stalls contribute at most a tenth of a second");
        restart.cancel(true);
        require(!restart.update(true, true, dt), "button restart cancels an ongoing keyboard hold");
    }
    {
        ian::MenuInputBarrier barrier;
        require(barrier.allowsInput(false), "initial main menu accepts input immediately");
        barrier.blockUntilRelease();
        for (int frame = 0; frame < 120; ++frame) {
            require(!barrier.allowsInput(true),
                "holding Save and Main Menu never activates Play in the next screen");
        }
        require(!barrier.allowsInput(false),
            "release of Save and Main Menu is consumed instead of opening class selection");
        require(barrier.allowsInput(true) && barrier.allowsInput(false),
            "a fresh click or keyboard activation works after returning to the menu");
        barrier.blockUntilRelease();
        require(!barrier.allowsInput(false),
            "a quick click released before the transition is also consumed");
        require(barrier.allowsInput(false),
            "the menu rearms on the next frame without a delay or an extra click");
    }
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
    for (const double damage : {.01, .45, 1.12, 2.0, 4.0, 8.0, 16.0, 1e12,
             0.0, -1.0, std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::quiet_NaN()}) {
        const auto appearance = ian::presentation::bulletAppearance(damage);
        requireNear(appearance.scale, 1.0, 1e-6, "damage never changes projectile size");
        require(appearance.color.r == basicBullet.color.r && appearance.color.g == basicBullet.color.g &&
            appearance.color.b == basicBullet.color.b, "damage never recolors projectiles");
    }
    const auto iron = ian::presentation::CannonballAppearance.color;
    require(iron.r < 150 && iron.g < 150 && iron.b < 150 && iron.r < iron.b,
        "ballistic shells keep a neutral cool iron palette");
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

    const double visibleStart = ian::presentation::insightFraction(25.0, 40.0);
    const double nextLevelFill = ian::presentation::insightFraction(15.0, 65.0);
    requireNear(ian::presentation::levelUpInsightFraction(0.0, visibleStart, nextLevelFill),
                visibleStart, 1e-9, "level-up begins at the visible bar without a jump");
    double previousFill = visibleStart;
    for (int step = 1; step <= 35; ++step) {
        const double fill = ian::presentation::levelUpInsightFraction(
            static_cast<double>(step)/100.0, visibleStart, nextLevelFill);
        require(fill >= previousFill && fill <= 1.0,
                "changing XP requirements cannot shrink the fill-to-full phase");
        previousFill = fill;
    }
    requireNear(previousFill, 1.0, 1e-9,
                "40-to-65 XP transition must reach a completely full bar");
    requireNear(ian::presentation::levelUpInsightFraction(0.5, visibleStart, nextLevelFill),
                1.0, 1e-9, "level-up holds the full bar before resetting");
    requireNear(ian::presentation::levelUpInsightFraction(0.55, visibleStart, nextLevelFill),
                0.0, 1e-9, "the new level begins with an empty bar");
    requireNear(ian::presentation::levelUpInsightFraction(1.0, visibleStart, nextLevelFill),
                15.0/65.0, 1e-9, "overflow uses the new level's requirement");
    const double updatedFill = ian::presentation::insightFraction(30.0, 65.0);
    requireNear(ian::presentation::levelUpInsightFraction(1.0, visibleStart, updatedFill),
                30.0/65.0, 1e-9, "XP received during animation is included in the final fill");
    requireNear(ian::presentation::levelUpInsightFraction(1.0, visibleStart,
                    ian::presentation::insightFraction(50.0, 125.0)),
                0.4, 1e-9, "a multiple-level grant displays the final level's remainder");
    requireNear(ian::presentation::levelUpInsightFraction(
                    ian::presentation::timelineProgress(0.0, 0.48), visibleStart, updatedFill),
                updatedFill, 1e-9, "a frame past animation completion lands on the target");

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
