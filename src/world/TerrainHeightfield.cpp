#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

[[nodiscard]] std::uint32_t mixBits(
    std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] double latticeValue(
    int x, int z, std::uint32_t seed) {
    const auto ux = static_cast<std::uint32_t>(x);
    const auto uz = static_cast<std::uint32_t>(z);
    const std::uint32_t hash = mixBits(
        seed ^ mixBits(ux * 0x9e3779b9U) ^
        mixBits(uz * 0x85ebca6bU));
    const double unit =
        static_cast<double>(hash) /
        static_cast<double>(
            std::numeric_limits<std::uint32_t>::max());
    return unit * 2.0 - 1.0;
}

[[nodiscard]] double smoother(double value) {
    value = std::clamp(value, 0.0, 1.0);
    return value * value * value *
           (value * (value * 6.0 - 15.0) + 10.0);
}

[[nodiscard]] double interpolate(
    double from, double to, double amount) {
    return from + (to - from) * amount;
}

[[nodiscard]] double valueNoise(
    double x, double z, std::uint32_t seed) {
    const int minimumX =
        static_cast<int>(std::floor(x));
    const int minimumZ =
        static_cast<int>(std::floor(z));
    const double localX =
        smoother(x - static_cast<double>(minimumX));
    const double localZ =
        smoother(z - static_cast<double>(minimumZ));
    const double north = interpolate(
        latticeValue(minimumX, minimumZ, seed),
        latticeValue(minimumX + 1, minimumZ, seed),
        localX);
    const double south = interpolate(
        latticeValue(minimumX, minimumZ + 1, seed),
        latticeValue(
            minimumX + 1, minimumZ + 1, seed),
        localX);
    return interpolate(north, south, localZ);
}

[[nodiscard]] double fractalNoise(
    double worldX, double worldZ,
    const WorldConfig& config,
    std::uint32_t seed) {
    constexpr int Octaves = 4;
    double frequency = config.terrainFrequency * 0.55;
    double amplitude = 1.0;
    double sum = 0.0;
    double weight = 0.0;
    for (int octave = 0; octave < Octaves; ++octave) {
        sum += valueNoise(
                   worldX * frequency,
                   worldZ * frequency,
                   seed +
                       static_cast<std::uint32_t>(
                           octave) *
                           0x9e3779b9U) *
               amplitude;
        weight += amplitude;
        frequency *= 2.05;
        amplitude *= 0.48;
    }
    return weight > 0.0 ? sum / weight : 0.0;
}

[[nodiscard]] double coreBlend(
    double worldX, double worldZ,
    const WorldConfig& config) {
    const double distance =
        std::hypot(worldX, worldZ);
    const double inner = config.coreFlatRadius;
    const double outer =
        std::min(
            config.terrainWorldSize * 0.46,
            inner * 1.7 + 2.0);
    if (distance <= inner) {
        return 0.0;
    }
    if (distance >= outer) {
        return 1.0;
    }
    return smoother(
        (distance - inner) /
        std::max(outer - inner, 1e-6));
}

} // namespace

TerrainHeightfield::TerrainHeightfield(
    WorldConfig config)
    : config_(config),
      seed_(config.terrainSeed),
      spacing_(
          config.terrainWorldSize /
          static_cast<double>(
              config.terrainResolution - 1)) {
    generate(seed_);
}

void TerrainHeightfield::generate(std::uint32_t seed) {
    seed_ = seed;
    const int size = config_.terrainResolution;
    heights_.resize(
        static_cast<std::size_t>(size) *
        static_cast<std::size_t>(size));
    minimumHeight_ =
        std::numeric_limits<double>::infinity();
    maximumHeight_ =
        -std::numeric_limits<double>::infinity();
    const double halfSize =
        config_.terrainWorldSize * 0.5;
    for (int z = 0; z < size; ++z) {
        const double worldZ =
            -halfSize +
            static_cast<double>(z) * spacing_;
        for (int x = 0; x < size; ++x) {
            const double worldX =
                -halfSize +
                static_cast<double>(x) * spacing_;
            const double height =
                fractalNoise(
                    worldX, worldZ, config_, seed_) *
                config_.terrainAmplitude *
                coreBlend(worldX, worldZ, config_);
            heights_[sampleIndex(x, z)] =
                static_cast<float>(height);
            minimumHeight_ =
                std::min(minimumHeight_, height);
            maximumHeight_ =
                std::max(maximumHeight_, height);
        }
    }
}

