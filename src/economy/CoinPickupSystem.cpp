#include "economy/CoinPickupSystem.hpp"

#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double Gravity = 18.0;
// Fitted coin plus its expanded outline has a vertical radius near 0.23.
// Keep a little slope clearance so the shell never cuts into terrain.
constexpr double GroundOffset = 0.30;
constexpr double MagnetDelay = 0.16;
constexpr double MaximumLifetime = 50.0;

std::uint64_t mixBits(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

double unitRandom(std::uint64_t seed) {
    return static_cast<double>(mixBits(seed) >> 11U) *
           (1.0 / 9007199254740992.0);
}

double length(Vec3 value) {
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

void CoinPickupSystem::reset() {
    pickups_.clear();
    nextId_ = 1;
}

void CoinPickupSystem::spawn(
    Vec3 position, int amount, std::uint64_t seed,
    const TerrainHeightfield& terrain) {
    if (amount <= 0) {
        return;
    }
    const double ground = terrain.getHeight(position.x, position.z);
    position.y = std::max(position.y + 0.48, ground + GroundOffset + 0.32);
    for (int index = 0;
         index < amount && pickups_.size() < MaximumPickups;
         ++index) {
        const std::uint64_t coinSeed =
            mixBits(seed + static_cast<std::uint64_t>(index) *
                               0x9e3779b97f4a7c15ULL);
        const double angle = unitRandom(coinSeed) * Pi * 2.0;
        const double horizontalSpeed =
            1.35 + unitRandom(coinSeed ^ 0x5f356495ULL) * 2.15;
        pickups_.push_back({
            .id = nextId_++,
            .position = position,
            .velocity = {
                std::cos(angle) * horizontalSpeed,
                3.8 + unitRandom(coinSeed ^ 0xa13fc965ULL) * 2.2,
                std::sin(angle) * horizontalSpeed,
            },
            .age = 0.0,
            .magnetTime = 0.0,
            .spinPhase = unitRandom(coinSeed ^ 0xc2b2ae35ULL) * Pi * 2.0,
            .value = 1,
            .magnetized = false,
        });
    }
}

CoinCollection CoinPickupSystem::tick(
    double deltaSeconds, Vec3 playerPosition,
    const TerrainHeightfield& terrain) {
    CoinCollection collected{};
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return collected;
    }
    deltaSeconds = std::min(deltaSeconds, 0.1);
    const Vec3 target{
        playerPosition.x,
        playerPosition.y - 0.72,
        playerPosition.z,
    };
    const double attractionSquared = AttractionRadius * AttractionRadius;
    const double collectionSquared = CollectionRadius * CollectionRadius;

    for (CoinPickup& coin : pickups_) {
        coin.age += deltaSeconds;
        Vec3 toPlayer{
            target.x - coin.position.x,
            target.y - coin.position.y,
            target.z - coin.position.z,
        };
        const double distanceSquared =
            toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y +
            toPlayer.z * toPlayer.z;
        if (!coin.magnetized && coin.age >= MagnetDelay &&
            distanceSquared <= attractionSquared) {
            coin.magnetized = true;
            coin.magnetTime = 0.0;
        }

        if (coin.magnetized) {
            coin.magnetTime += deltaSeconds;
            const double distance = std::max(length(toPlayer), 0.0001);
            const double accelerationEase = std::clamp(
                coin.magnetTime / 0.24, 0.0, 1.0);
            const double speed =
                7.0 + accelerationEase * 18.0 +
                std::max(0.0, AttractionRadius - distance) * 1.1;
            const double swirl =
                std::sin(coin.spinPhase + coin.age * 13.0) *
                (1.0 - accelerationEase) * 1.4;
            coin.velocity = {
                toPlayer.x / distance * speed -
                    toPlayer.z / distance * swirl,
                toPlayer.y / distance * speed,
                toPlayer.z / distance * speed +
                    toPlayer.x / distance * swirl,
            };
            coin.position.x += coin.velocity.x * deltaSeconds;
            coin.position.y += coin.velocity.y * deltaSeconds;
            coin.position.z += coin.velocity.z * deltaSeconds;
        } else {
            coin.velocity.y -= Gravity * deltaSeconds;
            coin.position.x += coin.velocity.x * deltaSeconds;
            coin.position.y += coin.velocity.y * deltaSeconds;
            coin.position.z += coin.velocity.z * deltaSeconds;
            const double ground = terrain.getHeight(
                coin.position.x, coin.position.z) + GroundOffset;
            if (coin.position.y < ground) {
                coin.position.y = ground;
                if (coin.velocity.y < -0.45) {
                    coin.velocity.y = -coin.velocity.y * 0.43;
                } else {
                    coin.velocity.y = 0.0;
                }
                const double drag = std::pow(0.10, deltaSeconds);
                coin.velocity.x *= drag;
                coin.velocity.z *= drag;
            }
        }
    }

    std::erase_if(pickups_, [&](const CoinPickup& coin) {
        const double deltaX = target.x - coin.position.x;
        const double deltaY = target.y - coin.position.y;
        const double deltaZ = target.z - coin.position.z;
        const bool reachedPlayer = coin.magnetized &&
            deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ <=
                collectionSquared;
        if (reachedPlayer) {
            collected.value += coin.value;
            ++collected.count;
            collected.position = coin.position;
        }
        return reachedPlayer || coin.age >= MaximumLifetime;
    });
    return collected;
}

std::span<const CoinPickup> CoinPickupSystem::pickups() const {
    return pickups_;
}

} // namespace ian
