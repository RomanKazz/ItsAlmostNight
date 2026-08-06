#include "TestHarness.hpp"
#include "world/SpatialHash.hpp"

#include <cstddef>
#include <limits>

void runSpatialHashTests() {
    ian::SpatialHash hash;
    hash.insert({1, 1}, {0.0, 0.0, 0.0});
    hash.insert({2, 1}, {1.9, 0.0, 0.0});
    hash.insert({3, 1}, {8.0, 0.0, 8.0});
    hash.insert({6, 1}, {150.0, 0.0, -150.0});
    require(hash.entryCount() == 4, "spatial hash tracks inserted entries");

    std::size_t farMapCount = 0;
    hash.forEachNearby(
        {150.0, 0.0, -150.0}, 1.0,
        [&farMapCount](const ian::SpatialEntry&) { ++farMapCount; });
    require(farMapCount == 1,
            "spatial hash covers the full terrain extent");

    std::size_t nearbyCount = 0;
    hash.forEachNearby({0.0, 0.0, 0.0}, 2.0,
                       [&nearbyCount](const ian::SpatialEntry&) { ++nearbyCount; });
    require(nearbyCount == 2, "spatial query crosses bucket boundary");

    std::size_t distantCount = 0;
    hash.forEachNearby({0.0, 0.0, 0.0}, 1.0,
                       [&distantCount](const ian::SpatialEntry&) { ++distantCount; });
    require(distantCount == 1, "spatial query filters by exact radius");

    hash.insert(
        {4, 1},
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    hash.insert(
        {5, 1},
        {std::numeric_limits<double>::infinity(), 0.0, 0.0});
    require(hash.entryCount() == 4,
            "spatial hash rejects non-finite positions");

    std::size_t unboundedCount = 0;
    hash.forEachNearby(
        {0.0, 0.0, 0.0},
        std::numeric_limits<double>::infinity(),
        [&unboundedCount](const ian::SpatialEntry&) {
            ++unboundedCount;
        });
    require(unboundedCount == 4,
            "unbounded spatial query remains defined");

    hash.clear();
    require(hash.entryCount() == 0, "clear removes active entries");
}
