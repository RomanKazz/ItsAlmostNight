#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ian {

class TerrainHeightfield;
class CollisionWorld;

struct CoinPickup {
    std::uint64_t id{};
    Vec3 position{};
    Vec3 velocity{};
    double age{};
    double magnetTime{};
    double spinPhase{};
    int value{1};
    bool magnetized{};
};

struct CoinCollection {
    int value{};
    int count{};
    Vec3 position{};
};

class CoinPickupSystem {
  public:
    static constexpr double AttractionRadius = 5.0;
    static constexpr double CollectionRadius = 0.62;
    static constexpr std::size_t MaximumPickups = 768;

    void reset();
    void spawn(Vec3 position, int amount, std::uint64_t seed,
               const TerrainHeightfield& terrain);
    [[nodiscard]] CoinCollection tick(
        double deltaSeconds, Vec3 playerPosition,
        const TerrainHeightfield& terrain,
        const CollisionWorld& collisionWorld);
    [[nodiscard]] std::span<const CoinPickup> pickups() const;

  private:
    std::vector<CoinPickup> pickups_;
    std::uint64_t nextId_{1};
};

} // namespace ian
