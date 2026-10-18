#include "ui/MinimapLandscape.hpp"
#include "game/Simulation.hpp"
#include "world/VisualBiomes.hpp"

#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <thread>
#include <system_error>
#include <vector>

namespace ian {
namespace {
constexpr int AtlasSize = 1024;
// Cartographic colors deliberately improve contrast without recoloring the world.
Color terrainColor(const visual_biomes::Layout& layout, float x, float z) {
    constexpr std::array<Color, 4> palette{{
        {113, 174, 61, 255}, {185, 79, 145, 255},
        {216, 155, 67, 255}, {116, 194, 226, 255}}};
    const auto weights = visual_biomes::weights(layout, x, z);
    float r = 0, g = 0, b = 0;
    for (std::size_t i = 0; i < palette.size(); ++i) {
        r += weights[i] * palette[i].r;
        g += weights[i] * palette[i].g;
        b += weights[i] * palette[i].b;
    }
    return {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
        static_cast<unsigned char>(b), 255};
}
float sampleHeight(const SimulationSnapshot& snapshot, double x, double z) {
    if (snapshot.terrainResolution < 2 || snapshot.terrainSamples.size() <
        static_cast<std::size_t>(snapshot.terrainResolution) * static_cast<std::size_t>(snapshot.terrainResolution))
        return 0;
    const int n = snapshot.terrainResolution;
    const double half = snapshot.terrainWorldSize * .5;
    const double gx = std::clamp((x + half) / snapshot.terrainWorldSize * (n - 1), 0.0, double(n - 1));
    const double gz = std::clamp((z + half) / snapshot.terrainWorldSize * (n - 1), 0.0, double(n - 1));
    const int ix = static_cast<int>(gx), iz = static_cast<int>(gz);
    const auto at = [&](int sx, int sz) { return snapshot.terrainSamples[static_cast<std::size_t>(sz) * static_cast<std::size_t>(n) + static_cast<std::size_t>(sx)]; };
    return std::lerp(std::lerp(at(ix, iz), at(std::min(ix + 1, n - 1), iz), static_cast<float>(gx - ix)),
        std::lerp(at(ix, std::min(iz + 1, n - 1)), at(std::min(ix + 1, n - 1), std::min(iz + 1, n - 1)), static_cast<float>(gx - ix)),
        static_cast<float>(gz - iz));
}
} // namespace

void MinimapLandscape::shutdown() {
    if (IsTextureValid(texture_)) UnloadTexture(texture_);
    texture_ = {};
    samples_ = nullptr;
}

void MinimapLandscape::update(const SimulationSnapshot& snapshot) {
    if (IsTextureValid(texture_) && seed_ == snapshot.terrainSeed &&
        samples_ == snapshot.terrainSamples.data() && resolution_ == snapshot.terrainResolution &&
        worldSize_ == snapshot.terrainWorldSize) return;
    shutdown();
    if (snapshot.terrainWorldSize <= 0) return;
    seed_ = snapshot.terrainSeed;
    samples_ = snapshot.terrainSamples.data();
    resolution_ = snapshot.terrainResolution;
    worldSize_ = snapshot.terrainWorldSize;
    const double step = worldSize_ / AtlasSize;
    const double half = worldSize_ * .5;
    const auto biomes = visual_biomes::generate(seed_, worldSize_);
    std::vector<float> heights(AtlasSize * AtlasSize);
    std::vector<float> shores(AtlasSize * AtlasSize, std::numeric_limits<float>::infinity());
    for (int z = 0; z < AtlasSize; ++z)
        for (int x = 0; x < AtlasSize; ++x)
            heights[static_cast<std::size_t>(z) * AtlasSize + static_cast<std::size_t>(x)] =
                sampleHeight(snapshot, (x + .5) * step - half, (z + .5) * step - half);
    // Use the same signed shape as world collision, including bays and islands.
    // Restrict expensive shoreline sampling to each lake's bounding rectangle.
    for (const PondDefinition& pond : snapshot.ponds) {
        const double extent = std::max(pond.radiusX, pond.radiusZ) * 1.2 + pond.bayRadius + 3;
        const auto pixel = [&](double v) { return std::clamp(static_cast<int>((v + half) / step), 0, AtlasSize - 1); };
        for (int z = pixel(pond.z - extent); z <= pixel(pond.z + extent); ++z)
            for (int x = pixel(pond.x - extent); x <= pixel(pond.x + extent); ++x) {
                auto& distance = shores[static_cast<std::size_t>(z) * AtlasSize + static_cast<std::size_t>(x)];
                distance = std::min(distance, static_cast<float>(pond.signedDistance(
                    (x + .5) * step - half, (z + .5) * step - half)));
            }
    }
    Image image = GenImageColor(AtlasSize, AtlasSize, BLANK);
    auto* pixels = static_cast<Color*>(image.data);
    const auto heightAt = [&](int x, int z) { return heights[static_cast<std::size_t>(std::clamp(z, 0, AtlasSize - 1)) * AtlasSize + static_cast<std::size_t>(std::clamp(x, 0, AtlasSize - 1))]; };
    // Each row only reads the completed height/shore fields and writes its
    // own pixels. Keep all raylib texture operations on the calling thread.
    const auto paintRows = [&](int firstRow, int endRow) {
        for (int z = firstRow; z < endRow; ++z) {
            for (int x = 0; x < AtlasSize; ++x) {
                const std::size_t i = static_cast<std::size_t>(z) * AtlasSize + static_cast<std::size_t>(x);
                const float height = heights[i];
                const float dx = static_cast<float>((heightAt(x + 2, z) - heightAt(x - 2, z)) / (4 * step));
                const float dz = static_cast<float>((heightAt(x, z + 2) - heightAt(x, z - 2)) / (4 * step));
                const float light = std::clamp(1.01F - .24F * (dx + dz * .8F) /
                    std::sqrt(1 + dx * dx + dz * dz), .82F, 1.16F);
                Color color = terrainColor(biomes,
                    static_cast<float>((x + .5) * step - half),
                    static_cast<float>((z + .5) * step - half));
                color.r = static_cast<unsigned char>(std::min(255.0F, color.r * light));
                color.g = static_cast<unsigned char>(std::min(255.0F, color.g * light));
                color.b = static_cast<unsigned char>(std::min(255.0F, color.b * light));
                // Sparse contour lines aid navigation without competing with markers.
                const float contour = std::abs(std::remainder(height, 3.0F));
                const float slope = std::hypot(dx, dz);
                const float contourWidth = static_cast<float>(step) * slope * .7F;
                if (slope > .08F && contour < contourWidth)
                    color = ColorLerp(color, {37, 79, 51, 255}, .12F * (1 - contour / contourWidth));
                const float shore = shores[i];
                if (shore < 1.6F && shore >= 0)
                    color = ColorLerp({237, 218, 147, 255}, color, shore / 1.6F);
                if (shore < 0) {
                    const float depth = std::clamp((-shore - .5F) / 7.0F, 0.0F, 1.0F);
                    color = ColorLerp({26, 224, 215, 255}, {6, 133, 164, 255}, depth);
                    if (shore > -.55F)
                        color = ColorLerp({144, 255, 239, 255}, color, -shore / .55F);
                    // Short, quiet cartographic wave strokes, clipped to deep water.
                    const double wx = (x + .5) * step - half;
                    const double wz = (z + .5) * step - half;
                    if (shore < -3 && std::abs(std::remainder(wz + std::sin(wx * .17) * .45, 5.0)) < step * .55 &&
                        std::sin(wx * .42 + std::floor(wz / 5) * 2.7) > .72)
                        color = ColorLerp(color, {171, 232, 231, 255}, .35F);
                }
                pixels[i] = color;
            }
        }
    };
    {
        constexpr unsigned int MaximumWorkers = 4;
        const unsigned int workerCount = std::clamp(
            std::thread::hardware_concurrency(), 1U, MaximumWorkers);
        std::array<std::jthread, MaximumWorkers - 1> workers;
        unsigned int started = 0;
        // If a thread cannot be created, paint its rows on this thread too.
        // Existing jthreads still join before the image is uploaded/freed.
        try {
            for (; started + 1 < workerCount; ++started) {
                const int first = static_cast<int>(started) * AtlasSize /
                    static_cast<int>(workerCount);
                const int end = static_cast<int>(started + 1) * AtlasSize /
                    static_cast<int>(workerCount);
                workers[started] = std::jthread(paintRows, first, end);
            }
        } catch (const std::system_error&) {
            // Resource pressure only changes how many CPU workers are used.
        }
        paintRows(static_cast<int>(started) * AtlasSize /
            static_cast<int>(workerCount), AtlasSize);
    }
    // Roads and vegetation are baked once with the landscape, never submitted
    // as hundreds of per-frame HUD sprites. Their positions come from the world.
    const auto atlasPoint = [&](double x, double z) {
        return Vector2{static_cast<float>((x + half) / step),
            static_cast<float>((z + half) / step)};
    };
    const auto dry = [&](Vector2 p, float clearance) {
        if (p.x < 0 || p.y < 0 || p.x >= AtlasSize || p.y >= AtlasSize) return false;
        return shores[static_cast<std::size_t>(p.y) * AtlasSize +
            static_cast<std::size_t>(p.x)] > clearance;
    };
    std::vector<Vector2> roadCenters;
    std::vector<float> roadMask(AtlasSize * AtlasSize);
    for (const auto& path : snapshot.terrainPaths) {
        const auto curve = [&](double t) {
            const double u = 1 - t;
            return atlasPoint(u*u*path.fromX + 2*u*t*path.controlX + t*t*path.toX,
                u*u*path.fromZ + 2*u*t*path.controlZ + t*t*path.toZ);
        };
        for (int segment = 0; segment < 128; ++segment) {
            const double t = (segment + .5) / 128.0;
            const float fade = static_cast<float>(std::min(
                path.fadeIn > 0 ? std::clamp(t / path.fadeIn, 0.0, 1.0) : 1.0,
                path.fadeOut > 0 ? std::clamp((1-t) / path.fadeOut, 0.0, 1.0) : 1.0));
            const Vector2 from = curve(segment / 128.0), to = curve((segment + 1) / 128.0);
            const float width = static_cast<float>(path.halfWidth * .8 / step);
            const int left = std::max(0, static_cast<int>(std::min(from.x, to.x)-width-2));
            const int right = std::min(AtlasSize-1, static_cast<int>(std::max(from.x, to.x)+width+2));
            const int top = std::max(0, static_cast<int>(std::min(from.y, to.y)-width-2));
            const int bottom = std::min(AtlasSize-1, static_cast<int>(std::max(from.y, to.y)+width+2));
            const float dx = to.x-from.x, dz = to.y-from.y, lengthSquared = dx*dx+dz*dz;
            for (int z = top; z <= bottom; ++z) for (int x = left; x <= right; ++x) {
                const float px = static_cast<float>(x)+.5F-from.x, pz = static_cast<float>(z)+.5F-from.y;
                const float progress = lengthSquared > .0001F ? std::clamp((px*dx+pz*dz)/lengthSquared, 0.0F, 1.0F) : 0;
                const float distance = std::hypot(px-dx*progress, pz-dz*progress);
                auto& mask = roadMask[static_cast<std::size_t>(z)*AtlasSize+static_cast<std::size_t>(x)];
                mask = std::max(mask, std::clamp(width+.8F-distance, 0.0F, 1.0F)*fade);
            }
            roadCenters.push_back(from);
        }
    }
    for (std::size_t i = 0; i < roadMask.size(); ++i)
        if (shores[i] > .8F && roadMask[i] > 0)
            pixels[i] = ColorLerp(pixels[i], {238, 211, 139, 255}, roadMask[i]*.86F);
    constexpr std::array paths{"assets/ui/map/forest.png", "assets/ui/map/corrupted.png",
        "assets/ui/map/desert.png", "assets/ui/map/snow.png", "assets/ui/map/boulders.png",
        "assets/ui/map/mountain.png"};
    std::array<Image, paths.size()> stamps{};
    for (std::size_t i = 0; i < paths.size(); ++i) stamps[i] = LoadImage(paths[i]);
    const auto stamp = [&](std::size_t index, Vector2 p, float size) {
        if (IsImageValid(stamps[index])) ImageDraw(&image, stamps[index], {0, 0, 256, 256},
            {p.x - size * .5F, p.y - size * .78F, size, size}, WHITE);
    };
    // Stable world cells retain the same silhouette when a resource is depleted.
    std::set<std::pair<int, int>> trees, stones;
    for (const auto& node : snapshot.resourceNodes) {
        const bool tree = node.type == ResourceType::Wood;
        if (!tree && node.type != ResourceType::Stone) continue;
        const Vector2 point = atlasPoint(node.position.x, node.position.z);
        if (!dry(point, 3.0F) || std::ranges::any_of(roadCenters, [&](Vector2 road) {
                return std::hypot(point.x-road.x, point.y-road.y) < 5.0 / step;
            })) continue;
        const double spacing = tree ? 11.0 : 24.0;
        auto& cells = tree ? trees : stones;
        if (!cells.emplace(static_cast<int>(std::floor(node.position.x / spacing)),
            static_cast<int>(std::floor(node.position.z / spacing))).second) continue;
        if (!tree) { stamp(4, point, static_cast<float>(7.5 / step)); continue; }
        const auto weights = visual_biomes::weights(biomes,
            static_cast<float>(node.position.x), static_cast<float>(node.position.z));
        const std::size_t biome = static_cast<std::size_t>(
            std::max_element(weights.begin(), weights.end()) - weights.begin());
        const float variation = visual_biomes::randomUnit(seed_, node.id.index);
        stamp(biome, point, static_cast<float>((10.0 + 2.0 * variation) / step));
    }
    // Relief symbols only mark real raised, steep terrain, never invented obstacles.
    for (double z = -half + 14; z < half - 14; z += 22)
        for (double x = -half + 14; x < half - 14; x += 22) {
            const float h = sampleHeight(snapshot, x, z);
            const float slope = std::hypot(sampleHeight(snapshot, x+3, z) - h,
                sampleHeight(snapshot, x, z+3) - h) / 3;
            const Vector2 point = atlasPoint(x, z);
            if (h > 10 && slope > .6F && dry(point, 5))
                stamp(5, point, static_cast<float>(16.0 / step));
        }
    for (auto& sprite : stamps) if (IsImageValid(sprite)) UnloadImage(sprite);
    texture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    if (IsTextureValid(texture_)) SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
}

void MinimapLandscape::draw(const SimulationSnapshot& snapshot, Rectangle bounds, double radius) {
    update(snapshot);
    DrawRectangleRec(bounds, {53, 79, 57, 255});
    if (!IsTextureValid(texture_)) return;
    const double half = worldSize_ * .5;
    const double left = std::max(-half, snapshot.playerPosition.x - radius);
    const double top = std::max(-half, snapshot.playerPosition.z - radius);
    const double right = std::min(half, snapshot.playerPosition.x + radius);
    const double bottom = std::min(half, snapshot.playerPosition.z + radius);
    if (right <= left || bottom <= top) return;
    const float scale = bounds.width / static_cast<float>(2 * radius);
    const float sourceScale = static_cast<float>(AtlasSize / worldSize_);
    DrawTexturePro(texture_,
        {static_cast<float>(left + half) * sourceScale, static_cast<float>(top + half) * sourceScale,
         static_cast<float>(right - left) * sourceScale, static_cast<float>(bottom - top) * sourceScale},
        {bounds.x + static_cast<float>(left - snapshot.playerPosition.x + radius) * scale,
         bounds.y + static_cast<float>(top - snapshot.playerPosition.z + radius) * scale,
         static_cast<float>(right - left) * scale, static_cast<float>(bottom - top) * scale}, {}, 0, WHITE);
}
} // namespace ian
