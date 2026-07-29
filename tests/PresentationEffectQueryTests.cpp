#include "TestHarness.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <array>

void runPresentationEffectQueryTests() {
    const ian::EntityId tree{10U, 1U};
    const ian::EntityId otherTree{11U, 1U};
    const std::array<ian::PresentationEffect, 2> effects{{
        {
            .type = ian::PresentationEffectType::Hit,
            .entityId = std::nullopt,
            .position = {0.1, 0.0, 0.0},
            .remaining = 0.2,
            .duration = 0.3,
        },
        {
            .type =
                ian::PresentationEffectType::
                    ResourceHitWood,
            .entityId = tree,
            .position = {0.1, 0.0, 0.0},
            .remaining = 0.3,
            .duration = 0.46,
        },
    }};

    require(
        ian::presentation::resourceHitFlash(
            effects, tree) > 0.0F &&
            ian::presentation::resourceHitScale(
                effects, tree) < 1.0F,
        "targeted resource effect drives hit response");
    require(
        ian::presentation::resourceHitFlash(
            effects, otherTree) == 0.0F &&
            ian::presentation::resourceHitScale(
                effects, otherTree) == 1.0F,
        "nearby generic hit cannot animate another resource");
    const ian::Vec3 offset =
        ian::presentation::resourceHitOffset(
            effects, tree, {0.0, 0.0, 0.0});
    require(
        offset.x < 0.0 && offset.z == 0.0,
        "resource offset follows its own impact direction");
}
