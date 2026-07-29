#include "TestHarness.hpp"
#include "core/DeterministicRandom.hpp"

void runDeterministicRandomTests() {
    constexpr double first = ian::unitRandom(42U);
    constexpr double repeated = ian::unitRandom(42U);
    constexpr double second = ian::unitRandom(43U);
    static_assert(first == repeated);
    require(
        first == repeated,
        "deterministic random repeats for identical seed");
    require(
        first >= 0.0 && first < 1.0 &&
            second >= 0.0 && second < 1.0,
        "deterministic random stays in unit interval");
    require(
        first != second,
        "deterministic random separates adjacent seeds");
}
