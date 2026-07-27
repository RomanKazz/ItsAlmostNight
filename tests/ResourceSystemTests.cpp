#include "TestHarness.hpp"
#include "resources/ResourceSystem.hpp"

void runResourceSystemTests() {
    ian::ResourceSystem resources;
    const ian::Vec3 origin{0.0, 1.7, 6.0};
    const ian::Vec3 direction{0.0, 0.0, -1.0};

    const auto target = resources.raycast(origin, direction, 4.0);
    require(target.has_value(), "raycast finds resource inside pickaxe range");

    const auto firstHit = resources.damage(*target, 1.0);
    require(firstHit.has_value() && !firstHit->collected, "partial damage keeps node active");
    resources.damage(*target, 1.0);
    const auto finalHit = resources.damage(*target, 1.0);
    require(finalHit.has_value() && finalHit->collected, "lethal damage collects node");
    require(finalHit->amount == 15, "tree grants configured wood yield");
    require(!resources.raycast(origin, direction, 4.0), "collected node leaves raycast");

    resources.tick(12.0);
    require(resources.raycast(origin, direction, 4.0).has_value(), "node respawns after timer");
}
