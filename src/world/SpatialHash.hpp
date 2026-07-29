#pragma once

#include "core/Types.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace ian {

struct SpatialEntry {
    EntityId id;
    Vec3 position;
};

class SpatialHash {
  public:
    static constexpr double CellSize = 2.0;
    static constexpr double MinimumCoordinate = -48.0;
    static constexpr int GridSize = 48;
    static constexpr std::size_t MaxEntries = 4096;

    SpatialHash();

    void clear();
    void insert(EntityId id, Vec3 position);

    template <typename Visitor>
    void forEachNearby(Vec3 position, double radius, Visitor&& visitor) const {
        const int minimumX = cellCoordinate(position.x - radius);
        const int maximumX = cellCoordinate(position.x + radius);
        const int minimumZ = cellCoordinate(position.z - radius);
        const int maximumZ = cellCoordinate(position.z + radius);
        const double radiusSquared = radius * radius;

        for (int z = minimumZ; z <= maximumZ; ++z) {
            for (int x = minimumX; x <= maximumX; ++x) {
                if (!contains(x, z)) {
                    continue;
                }
                int entryIndex = bucketHeads_[indexOf(x, z)];
                while (entryIndex >= 0) {
                    const StoredEntry& stored = entries_[static_cast<std::size_t>(entryIndex)];
                    const SpatialEntry& entry = stored.entry;
                    const double deltaX = entry.position.x - position.x;
                    const double deltaZ = entry.position.z - position.z;
                    if ((deltaX * deltaX) + (deltaZ * deltaZ) <= radiusSquared) {
                        visitor(entry);
                    }
                    entryIndex = stored.next;
                }
            }
        }
    }

    [[nodiscard]] std::size_t entryCount() const;

  private:
    static constexpr std::size_t BucketCount =
        static_cast<std::size_t>(GridSize) * static_cast<std::size_t>(GridSize);

    struct StoredEntry {
        SpatialEntry entry;
        int next{-1};
    };

    [[nodiscard]] static int cellCoordinate(double coordinate);
    [[nodiscard]] static bool contains(int x, int z);
    [[nodiscard]] static std::size_t indexOf(int x, int z);

    std::array<int, BucketCount> bucketHeads_{};
    std::array<StoredEntry, MaxEntries> entries_{};
    std::size_t entryCount_{};
};

} // namespace ian
