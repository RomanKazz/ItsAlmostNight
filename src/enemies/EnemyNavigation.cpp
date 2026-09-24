#include "enemies/EnemyNavigation.hpp"

#include "buildings/ModularBuildingConstants.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <tuple>
#include <utility>

namespace ian {
namespace {

struct TraversalEdge {
    int to{-1};
    const RampInstance* ramp{};
};

struct RampEndpoints {
    Vec3 lowInside;
    Vec3 highInside;
    Vec3 lowConnection;
    Vec3 highConnection;
};

RampEndpoints rampEndpoints(
    const RampInstance& ramp, double cellSize) {
    const bool alongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    const double minimumX = ramp.anchor.x * cellSize;
    const double maximumX =
        (ramp.anchor.x +
         (alongZ ? ModularRampWidthCells
                 : ModularRampRunCells)) * cellSize;
    const double minimumZ = ramp.anchor.z * cellSize;
    const double maximumZ =
        (ramp.anchor.z +
         (alongZ ? ModularRampRunCells
                 : ModularRampWidthCells)) * cellSize;
    const double inset = std::min(0.18, cellSize * 0.18);
    const double connection = std::min(0.12, cellSize * 0.12);
    const double centerX = (minimumX + maximumX) * 0.5;
    const double centerZ = (minimumZ + maximumZ) * 0.5;
    switch (ramp.rotation) {
    case Rotation::Deg0:
        return {
            {centerX, ramp.bottomHeight, minimumZ + inset},
            {centerX, ramp.topHeight, maximumZ - inset},
            {centerX, ramp.bottomHeight, minimumZ - connection},
            {centerX, ramp.topHeight, maximumZ + connection},
        };
    case Rotation::Deg90:
        return {
            {maximumX - inset, ramp.bottomHeight, centerZ},
            {minimumX + inset, ramp.topHeight, centerZ},
            {maximumX + connection, ramp.bottomHeight, centerZ},
            {minimumX - connection, ramp.topHeight, centerZ},
        };
    case Rotation::Deg180:
        return {
            {centerX, ramp.bottomHeight, maximumZ - inset},
            {centerX, ramp.topHeight, minimumZ + inset},
            {centerX, ramp.bottomHeight, maximumZ + connection},
            {centerX, ramp.topHeight, minimumZ - connection},
        };
    case Rotation::Deg270:
        return {
            {minimumX + inset, ramp.bottomHeight, centerZ},
            {maximumX - inset, ramp.topHeight, centerZ},
            {minimumX - connection, ramp.bottomHeight, centerZ},
            {maximumX + connection, ramp.topHeight, centerZ},
        };
    }
    return {};
}

} // namespace

class MultiLevelNavigation {
  public:
    MultiLevelNavigation(
        EnemyNavigationView view, Vec3 corePosition,
        int coreStorey)
        : view_(view), groundNode_(
              static_cast<int>(view.platformFrames.size())) {
        if (coreStorey >= 0 && view.platformFrames.empty()) {
            return;
        }
        edges_.resize(view.platformFrames.size() + 1U);
        for (std::size_t index = 0;
             index < view.platformFrames.size(); ++index) {
            const PlatformFrameInstance& frame =
                view.platformFrames[index];
            if (frame.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            frameIndex_.emplace(
                std::tuple{
                    frame.storey,
                    frame.anchor.x,
                    frame.anchor.z},
                static_cast<int>(index));
            maximumStorey_ = std::max(
                maximumStorey_, frame.storey);
        }
        for (std::size_t left = 0;
             left < view.platformFrames.size(); ++left) {
            const PlatformFrameInstance& a =
                view.platformFrames[left];
            if (a.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            if (a.storey == 0) {
                connect(groundNode_, static_cast<int>(left), nullptr);
            }
            constexpr std::array<GridCoord, 2> ForwardNeighbours{{
                {PlatformFrameWidthCells, 0, 0},
                {0, 0, PlatformFrameWidthCells},
            }};
            for (const GridCoord offset : ForwardNeighbours) {
                const auto neighbour = frameIndex_.find(
                    std::tuple{
                        a.storey,
                        a.anchor.x + offset.x,
                        a.anchor.z + offset.z});
                if (neighbour == frameIndex_.end()) {
                    continue;
                }
                const PlatformFrameInstance& b =
                    view.platformFrames[
                        static_cast<std::size_t>(neighbour->second)];
                if (std::abs(a.floorHeight - b.floorHeight) <= 1e-4) {
                    connect(
                        static_cast<int>(left),
                        neighbour->second, nullptr);
                }
            }
        }
        goalNode_ = coreStorey < 0
            ? groundNode_
            : frameAt(corePosition, coreStorey, corePosition.y);
        if (goalNode_ < 0) {
            return;
        }
        for (const RampInstance& ramp : view.ramps) {
            if (ramp.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            const RampEndpoints endpoints =
                rampEndpoints(ramp, view.cellSize);
            const int low = frameAt(
                endpoints.lowConnection,
                ramp.targetStorey - 1,
                ramp.bottomHeight);
            const int high = frameAt(
                endpoints.highConnection,
                ramp.targetStorey,
                ramp.topHeight);
            if (low >= 0 && high >= 0) {
                connect(low, high, &ramp);
            }
        }
        buildRoutes();
    }

    [[nodiscard]] std::optional<Vec3> waypoint(
        const EnemyInstance& enemy, double terrainHeight,
        Vec3 corePosition) const {
        if (!valid()) {
            return std::nullopt;
        }
        const double surfaceHeight =
            terrainHeight + enemy.surfaceHeightOffset;
        for (const RampInstance& ramp : view_.ramps) {
            if (ramp.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            const RampEndpoints endpoints =
                rampEndpoints(ramp, view_.cellSize);
            if (!insideRamp(enemy.position, ramp) ||
                std::abs(surfaceHeight - ramp.bottomHeight) >
                    std::abs(ramp.topHeight - ramp.bottomHeight) + 0.6) {
                continue;
            }
            const int low = frameAt(
                endpoints.lowConnection,
                ramp.targetStorey - 1, ramp.bottomHeight);
            const int high = frameAt(
                endpoints.highConnection,
                ramp.targetStorey, ramp.topHeight);
            if (low >= 0 && high >= 0) {
                const int lowDistance =
                    distance_[static_cast<std::size_t>(low)];
                const int highDistance =
                    distance_[static_cast<std::size_t>(high)];
                if (lowDistance >= 0 && highDistance >= 0) {
                    return highDistance < lowDistance
                        ? endpoints.highConnection
                        : endpoints.lowConnection;
                }
            }
        }

        int current = groundNode_;
        for (int storey = 0;
             storey <= maximumStorey_; ++storey) {
            const int frame = frameAt(
                enemy.position, storey, surfaceHeight);
            if (frame >= 0) {
                current = frame;
                break;
            }
        }
        if (current == goalNode_) {
            return current == groundNode_
                ? groundRampDetour(
                      enemy, surfaceHeight, corePosition)
                : std::optional<Vec3>{corePosition};
        }
        if (current < 0 ||
            current >= static_cast<int>(next_.size()) ||
            !next_[static_cast<std::size_t>(current)]) {
            return std::nullopt;
        }
        const TraversalEdge edge =
            *next_[static_cast<std::size_t>(current)];
        if (edge.ramp) {
            const RampEndpoints endpoints =
                rampEndpoints(*edge.ramp, view_.cellSize);
            return view_.platformFrames[
                       static_cast<std::size_t>(current)].storey <
                    edge.ramp->targetStorey
                ? endpoints.lowInside
                : endpoints.highInside;
        }
        if (edge.to == groundNode_) {
            return groundRampDetour(
                enemy, surfaceHeight, corePosition);
        }
        return frameCenter(
            view_.platformFrames[static_cast<std::size_t>(edge.to)]);
    }

    [[nodiscard]] bool valid() const {
        return goalNode_ >= 0 &&
            groundNode_ < static_cast<int>(distance_.size()) &&
            distance_[static_cast<std::size_t>(groundNode_)] >= 0;
    }

  private:
    [[nodiscard]] static bool crossesRectangle(
        Vec3 start, Vec3 end, double minimumX,
        double maximumX, double minimumZ,
        double maximumZ) {
        // The small inset makes a path along the padded rectangle edge
        // visible, while paths through its interior remain blocked.
        minimumX += 0.01;
        maximumX -= 0.01;
        minimumZ += 0.01;
        maximumZ -= 0.01;
        double enter = 0.0;
        double leave = 1.0;
        const auto clip = [&enter, &leave](
                              double origin, double delta,
                              double minimum, double maximum) {
            if (std::abs(delta) < 1e-9) {
                return origin > minimum && origin < maximum;
            }
            double first = (minimum - origin) / delta;
            double last = (maximum - origin) / delta;
            if (first > last) {
                std::swap(first, last);
            }
            enter = std::max(enter, first);
            leave = std::min(leave, last);
            return enter < leave;
        };
        return clip(start.x, end.x - start.x,
                    minimumX, maximumX) &&
            clip(start.z, end.z - start.z,
                 minimumZ, maximumZ) &&
            enter < 1.0 && leave > 0.0;
    }

    [[nodiscard]] std::optional<Vec3> groundRampDetour(
        const EnemyInstance& enemy, double surfaceHeight,
        Vec3 corePosition) const {
        if (goalNode_ != groundNode_) {
            return std::nullopt;
        }
        double bestRampDistance =
            std::numeric_limits<double>::infinity();
        std::optional<Vec3> bestWaypoint;
        for (const RampInstance& ramp : view_.ramps) {
            if (ramp.supportState !=
                    StructuralSupportState::Supported ||
                ramp.bottomHeight > surfaceHeight + 0.85 ||
                ramp.topHeight < surfaceHeight + 1.15) {
                continue;
            }
            const bool alongZ =
                ramp.rotation == Rotation::Deg0 ||
                ramp.rotation == Rotation::Deg180;
            const double padding =
                enemyPhysicalCapsule(enemy.type).radius + 0.22;
            const double minimumX =
                ramp.anchor.x * view_.cellSize - padding;
            const double maximumX =
                (ramp.anchor.x +
                 (alongZ ? ModularRampWidthCells
                         : ModularRampRunCells)) *
                    view_.cellSize + padding;
            const double minimumZ =
                ramp.anchor.z * view_.cellSize - padding;
            const double maximumZ =
                (ramp.anchor.z +
                 (alongZ ? ModularRampRunCells
                         : ModularRampWidthCells)) *
                    view_.cellSize + padding;
            if (!crossesRectangle(
                    enemy.position, corePosition,
                    minimumX, maximumX,
                    minimumZ, maximumZ)) {
                continue;
            }
            const double rampDistance = std::hypot(
                enemy.position.x -
                    (minimumX + maximumX) * 0.5,
                enemy.position.z -
                    (minimumZ + maximumZ) * 0.5);
            if (rampDistance >= bestRampDistance) {
                continue;
            }
            const std::array<Vec3, 4> corners{{
                {minimumX, surfaceHeight, minimumZ},
                {minimumX, surfaceHeight, maximumZ},
                {maximumX, surfaceHeight, minimumZ},
                {maximumX, surfaceHeight, maximumZ},
            }};
            const auto visible = [=](Vec3 from, Vec3 to) {
                return !crossesRectangle(
                    from, to, minimumX, maximumX,
                    minimumZ, maximumZ);
            };
            double bestPathLength =
                std::numeric_limits<double>::infinity();
            std::optional<Vec3> rampWaypoint;
            for (std::size_t first = 0;
                 first < corners.size(); ++first) {
                if (!visible(enemy.position, corners[first])) {
                    continue;
                }
                for (std::size_t last = 0;
                     last < corners.size(); ++last) {
                    if (first != last &&
                        !visible(corners[first], corners[last])) {
                        continue;
                    }
                    if (!visible(corners[last], corePosition)) {
                        continue;
                    }
                    const double pathLength =
                        std::hypot(
                            enemy.position.x - corners[first].x,
                            enemy.position.z - corners[first].z) +
                        std::hypot(
                            corners[first].x - corners[last].x,
                            corners[first].z - corners[last].z) +
                        std::hypot(
                            corners[last].x - corePosition.x,
                            corners[last].z - corePosition.z);
                    if (pathLength < bestPathLength) {
                        bestPathLength = pathLength;
                        const double firstDistance = std::hypot(
                            enemy.position.x - corners[first].x,
                            enemy.position.z - corners[first].z);
                        rampWaypoint = firstDistance < 0.28
                            ? corners[last]
                            : corners[first];
                    }
                }
            }
            if (rampWaypoint) {
                bestRampDistance = rampDistance;
                bestWaypoint = rampWaypoint;
            }
        }
        return bestWaypoint;
    }

    [[nodiscard]] Vec3 frameCenter(
        const PlatformFrameInstance& frame) const {
        return {
            (frame.anchor.x + PlatformFrameWidthCells * 0.5) *
                view_.cellSize,
            frame.floorHeight,
            (frame.anchor.z + PlatformFrameWidthCells * 0.5) *
                view_.cellSize,
        };
    }

    [[nodiscard]] bool contains(
        const PlatformFrameInstance& frame,
        Vec3 point, double margin = 0.0) const {
        const double minimumX = frame.anchor.x * view_.cellSize;
        const double maximumX =
            (frame.anchor.x + PlatformFrameWidthCells) * view_.cellSize;
        const double minimumZ = frame.anchor.z * view_.cellSize;
        const double maximumZ =
            (frame.anchor.z + PlatformFrameWidthCells) * view_.cellSize;
        return point.x >= minimumX - margin &&
            point.x <= maximumX + margin &&
            point.z >= minimumZ - margin &&
            point.z <= maximumZ + margin;
    }

    [[nodiscard]] int frameAt(
        Vec3 point, int storey, double height) const {
        const int cellX = static_cast<int>(
            std::floor(point.x / view_.cellSize));
        const int cellZ = static_cast<int>(
            std::floor(point.z / view_.cellSize));
        const auto candidate = frameIndex_.find(
            std::tuple{
                storey,
                snapPlatformFrameAxis(cellX),
                snapPlatformFrameAxis(cellZ)});
        if (candidate == frameIndex_.end()) {
            return -1;
        }
        const PlatformFrameInstance& frame =
            view_.platformFrames[
                static_cast<std::size_t>(candidate->second)];
        return std::abs(frame.floorHeight - height) < 1.25 &&
                contains(frame, point, 0.18)
            ? candidate->second
            : -1;
    }

    [[nodiscard]] bool insideRamp(
        Vec3 point, const RampInstance& ramp) const {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const double minimumX = ramp.anchor.x * view_.cellSize;
        const double maximumX =
            (ramp.anchor.x +
             (alongZ ? ModularRampWidthCells
                     : ModularRampRunCells)) * view_.cellSize;
        const double minimumZ = ramp.anchor.z * view_.cellSize;
        const double maximumZ =
            (ramp.anchor.z +
             (alongZ ? ModularRampRunCells
                     : ModularRampWidthCells)) * view_.cellSize;
        return point.x >= minimumX - 0.08 &&
            point.x <= maximumX + 0.08 &&
            point.z >= minimumZ - 0.08 &&
            point.z <= maximumZ + 0.08;
    }

    void connect(int left, int right, const RampInstance* ramp) {
        edges_[static_cast<std::size_t>(left)].push_back({right, ramp});
        edges_[static_cast<std::size_t>(right)].push_back({left, ramp});
    }

    void buildRoutes() {
        distance_.assign(edges_.size(), -1);
        next_.assign(edges_.size(), std::nullopt);
        std::queue<int> pending;
        distance_[static_cast<std::size_t>(goalNode_)] = 0;
        pending.push(goalNode_);
        while (!pending.empty()) {
            const int node = pending.front();
            pending.pop();
            for (const TraversalEdge& edge :
                 edges_[static_cast<std::size_t>(node)]) {
                if (distance_[static_cast<std::size_t>(edge.to)] >= 0) {
                    continue;
                }
                distance_[static_cast<std::size_t>(edge.to)] =
                    distance_[static_cast<std::size_t>(node)] + 1;
                next_[static_cast<std::size_t>(edge.to)] =
                    TraversalEdge{node, edge.ramp};
                pending.push(edge.to);
            }
        }
    }

    EnemyNavigationView view_;
    int groundNode_{};
    int goalNode_{-1};
    std::vector<std::vector<TraversalEdge>> edges_;
    std::vector<int> distance_;
    std::vector<std::optional<TraversalEdge>> next_;
    std::map<std::tuple<int, int, int>, int> frameIndex_;
    int maximumStorey_{};
};

std::shared_ptr<MultiLevelNavigation> makeMultiLevelNavigation(
    EnemyNavigationView view, Vec3 corePosition, int coreStorey) {
    return std::make_shared<MultiLevelNavigation>(
        view, corePosition, coreStorey);
}

std::optional<Vec3> multiLevelWaypoint(
    const MultiLevelNavigation& navigation,
    const EnemyInstance& enemy, double terrainHeight,
    Vec3 corePosition) {
    return navigation.waypoint(enemy, terrainHeight, corePosition);
}

} // namespace ian
