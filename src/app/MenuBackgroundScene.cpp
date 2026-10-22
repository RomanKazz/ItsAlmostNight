#include "app/MenuBackgroundScene.hpp"

#include "buildings/BuildingOrientation.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace ian {

bool MenuBackgroundScene::supports(BuildingType type) {
    const int index = static_cast<int>(type);
    return index >= 0 &&
        index < static_cast<int>(GameBalance::BuildingTypeCount) &&
        !isRemovedStorageBuilding(type);
}

BuildingPreview MenuBackgroundScene::preview(
    BuildingType type, Vec3 terrainHit, std::uint8_t rotation,
    const TerrainHeightfield& terrain, double worldLimit) const {
    const double halfExtent = buildingFootprintHalfExtent(type);
    const double offset = halfExtent == 1.0 ? 0.0 : 0.5;
    const GridPosition grid{
        static_cast<int>(std::floor(terrainHit.x - offset + 0.5)),
        static_cast<int>(std::floor(terrainHit.z - offset + 0.5)),
    };
    const Vec3 center = buildingWorldPosition(type, grid);
    // Sample the whole footprint so buildings stay level on slopes.
    double bottom = terrain.getHeight(center.x, center.z);
    double top = bottom;
    for (int z = 0; z <= 4; ++z) {
        for (int x = 0; x <= 4; ++x) {
            const double height = terrain.getHeight(
                center.x - halfExtent + halfExtent * x * 0.5,
                center.z - halfExtent + halfExtent * z * 0.5);
            bottom = std::min(bottom, height);
            top = std::max(top, height);
        }
    }
    PlacementError error = PlacementError::None;
    if (std::abs(center.x) + halfExtent > worldLimit ||
        std::abs(center.z) + halfExtent > worldLimit ||
        !terrain.isInside(center.x - halfExtent, center.z - halfExtent) ||
        !terrain.isInside(center.x + halfExtent, center.z + halfExtent)) {
        error = PlacementError::OutOfRange;
    } else if (buildings().size() >= MaximumBuildings) {
        error = PlacementError::LimitReached;
    } else if (type == BuildingType::Core && buildings_.hasCore()) {
        error = PlacementError::CoreAlreadyPlaced;
    } else {
        for (const auto& building : buildings()) {
            const Vec3 other = buildingWorldPosition(building);
            const double distance = halfExtent +
                buildingFootprintHalfExtent(building.type);
            if (std::abs(center.x - other.x) < distance &&
                std::abs(center.z - other.z) < distance &&
                std::abs(top - other.y) < 2.75) {
                error = PlacementError::Occupied;
                break;
            }
        }
    }
    return {
        .type = type,
        .gridPosition = grid,
        .rotation = static_cast<std::uint8_t>(
            rotation % buildingRotationStepCount(type)),
        .placement = {error, {}},
        .baseHeight = top,
        .foundationBottomHeight = bottom,
    };
}

bool MenuBackgroundScene::place(const BuildingPreview& preview) {
    if (!preview.placement.valid() || !supports(preview.type) ||
        buildings().size() >= MaximumBuildings) return false;
    auto next = buildings();
    const double health = buildings_.baseMaxHealth(preview.type);
    next.push_back({
        .id = {nextIndex_, 1U},
        .type = preview.type,
        .gridPosition = preview.gridPosition,
        .rotation = preview.rotation,
        .health = health,
        .maxHealth = health,
        .baseHeight = preview.baseHeight,
        .platformStorey = preview.platformStorey,
        .foundationBottomHeight = preview.foundationBottomHeight,
    });
    if (!buildings_.restoreBuildings(next, {}, 12)) return false;
    ++nextIndex_;
    return true;
}

bool MenuBackgroundScene::remove(
    Vec3 origin, Vec3 direction, double maximumDistance) {
    const auto id = buildings_.raycast(origin, direction, maximumDistance);
    return id && buildings_.remove(*id).has_value();
}

const std::vector<BuildingInstance>& MenuBackgroundScene::buildings() const {
    return buildings_.buildings();
}

nlohmann::json MenuBackgroundScene::toJson() const {
    auto result = nlohmann::json::array();
    for (const auto& building : buildings()) {
        result.push_back({
            {"type", static_cast<int>(building.type)},
            {"x", building.gridPosition.x},
            {"z", building.gridPosition.z},
            {"rotation", building.rotation},
            {"level", building.level},
            {"baseHeight", building.baseHeight},
            {"foundationBottomHeight", building.foundationBottomHeight},
            {"platformStorey", building.platformStorey},
            {"open", building.open},
        });
    }
    return result;
}

bool MenuBackgroundScene::loadJson(
    const nlohmann::json& document, double worldLimit) {
    try {
        if (!document.is_array() || document.size() > MaximumBuildings ||
            !std::isfinite(worldLimit) || worldLimit <= 0.0) return false;
        std::vector<BuildingInstance> restored;
        restored.reserve(document.size());
        bool hasCore = false;
        for (const auto& entry : document) {
            const auto type = static_cast<BuildingType>(entry.at("type").get<int>());
            if (!supports(type) || (type == BuildingType::Core && hasCore)) return false;
            hasCore = hasCore || type == BuildingType::Core;
            const GridPosition grid{entry.at("x").get<int>(), entry.at("z").get<int>()};
            const int rotation = entry.value("rotation", 0);
            const int level = entry.value("level", 1);
            const double height = entry.at("baseHeight").get<double>();
            const double bottom = entry.value("foundationBottomHeight", height);
            const int storey = entry.value("platformStorey", -1);
            const Vec3 center = buildingWorldPosition(type, grid);
            const double extent = buildingFootprintHalfExtent(type);
            if (rotation < 0 || rotation >= buildingRotationStepCount(type) ||
                level < 1 || level > MaxBuildingLevel ||
                !std::isfinite(height) || !std::isfinite(bottom) ||
                std::abs(height) >= 1000.0 || std::abs(bottom) >= 1000.0 ||
                bottom > height || storey < -1 || storey > 32 ||
                std::abs(center.x) + extent > worldLimit ||
                std::abs(center.z) + extent > worldLimit) return false;
            const double health = buildingMaxHealthAtLevel(
                buildings_.baseMaxHealth(type), static_cast<std::uint8_t>(level));
            restored.push_back({
                .id = {static_cast<std::uint32_t>(1000U + restored.size()), 1U},
                .type = type,
                .gridPosition = grid,
                .rotation = static_cast<std::uint8_t>(rotation),
                .level = static_cast<std::uint8_t>(level),
                .health = health,
                .maxHealth = health,
                .open = entry.value("open", false),
                .baseHeight = height,
                .platformStorey = storey,
                .foundationBottomHeight = bottom,
            });
        }
        if (!buildings_.restoreBuildings(restored, {}, 12)) return false;
        nextIndex_ = static_cast<std::uint32_t>(1000U + restored.size());
        return true;
    } catch (...) {
        return false;
    }
}

bool MenuBackgroundScene::copyBuildings(
    std::span<const BuildingInstance> buildings) {
    if (buildings.size() > MaximumBuildings) return false;
    if (!buildings_.restoreBuildings(buildings, {}, 12)) return false;
    nextIndex_ = 1000U;
    for (const auto& building : buildings) {
        nextIndex_ = std::max(nextIndex_, building.id.index + 1U);
    }
    return true;
}

} // namespace ian
