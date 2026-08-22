#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace ian {

class TerrainHeightfield;
class CollisionWorld;

enum class CoinType : std::uint8_t { Bronze, Silver, HighValue };
enum class PickupKind : std::uint8_t { Coin, Heart };

[[nodiscard]] constexpr int coinValue(CoinType type) {
    switch (type) {
    case CoinType::Bronze: return 1;
    case CoinType::Silver: return 5;
    case CoinType::HighValue: return 10;
    }
    return 1;
}

struct CoinPickup {
    std::uint64_t id{};
    Vec3 position{};
    Vec3 velocity{};
    double age{};
    double magnetTime{};
    double spinPhase{};
    CoinType type{CoinType::Bronze};
    PickupKind kind{PickupKind::Coin};
    int value{1};
    bool magnetized{};
};

struct CoinCollection {
    int value{};
    int count{};
    double healing{};
    int heartCount{};
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
    void spawnValue(Vec3 position, int value, std::uint64_t seed,
                    const TerrainHeightfield& terrain,
                    double burstSpread = 1.0);
    void spawnHeart(Vec3 position, std::uint64_t seed,
                    const TerrainHeightfield& terrain,
                    double burstSpread = 1.0);
    [[nodiscard]] CoinCollection tick(
        double deltaSeconds, Vec3 playerPosition,
        const TerrainHeightfield& terrain,
        const CollisionWorld& collisionWorld,
        double missingHealth = 0.0,
        double attractionRadius = AttractionRadius);
    [[nodiscard]] std::span<const CoinPickup> pickups() const;

  private:
    std::vector<CoinPickup> pickups_;
    std::uint64_t nextId_{1};
};

} // namespace ian
