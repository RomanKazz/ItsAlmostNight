#include "world/SpatialHash.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

SpatialHash::SpatialHash()
    : bucketHeads_(BucketCount, -1), entries_(MaxEntries) {
    clear();
}

void SpatialHash::clear() {
    std::fill(bucketHeads_.begin(), bucketHeads_.end(), -1);
    entryCount_ = 0;
}

void SpatialHash::insert(EntityId id, Vec3 position) {
    const int x = cellCoordinate(position.x);
    const int z = cellCoordinate(position.z);
    if (!contains(x, z) || entryCount_ >= MaxEntries) {
        return;
    }
    const std::size_t bucketIndex = indexOf(x, z);
    entries_[entryCount_] = {
        .entry = {id, position},
        .next = bucketHeads_[bucketIndex],
    };
    bucketHeads_[bucketIndex] = static_cast<int>(entryCount_);
    ++entryCount_;
}

std::size_t SpatialHash::entryCount() const {
    return entryCount_;
}

int SpatialHash::cellCoordinate(double coordinate) {
    if (!std::isfinite(coordinate)) {
        return coordinate < 0.0 ? -1 : GridSize;
    }
    if (coordinate < MinimumCoordinate) {
        return -1;
    }
    if (coordinate >=
        MinimumCoordinate + CellSize * static_cast<double>(GridSize)) {
        return GridSize;
    }
    return static_cast<int>(std::floor((coordinate - MinimumCoordinate) / CellSize));
}

bool SpatialHash::contains(int x, int z) {
    return x >= 0 && z >= 0 && x < GridSize && z < GridSize;
}

std::size_t SpatialHash::indexOf(int x, int z) {
    return static_cast<std::size_t>(z) * static_cast<std::size_t>(GridSize) +
           static_cast<std::size_t>(x);
}

} // namespace ian
