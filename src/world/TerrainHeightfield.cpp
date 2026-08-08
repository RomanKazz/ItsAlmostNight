#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

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

[[nodiscard]] double largeTerrainHeight(
    double worldX, double worldZ,
    const WorldConfig& config,
    std::uint32_t seed) {
    constexpr int Octaves = 3;
    double frequency =
        1.0 / std::max(config.terrainFeatureSize, 1.0);
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
        frequency *= 2.0;
        amplitude *= 0.38;
    }
    const double broad =
        weight > 0.0 ? sum / weight : 0.0;
    const double valley = valueNoise(
        worldX * frequency * 0.19 + 37.2,
        worldZ * frequency * 0.19 - 18.7,
        seed ^ 0x27d4eb2fU);
    return std::clamp(
        (broad * 0.86 + valley * 0.14) *
            config.terrainAmplitude,
        -config.terrainAmplitude,
        config.terrainAmplitude);
}

[[nodiscard]] double terracedHeight(
    double height,
    const WorldConfig& config) {
    const double step =
        std::max(config.terrainTerraceHeight, 0.01);
    const double scaled = height / step;
    const double lower = std::floor(scaled);
    const double fraction = scaled - lower;
    const double transition = std::clamp(
        config.terrainSlopeWidth /
            std::max(config.terrainFeatureSize, 1.0),
        0.10, 0.46);
    const double start = 0.5 - transition * 0.5;
    const double end = 0.5 + transition * 0.5;
    return (lower + smoother(
               (fraction - start) /
               std::max(end - start, 1e-6))) *
           step;
}

struct TerrainPlateau {
    double x{};
    double z{};
    double radius{};
    double height{};
};

struct TerrainShape {
    double height{};
    double protectedAmount{};
};

[[nodiscard]] double distanceToSegment(
    double x, double z,
    const TerrainPlateau& from,
    const TerrainPlateau& to,
    double& progress) {
    const double deltaX = to.x - from.x;
    const double deltaZ = to.z - from.z;
    const double lengthSquared =
        deltaX * deltaX + deltaZ * deltaZ;
    if (lengthSquared <= 1e-9) {
        progress = 0.0;
        return std::hypot(x - from.x, z - from.z);
    }
    progress = std::clamp(
        ((x - from.x) * deltaX +
         (z - from.z) * deltaZ) /
            lengthSquared,
        0.0, 1.0);
    return std::hypot(
        x - (from.x + deltaX * progress),
        z - (from.z + deltaZ * progress));
}

[[nodiscard]] std::vector<TerrainPlateau>
makeBuildPlateaus(
    const WorldConfig& config,
    std::uint32_t seed) {
    const double halfSize = config.terrainWorldSize * 0.5;
    const bool raisedBoundary =
        config.terrainBoundaryRiseWidth > 0.0 &&
        config.terrainBoundaryRiseHeight > 0.0 &&
        config.terrainWorldSize >=
            config.terrainBoundaryRiseWidth * 4.0;
    const double playableLimit = raisedBoundary
        ? halfSize - config.terrainBoundaryRiseWidth
        : halfSize;
    const double radius = std::min(
        config.terrainBuildPlateauRadius,
        std::max(2.0, playableLimit * 0.22));
    std::vector<TerrainPlateau> plateaus;
    plateaus.reserve(5U);
    plateaus.push_back({
        0.0, 0.0,
        std::max(config.coreFlatRadius, radius), 0.0});
    if (playableLimit < config.terrainFeatureSize * 0.75) {
        return plateaus;
    }
    constexpr std::array<std::array<double, 2>, 4>
        Locations{{
            {{-0.34, -0.21}},
            {{0.35, -0.28}},
            {{0.26, 0.34}},
            {{-0.36, 0.29}},
        }};
    for (const auto& location : Locations) {
        const double x = location[0] * playableLimit;
        const double z = location[1] * playableLimit;
        plateaus.push_back({
            x, z, radius,
            terracedHeight(
                largeTerrainHeight(
                    x, z, config, seed),
                config),
        });
    }
    return plateaus;
}