double TerrainHeightfield::getHeight(
    double worldX, double worldZ) const {
    if (!std::isfinite(worldX) || !std::isfinite(worldZ)) {
        return 0.0;
    }
    const double halfSize =
        config_.terrainWorldSize * 0.5;
    const double sampleX = std::clamp(
        (worldX + halfSize) / spacing_, 0.0,
        static_cast<double>(
            config_.terrainResolution - 1));
    const double sampleZ = std::clamp(
        (worldZ + halfSize) / spacing_, 0.0,
        static_cast<double>(
            config_.terrainResolution - 1));
    const int x0 =
        static_cast<int>(std::floor(sampleX));
    const int z0 =
        static_cast<int>(std::floor(sampleZ));
    const int x1 = std::min(
        x0 + 1, config_.terrainResolution - 1);
    const int z1 = std::min(
        z0 + 1, config_.terrainResolution - 1);
    const double amountX =
        sampleX - static_cast<double>(x0);
    const double amountZ =
        sampleZ - static_cast<double>(z0);
    const double north = interpolate(
        heights_[sampleIndex(x0, z0)],
        heights_[sampleIndex(x1, z0)], amountX);
    const double south = interpolate(
        heights_[sampleIndex(x0, z1)],
        heights_[sampleIndex(x1, z1)], amountX);
    return interpolate(north, south, amountZ);
}

Vec3 TerrainHeightfield::getNormal(
    double worldX, double worldZ) const {
    const double step = spacing_ * 0.5;
    const double left =
        getHeight(worldX - step, worldZ);
    const double right =
        getHeight(worldX + step, worldZ);
    const double north =
        getHeight(worldX, worldZ - step);
    const double south =
        getHeight(worldX, worldZ + step);
    Vec3 normal{
        left - right,
        step * 2.0,
        north - south,
    };
    const double length = std::sqrt(
        normal.x * normal.x +
        normal.y * normal.y +
        normal.z * normal.z);
    if (length <= 1e-9) {
        return {0.0, 1.0, 0.0};
    }
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    return normal;
}

bool TerrainHeightfield::isInside(
    double worldX, double worldZ) const {
    const double halfSize =
        config_.terrainWorldSize * 0.5;
    return worldX >= -halfSize && worldX <= halfSize &&
           worldZ >= -halfSize && worldZ <= halfSize;
}

std::optional<Vec3> TerrainHeightfield::raycast(
    Vec3 origin, Vec3 direction,
    double maximumDistance) const {
    const double directionLength = std::sqrt(
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    if (directionLength <= 1e-9 ||
        maximumDistance <= 0.0) {
        return std::nullopt;
    }
    direction.x /= directionLength;
    direction.y /= directionLength;
    direction.z /= directionLength;
    constexpr double MarchStep = 0.2;
    double previousDistance = 0.0;
    double previousClearance =
        origin.y -
        getHeight(origin.x, origin.z);
    for (double distance = MarchStep;
         distance <= maximumDistance + MarchStep;
         distance += MarchStep) {
        const double clampedDistance =
            std::min(distance, maximumDistance);
        const Vec3 point{
            origin.x +
                direction.x * clampedDistance,
            origin.y +
                direction.y * clampedDistance,
            origin.z +
                direction.z * clampedDistance,
        };
        if (!isInside(point.x, point.z)) {
            return std::nullopt;
        }
        const double clearance =
            point.y -
            getHeight(point.x, point.z);
        if (clearance <= 0.0 &&
            previousClearance >= 0.0) {
            double low = previousDistance;
            double high = clampedDistance;
            for (int iteration = 0;
                 iteration < 10; ++iteration) {
                const double middle =
                    (low + high) * 0.5;
                const Vec3 sample{
                    origin.x + direction.x * middle,
                    origin.y + direction.y * middle,
                    origin.z + direction.z * middle,
                };
                if (sample.y >
                    getHeight(sample.x, sample.z)) {
                    low = middle;
                } else {
                    high = middle;
                }
            }
            const double hitDistance =
                (low + high) * 0.5;
            Vec3 hit{
                origin.x +
                    direction.x * hitDistance,
                0.0,
                origin.z +
                    direction.z * hitDistance,
            };
            hit.y = getHeight(hit.x, hit.z);
            return hit;
        }
        previousDistance = clampedDistance;
        previousClearance = clearance;
        if (clampedDistance >= maximumDistance) {
            break;
        }
    }
    return std::nullopt;
}

std::pair<double, double>
TerrainHeightfield::minMaxHeight() const {
    return {minimumHeight_, maximumHeight_};
}

const WorldConfig& TerrainHeightfield::config() const {
    return config_;
}

std::uint32_t TerrainHeightfield::seed() const {
    return seed_;
}

int TerrainHeightfield::resolution() const {
    return config_.terrainResolution;
}

double TerrainHeightfield::spacing() const {
    return spacing_;
}

std::span<const float>
TerrainHeightfield::samples() const {
    return heights_;
}

std::size_t TerrainHeightfield::sampleIndex(
    int x, int z) const {
    return static_cast<std::size_t>(z) *
               static_cast<std::size_t>(
                   config_.terrainResolution) +
           static_cast<std::size_t>(x);
}

} // namespace ian
