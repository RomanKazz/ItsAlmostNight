#include "TestHarness.hpp"
#include "core/SaturatingArithmetic.hpp"

#include <limits>

void runSaturatingArithmeticTests() {
    constexpr int Maximum = std::numeric_limits<int>::max();
    constexpr int Minimum = std::numeric_limits<int>::min();

    require(ian::saturatingAdd(20, 22) == 42,
            "saturating add preserves ordinary sums");
    require(ian::saturatingAdd(Maximum - 1, 2) == Maximum,
            "saturating add clamps positive overflow");
    require(ian::saturatingAdd(Minimum + 1, -2) == Minimum,
            "saturating add clamps negative overflow");
    require(ian::saturatingMultiplyNonNegative(7, 6) == 42,
            "saturating multiply preserves ordinary products");
    require(ian::saturatingMultiplyNonNegative(Maximum, 2) == Maximum,
            "saturating multiply clamps positive overflow");
}