[[nodiscard]] TerrainShape shapeInteriorTerrain(
    double worldX, double worldZ,
    const WorldConfig& config,
    std::uint32_t seed,
    const std::vector<TerrainPlateau>& plateaus) {
    TerrainShape result{
        .height = terracedHeight(
            largeTerrainHeight(
                worldX, worldZ, config, seed),
            config),
    };
    const double feather =
        std::max(config.terrainSlopeWidth, 0.01);
    for (const TerrainPlateau& plateau : plateaus) {
        const double distance = std::hypot(
            worldX - plateau.x,
            worldZ - plateau.z);
        const double mask = 1.0 - smoother(
            (distance - plateau.radius) / feather);
        result.height = interpolate(
            result.height, plateau.height, mask);
        result.protectedAmount =
            std::max(result.protectedAmount, mask);
    }
    if (!plateaus.empty()) {
        const TerrainPlateau& center = plateaus.front();
        const double corridorHalfWidth =
            std::max(3.5,
                     config.terrainBuildPlateauRadius * 0.34);
        for (std::size_t index = 1U;
             index < plateaus.size(); ++index) {
            double progress = 0.0;
            const double distance = distanceToSegment(
                worldX, worldZ, center,
                plateaus[index], progress);
            const double mask = 1.0 - smoother(
                (distance - corridorHalfWidth) /
                (feather * 0.72));
            const double routeHeight = interpolate(
                center.height,
                plateaus[index].height,
                smoother(progress));
            result.height = interpolate(
                result.height, routeHeight, mask);
            result.protectedAmount =
                std::max(result.protectedAmount, mask);
        }
    }
    // Connections must never tilt their destination build plateaus.
    for (const TerrainPlateau& plateau : plateaus) {
        const double distance = std::hypot(
            worldX - plateau.x,
            worldZ - plateau.z);
        const double mask = 1.0 - smoother(
            (distance - plateau.radius) / feather);
        result.height = interpolate(
            result.height, plateau.height, mask);
        result.protectedAmount =
            std::max(result.protectedAmount, mask);
    }
    const double detailScale = std::max(
        0.30 / std::max(config.terrainFrequency, 1e-4),
        4.0);
    const double surfaceNoise = valueNoise(
        worldX / detailScale,
        worldZ / detailScale,
        seed ^ 0x165667b1U) *
        config.terrainSurfaceNoiseAmplitude;
    result.height += surfaceNoise *
        (1.0 - result.protectedAmount);
    return result;
}

[[nodiscard]] double boundaryRise(
    double worldX, double worldZ,
    const WorldConfig& config,
    std::uint32_t seed) {
    if (config.terrainBoundaryRiseWidth <= 0.0 ||
        config.terrainBoundaryRiseHeight <= 0.0 ||
        config.terrainWorldSize <
            config.terrainBoundaryRiseWidth * 4.0) {
        return 0.0;
    }
    const double halfSize =
        config.terrainWorldSize * 0.5;
    const double riseWidth = std::min(
        config.terrainBoundaryRiseWidth,
        std::max(
            0.0,
            halfSize - config.coreFlatRadius - 1.0));
    if (riseWidth <= 0.0) {
        return 0.0;
    }
    const double distanceToBoundary =
        halfSize - std::max(
            std::abs(worldX), std::abs(worldZ));
    const double amount = smoother(
        1.0 - distanceToBoundary /
                  riseWidth);
    // Broad peaks break the boundary silhouette while ridged noise gives
    // steeper mountain faces.  Keeping the multiplier above zero preserves
    // an unbroken raised rim behind the boundary forest.
    const double broad =
        valueNoise(
            worldX * 0.0125,
            worldZ * 0.0125,
            seed ^ 0xa511e9b3U) *
            0.5 +
        0.5;
    const double ridgeNoise = valueNoise(
        worldX * 0.031,
        worldZ * 0.031,
        seed ^ 0x63d83595U);
    const double ridge =
        1.0 - std::abs(ridgeNoise);
    const double detail = valueNoise(
        worldX * 0.068,
        worldZ * 0.068,
        seed ^ 0xc2b2ae35U);
    const double peak = smoother(
        std::clamp(
            broad * 0.72 + ridge * 0.28,
            0.0, 1.0));
    const double mountainShape = std::clamp(
        0.58 + peak * 1.08 + detail * 0.12,
        0.52, 1.78);
    return config.terrainBoundaryRiseHeight *
           amount * amount * mountainShape;
}

