#include "TestHarness.hpp"
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
}
