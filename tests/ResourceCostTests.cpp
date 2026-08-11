#include "TestHarness.hpp"
#include "economy/ResourceCost.hpp"

#include <limits>

void runResourceCostTests() {
    using namespace ian;

    require(
        addResourceCosts({2, 3, 4}, {5, 6, 7}) ==
            ResourceCost{7, 9, 11},
        "resource costs add component-wise");
    require(
        addResourceCosts(
            {std::numeric_limits<int>::max(), 0, 0},
            {1, 0, 0}).wood == std::numeric_limits<int>::max(),
        "resource cost addition saturates instead of overflowing");
    require(canAfford({2, 3, 4}, 2, 3, 4),
            "wallet can afford an exact resource cost");
    require(!canAfford({2, 3, 4}, 2, 2, 100),
            "one missing resource rejects the complete cost");
}