[[nodiscard]] double unitHash(std::uint32_t value) {
    return static_cast<double>(mixBits(value)) /
        static_cast<double>(
            std::numeric_limits<std::uint32_t>::max());
}

[[nodiscard]] double pondSignedDistance(
    const PondDefinition& pond,
    double worldX, double worldZ) {
    const double sine = std::sin(pond.rotation);
    const double cosine = std::cos(pond.rotation);
    const double offsetX = worldX - pond.x;
    const double offsetZ = worldZ - pond.z;
    const double localX = offsetX * cosine + offsetZ * sine;
    const double localZ = -offsetX * sine + offsetZ * cosine;
    const double angle = std::atan2(
        localZ / std::max(pond.radiusZ, 0.01),
        localX / std::max(pond.radiusX, 0.01));
    const double organicRadius =
        1.0 +
        std::sin(angle * 3.0 + pond.phase) * 0.095 +
        std::sin(angle * 5.0 - pond.phase * 1.37) * 0.052 +
        std::sin(angle * 7.0 + pond.phase * 0.61) * 0.026;
    const double normalized = std::hypot(
        localX / std::max(pond.radiusX, 0.01),
        localZ / std::max(pond.radiusZ, 0.01));
    double distance =
        (normalized - organicRadius) *
        std::min(pond.radiusX, pond.radiusZ);

    const double bayDistance = std::hypot(
        worldX - (pond.x + std::cos(pond.bayAngle) *
                             pond.radiusX * 0.72),
        worldZ - (pond.z + std::sin(pond.bayAngle) *
                             pond.radiusZ * 0.72)) -
        pond.bayRadius;
    distance = std::min(distance, bayDistance);
    if (pond.islandRadius > 0.0) {
        const double islandDistance = std::hypot(
            worldX - pond.islandX,
            worldZ - pond.islandZ) - pond.islandRadius;
        distance = std::max(distance, -islandDistance);
    }
    return distance;
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
    const std::vector<TerrainPlateau> plateaus =
        makeBuildPlateaus(config_, seed_);
    ponds_.clear();
    for (int z = 0; z < size; ++z) {
        const double worldZ =
            -halfSize +
            static_cast<double>(z) * spacing_;
        for (int x = 0; x < size; ++x) {
            const double worldX =
                -halfSize +
                static_cast<double>(x) * spacing_;
            const TerrainShape interior =
                shapeInteriorTerrain(
                    worldX, worldZ, config_, seed_,
                    plateaus);
            const double height = interior.height +
                boundaryRise(
                    worldX, worldZ, config_, seed_);
            heights_[sampleIndex(x, z)] =
                static_cast<float>(height);
            minimumHeight_ =
                std::min(minimumHeight_, height);
            maximumHeight_ =
                std::max(maximumHeight_, height);
        }
    }

    if (config_.pondMaximumCount <= 0 ||
        config_.pondMaximumAreaFraction <= 0.0) {
        return;
    }
    const std::uint32_t waterSeed =
        mixBits(seed_ ^ config_.pondSeed);
    const int countRange =
        config_.pondMaximumCount - config_.pondMinimumCount + 1;
    const int requestedCount = config_.pondMinimumCount +
        static_cast<int>(waterSeed %
            static_cast<std::uint32_t>(std::max(countRange, 1)));
    const double playableLimit = std::max(
        config_.coreFlatRadius + 12.0,
        halfSize - config_.terrainBoundaryRiseWidth - 3.0);
    const double playableArea =
        playableLimit * playableLimit * 4.0;
    const double targetArea = playableArea *
        config_.pondMaximumAreaFraction *
        (0.68 + unitHash(waterSeed ^ 0x68e31da4U) * 0.27);
    ponds_.reserve(static_cast<std::size_t>(requestedCount));
    for (int pondIndex = 0; pondIndex < requestedCount; ++pondIndex) {
        const std::uint32_t pondHash = mixBits(
            waterSeed + static_cast<std::uint32_t>(pondIndex) *
                            0x9e3779b9U);
        const double areaVariation =
            0.72 + unitHash(pondHash ^ 0xa511e9b3U) * 0.56;
        const double nominalArea =
            targetArea / static_cast<double>(requestedCount) *
            areaVariation;
        const double aspect =
            0.68 + unitHash(pondHash ^ 0x63d83595U) * 0.64;
        double radiusX = std::sqrt(
            nominalArea /
            (3.14159265358979323846 * aspect));
        double radiusZ = radiusX * aspect;
        radiusX = std::clamp(
            radiusX, config_.pondMinimumRadius,
            config_.pondMaximumRadius);
        radiusZ = std::clamp(
            radiusZ, config_.pondMinimumRadius * 0.72,
            config_.pondMaximumRadius);
        const double maximumRadius = std::max(radiusX, radiusZ);
        const double candidateLimit =
            playableLimit - maximumRadius -
            config_.pondShorelineWidth * 2.0;
        if (candidateLimit <= maximumRadius) {
            continue;
        }

        double bestX = 0.0;
        double bestZ = 0.0;
        double bestScore =
            std::numeric_limits<double>::infinity();
        bool found = false;
        for (int attempt = 0; attempt < 240; ++attempt) {
            const std::uint32_t attemptHash = mixBits(
                pondHash + static_cast<std::uint32_t>(attempt) *
                               0x85ebca6bU);
            const double x =
                (unitHash(attemptHash) * 2.0 - 1.0) *
                candidateLimit;
            const double z =
                (unitHash(attemptHash ^ 0xc2b2ae35U) * 2.0 - 1.0) *
                candidateLimit;
            constexpr double RouteHalfWidth = 7.0;
            if (std::abs(x) <= radiusX + RouteHalfWidth ||
                std::abs(z) <= radiusZ + RouteHalfWidth ||
                std::hypot(x, z) <=
                    maximumRadius + config_.coreFlatRadius + 10.0) {
                continue;
            }
            bool overlapsProtected = false;
            for (const TerrainPlateau& plateau : plateaus) {
                if (std::hypot(x - plateau.x, z - plateau.z) <
                    maximumRadius + plateau.radius + 5.0) {
                    overlapsProtected = true;
                    break;
                }
            }
            if (overlapsProtected) {
                continue;
            }
            const bool overlapsPond = std::any_of(
                ponds_.begin(), ponds_.end(),
                [x, z, maximumRadius](const PondDefinition& pond) {
                    return std::hypot(x - pond.x, z - pond.z) <
                        maximumRadius +
                            std::max(pond.radiusX, pond.radiusZ) + 7.0;
                });
            if (overlapsPond) {
                continue;
            }
            const std::array<double, 5> samples{{
                getHeight(x, z),
                getHeight(x + radiusX * 0.42, z),
                getHeight(x - radiusX * 0.42, z),
                getHeight(x, z + radiusZ * 0.42),
                getHeight(x, z - radiusZ * 0.42),
            }};
            const auto [lowest, highest] =
                std::minmax_element(samples.begin(), samples.end());
            const double average =
                std::accumulate(samples.begin(), samples.end(), 0.0) /
                static_cast<double>(samples.size());
            const double score = average +
                (*highest - *lowest) * 1.8 +
                unitHash(attemptHash ^ 0x27d4eb2fU) * 0.35;
            if (score < bestScore) {
                bestScore = score;
                bestX = x;
                bestZ = z;
                found = true;
            }
        }
        if (!found) {
            continue;
        }
        const double centerHeight = getHeight(bestX, bestZ);
        const double depth = interpolate(
            config_.pondMinimumDepth,
            config_.pondMaximumDepth,
            unitHash(pondHash ^ 0x165667b1U));
        PondDefinition pond{
            .x = bestX,
            .z = bestZ,
            .radiusX = radiusX,
            .radiusZ = radiusZ,
            .rotation = unitHash(pondHash ^ 0xb5297a4dU) *
                3.14159265358979323846,
            .waterLevel = centerHeight + 0.18,
            .depth = depth,
            .phase = unitHash(pondHash ^ 0x1b56c4e9U) *
                6.28318530717958647692,
            .bayAngle = unitHash(pondHash ^ 0x94d049bbU) *
                6.28318530717958647692,
            .bayRadius = std::min(radiusX, radiusZ) *
                (0.22 + unitHash(pondHash ^ 0x7f4a7c15U) * 0.12),
        };
        if (unitHash(pondHash ^ 0xd8163841U) > 0.52) {
            const double islandAngle = pond.phase + 1.3;
            pond.islandX = bestX + std::cos(islandAngle) * radiusX * 0.24;
            pond.islandZ = bestZ + std::sin(islandAngle) * radiusZ * 0.24;
            pond.islandRadius = std::min(radiusX, radiusZ) *
                (0.10 + unitHash(pondHash ^ 0xca01f9ddU) * 0.08);
        }
        ponds_.push_back(pond);
    }

    const double bankWidth = std::max(
        config_.pondShorelineWidth * 2.2, spacing_ * 3.0);
    minimumHeight_ =
        std::numeric_limits<double>::infinity();
    maximumHeight_ =
        -std::numeric_limits<double>::infinity();
    for (int z = 0; z < size; ++z) {
        const double worldZ =
            -halfSize + static_cast<double>(z) * spacing_;
        for (int x = 0; x < size; ++x) {
            const double worldX =
                -halfSize + static_cast<double>(x) * spacing_;
            double height = heights_[sampleIndex(x, z)];
            for (const PondDefinition& pond : ponds_) {
                const double distance = pondSignedDistance(
                    pond, worldX, worldZ);
                if (distance >= bankWidth) {
                    continue;
                }
                if (distance <= 0.0) {
                    const double centerAmount = smoother(
                        std::clamp(
                            -distance /
                                (std::min(
                                    pond.radiusX, pond.radiusZ) * 0.70),
                            0.0, 1.0));
                    const double desired = pond.waterLevel - 0.06 -
                        std::max(pond.depth - 0.06, 0.0) *
                            centerAmount;
                    height = interpolate(height, desired, 0.98);
                } else {
                    const double bankAmount = std::clamp(
                        distance / bankWidth, 0.0, 1.0);
                    const double desired = pond.waterLevel - 0.06 +
                        bankAmount * 0.30;
                    height = interpolate(
                        height, desired,
                        1.0 - smoother(bankAmount));
                }
            }
            heights_[sampleIndex(x, z)] =
                static_cast<float>(height);
            minimumHeight_ = std::min(minimumHeight_, height);
            maximumHeight_ = std::max(maximumHeight_, height);
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

double TerrainHeightfield::getBackdropHeight(
    double worldX, double worldZ) const {
    if (!std::isfinite(worldX) || !std::isfinite(worldZ)) {
        return 0.0;
    }
    const double halfSize = config_.terrainWorldSize * 0.5;
    const double edgeDistance =
        std::max(std::abs(worldX), std::abs(worldZ)) - halfSize;
    if (edgeDistance <= 0.0) {
        return getHeight(worldX, worldZ);
    }

    // The backdrop is deliberately wider than the in-map boundary rise so
    // the camera sees several overlapping ridges instead of a single wall.
    const double backdropWidth = std::max(
        config_.terrainBoundaryRiseWidth * 2.25, 96.0);
    const double outsideAmount = smoother(
        edgeDistance / backdropWidth);
    const double edgeX = std::clamp(worldX, -halfSize, halfSize);
    const double edgeZ = std::clamp(worldZ, -halfSize, halfSize);
    const double edgeHeight = getHeight(edgeX, edgeZ);

    // Broad noise defines the mountain mass, ridged noise creates distinct
    // crests, and the finer octave breaks up the silhouette. All fields are
    // seeded from the same world seed, so regenerated maps stay coherent.
    const double broad = valueNoise(
        worldX * 0.0105,
        worldZ * 0.0105,
        seed_ ^ 0x6d2b79f5U) * 0.5 + 0.5;
    const double ridgeNoise = valueNoise(
        worldX * 0.024,
        worldZ * 0.024,
        seed_ ^ 0xa511e9b3U);
    const double ridge = std::pow(
        1.0 - std::abs(ridgeNoise), 1.55);
    const double detail = valueNoise(
        worldX * 0.071,
        worldZ * 0.071,
        seed_ ^ 0x63d83595U);
    const double mountainRise =
        22.0 + broad * 34.0 + ridge * 27.0 + detail * 6.0;
    return edgeHeight + outsideAmount * mountainRise;
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

std::span<const PondDefinition>
TerrainHeightfield::ponds() const {
    return ponds_;
}

double TerrainHeightfield::waterSignedDistance(
    double worldX, double worldZ) const {
    double distance = std::numeric_limits<double>::infinity();
    for (const PondDefinition& pond : ponds_) {
        distance = std::min(
            distance,
            pondSignedDistance(pond, worldX, worldZ));
    }
    return distance;
}

std::optional<double> TerrainHeightfield::waterSurfaceHeight(
    double worldX, double worldZ) const {
    const PondDefinition* closest = nullptr;
    double closestDistance =
        std::numeric_limits<double>::infinity();
    for (const PondDefinition& pond : ponds_) {
        const double distance = pondSignedDistance(
            pond, worldX, worldZ);
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = &pond;
        }
    }
    if (closest == nullptr || closestDistance > 0.0) {
        return std::nullopt;
    }
    return closest->waterLevel;
}

double TerrainHeightfield::waterDepth(
    double worldX, double worldZ) const {
    const auto surface = waterSurfaceHeight(worldX, worldZ);
    return surface
        ? std::max(0.0, *surface - getHeight(worldX, worldZ))
        : 0.0;
}

bool TerrainHeightfield::isDeepWater(
    double worldX, double worldZ) const {
    return waterDepth(worldX, worldZ) >=
        config_.pondDeepWaterDepth;
}

double TerrainHeightfield::waterMovementMultiplier(
    double worldX, double worldZ) const {
    const double depth = waterDepth(worldX, worldZ);
    if (depth <= 0.02) {
        return 1.0;
    }
    const double amount = smoother(std::clamp(
        depth / std::max(config_.pondDeepWaterDepth, 0.01),
        0.0, 1.0));
    return interpolate(
        1.0, config_.pondShallowMovementMultiplier, amount);
}

std::size_t TerrainHeightfield::sampleIndex(
    int x, int z) const {
    return static_cast<std::size_t>(z) *
               static_cast<std::size_t>(
                   config_.terrainResolution) +
           static_cast<std::size_t>(x);
}

} // namespace ian
