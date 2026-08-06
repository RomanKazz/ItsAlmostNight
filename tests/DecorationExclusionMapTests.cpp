#include "TestHarness.hpp"
#include "graphics/DecorationExclusionMap.hpp"

#include <array>

void runDecorationExclusionMapTests() {
    ian::DecorationExclusionMap map;
    constexpr std::array exclusions{
        ian::DecorationExclusion{
            .shape = ian::DecorationExclusionShape::Circle,
            .centerX = 0.0,
            .centerZ = 0.0,
            .radius = 1.0,
        },
        ian::DecorationExclusion{
            .shape = ian::DecorationExclusionShape::Rectangle,
            .centerX = 5.0,
            .centerZ = -2.0,
            .halfWidth = 1.0,
            .halfDepth = 2.0,
        },
    };
    map.rebuild(12.0, exclusions, 1.0, 0.5);
    require(map.cellCount() == 48U * 48U,
            "decoration occupancy allocates one compact fixed world grid");
    require(map.blocked(0.0, 0.0) && map.blocked(1.8, 0.0),
            "circle exclusion includes decorative footprint padding");
    require(map.blocked(5.0, -3.8) && !map.blocked(8.0, -2.0),
            "rectangle exclusion is rasterized without blocking distant cells");
    require(map.blocked(12.1, 0.0),
            "decorations outside the precomputed world grid are rejected");
    require(map.blockedCellCount() > 0U &&
                map.blockedCellCount() < map.cellCount(),
            "occupancy remains sparse instead of disabling all decoration");

    const std::array duplicateCircles{
        exclusions.front(), exclusions.front()};
    ian::DecorationExclusionMap duplicateMap;
    duplicateMap.rebuild(12.0, duplicateCircles, 1.0, 0.5);
    ian::DecorationExclusionMap singleMap;
    singleMap.rebuild(
        12.0, std::span<const ian::DecorationExclusion>{
                  duplicateCircles.data(), 1U},
        1.0, 0.5);
    require(duplicateMap.blockedCellCount() ==
                singleMap.blockedCellCount(),
            "overlapping blockers do not inflate occupancy work or storage");

    const std::array relocated{
        ian::DecorationExclusion{
            .shape = ian::DecorationExclusionShape::Circle,
            .centerX = -6.0,
            .centerZ = 4.0,
            .radius = 1.0,
        },
    };
    singleMap.rebuild(12.0, relocated, 1.0, 0.5);
    require(!singleMap.blocked(0.0, 0.0) &&
                singleMap.blocked(-6.0, 4.0),
            "rare resource relocation replaces stale occupancy cells");
}
