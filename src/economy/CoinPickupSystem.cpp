#include "economy/CoinPickupSystem.hpp"

#include "world/CollisionWorld.hpp"
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
constexpr double CollisionRadius = 0.20;
// A reward must first burst outward and complete its readable bounce. Only
// then may the magnet pull it toward a nearby player.
constexpr double MagnetDelay = 0.85;
constexpr double MaximumLifetime = 50.0;
constexpr double HeartHealing = 25.0;

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
    for (int index = 0;
         index < amount && pickups_.size() < MaximumPickups; ++index) {
        spawnValue(
            position, 1,
            mixBits(seed + static_cast<std::uint64_t>(index) *
                               0x9e3779b97f4a7c15ULL),
            terrain);
    }
}

void CoinPickupSystem::spawnValue(
    Vec3 position, int amount, std::uint64_t seed,
    const TerrainHeightfield& terrain, double burstSpread) {
    if (amount <= 0) {
        return;
    }
    const double ground = terrain.getHeight(position.x, position.z);
    position.y = std::max(position.y + 0.48, ground + GroundOffset + 0.32);
    int remaining = amount;
    int index = 0;
    while (remaining > 0 && pickups_.size() < MaximumPickups) {
        const CoinType type = remaining >= 10
            ? CoinType::HighValue
            : remaining >= 5 ? CoinType::Silver : CoinType::Bronze;
        const int value = coinValue(type);
        const std::uint64_t coinSeed =
            mixBits(seed + static_cast<std::uint64_t>(index) *
                               0x9e3779b97f4a7c15ULL);
        const double angle = unitRandom(coinSeed) * Pi * 2.0;
        const double horizontalSpeed =
            (2.35 + unitRandom(coinSeed ^ 0x5f356495ULL) * 2.45) *
            std::max(0.0, burstSpread);
        pickups_.push_back({
            .id = nextId_++,
            .position = position,
            .velocity = {
                std::cos(angle) * horizontalSpeed,
                4.2 + unitRandom(coinSeed ^ 0xa13fc965ULL) * 2.4,
                std::sin(angle) * horizontalSpeed,
            },
            .age = 0.0,
            .magnetTime = 0.0,
            .spinPhase = unitRandom(coinSeed ^ 0xc2b2ae35ULL) * Pi * 2.0,
            .type = type,
            .value = value,
            .magnetized = false,
        });
        remaining -= value;
        ++index;
    }
}

void CoinPickupSystem::spawnHeart(
    Vec3 position, std::uint64_t seed,
    const TerrainHeightfield& terrain, double burstSpread) {
    if (pickups_.size() >= MaximumPickups) return;
    const double ground = terrain.getHeight(position.x, position.z);
    position.y = std::max(position.y + 0.48, ground + GroundOffset + 0.32);
    const double angle = unitRandom(seed) * Pi * 2.0;
    const double horizontalSpeed =
        (2.35 + unitRandom(seed ^ 0x5f356495ULL) * 2.45) *
        std::max(0.0, burstSpread);
    pickups_.push_back({
        .id = nextId_++,
        .position = position,
        .velocity = {
            std::cos(angle) * horizontalSpeed,
            4.2 + unitRandom(seed ^ 0xa13fc965ULL) * 2.4,
            std::sin(angle) * horizontalSpeed,
        },
        .spinPhase = unitRandom(seed ^ 0xc2b2ae35ULL) * Pi * 2.0,
        .kind = PickupKind::Heart,
        .value = 0,
    });
}

CoinCollection CoinPickupSystem::tick(
    double deltaSeconds, Vec3 playerPosition,
    const TerrainHeightfield& terrain,
    const CollisionWorld& collisionWorld,
    double missingHealth) {
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
    missingHealth = std::max(0.0, missingHealth);

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
        const bool canMagnetize = coin.kind == PickupKind::Coin ||
            missingHealth > 0.0;
        if (!canMagnetize && coin.magnetized) {
            coin.magnetized = false;
            coin.magnetTime = 0.0;
        }
        if (!coin.magnetized && canMagnetize && coin.age >= MagnetDelay &&
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
            const Vec3 previousPosition = coin.position;
            coin.velocity.y -= Gravity * deltaSeconds;
            coin.position.x += coin.velocity.x * deltaSeconds;
            coin.position.y += coin.velocity.y * deltaSeconds;
            coin.position.z += coin.velocity.z * deltaSeconds;
            double ground = terrain.getHeight(
                coin.position.x, coin.position.z) + GroundOffset;
            const auto surfaceLanding =
                coin.velocity.y <= 0.0
                ? collisionWorld.sweptPlayerLanding(
                      previousPosition, coin.position,
                      CollisionRadius,
                      previousPosition.y - GroundOffset,
                      coin.position.y - GroundOffset)
                : std::nullopt;
            if (surfaceLanding &&
                surfaceLanding->surfaceHeight + GroundOffset > ground) {
                coin.position.x = surfaceLanding->position.x;
                coin.position.z = surfaceLanding->position.z;
                ground = surfaceLanding->surfaceHeight + GroundOffset;
            }
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

    std::erase_if(pickups_, [&](CoinPickup& coin) {
        const double deltaX = target.x - coin.position.x;
        const double deltaY = target.y - coin.position.y;
        const double deltaZ = target.z - coin.position.z;
        bool reachedPlayer = coin.magnetized &&
            deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ <=
                collectionSquared;
        if (reachedPlayer) {
            if (coin.kind == PickupKind::Heart) {
                if (missingHealth <= 0.0) {
                    coin.magnetized = false;
                    coin.magnetTime = 0.0;
                    reachedPlayer = false;
                } else {
                    const double healing = std::min(
                        HeartHealing, missingHealth);
                    collected.healing += healing;
                    ++collected.heartCount;
                    missingHealth -= healing;
                    collected.position = coin.position;
                }
            } else {
                collected.value += coin.value;
                ++collected.count;
                collected.position = coin.position;
            }
        }
        return reachedPlayer || coin.age >= MaximumLifetime;
    });
    return collected;
}

std::span<const CoinPickup> CoinPickupSystem::pickups() const {
    return pickups_;
}

} // namespace ian
