#include "world/MapDefinition.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ian {
namespace {

using Json = nlohmann::json;

Vec3 parsePosition(const Json& value) {
    return {
        value.at("x").get<double>(),
        value.at("y").get<double>(),
        value.at("z").get<double>(),
    };
}

ResourceType parseResourceType(const std::string& value) {
    if (value == "wood") {
        return ResourceType::Wood;
    }
    if (value == "stone") {
        return ResourceType::Stone;
    }
    throw std::runtime_error("resource type must be wood or stone");
}

ResourceNodeDefinition parseResource(const Json& value) {
    const ResourceNodeDefinition definition{
        .type = parseResourceType(value.at("type").get<std::string>()),
        .position = parsePosition(value.at("position")),
        .radius = value.at("radius").get<double>(),
        .health = value.at("health").get<double>(),
        .yield = value.at("yield").get<int>(),
        .respawnSeconds = value.at("respawnSeconds").get<double>(),
    };
    if (definition.radius <= 0.0 || definition.health <= 0.0 || definition.yield <= 0 ||
        definition.respawnSeconds <= 0.0) {
        throw std::runtime_error("invalid resource node");
    }
    return definition;
}

MapObstacle parseObstacle(const Json& value) {
    const MapObstacle obstacle{
        .collision = {
            .minX = value.at("minX").get<double>(),
            .maxX = value.at("maxX").get<double>(),
            .minZ = value.at("minZ").get<double>(),
            .maxZ = value.at("maxZ").get<double>(),
        },
        .height = value.at("height").get<double>(),
    };
    if (obstacle.collision.minX >= obstacle.collision.maxX ||
        obstacle.collision.minZ >= obstacle.collision.maxZ || obstacle.height <= 0.0) {
        throw std::runtime_error("invalid static obstacle");
    }
    return obstacle;
}

} // namespace

MapDefinition MapDefinition::defaults() {
    return {
        .playerSpawn = {0.0, 0.0, 6.0},
        .worldLimit = 48.0,
        .coreBuildRadius = 12,
        .enemySpawnAnchors = {
            {0.0, 0.0, -20.0},
            {20.0, 0.0, 0.0},
            {-20.0, 0.0, 0.0},
        },
        .resources = {
            {ResourceType::Wood, {0.0, 1.0, 2.5}, 1.0, 3.0, 15, 12.0},
            {ResourceType::Wood, {-4.0, 1.0, -2.0}, 1.0, 3.0, 15, 12.0},
            {ResourceType::Wood, {5.0, 1.0, -5.0}, 1.0, 3.0, 15, 12.0},
            {ResourceType::Wood, {-8.0, 1.0, -9.0}, 1.0, 3.0, 15, 12.0},
            {ResourceType::Stone, {3.0, 0.8, 4.0}, 0.9, 4.0, 12, 15.0},
            {ResourceType::Stone, {8.0, 0.8, -1.0}, 0.9, 4.0, 12, 15.0},
            {ResourceType::Stone, {-6.0, 0.8, -5.0}, 0.9, 4.0, 12, 15.0},
        },
        .obstacles = {
            {{-9.0, -7.0, -8.0, -6.0}, 2.0},
            {{7.5, 10.5, -13.5, -10.5}, 3.0},
        },
    };
}

MapLoadResult parseMapDefinition(std::string_view json) {
    MapLoadResult result{.map = MapDefinition::defaults()};
    try {
        const Json document = Json::parse(json);
        MapDefinition parsed{
            .playerSpawn = parsePosition(document.at("playerSpawn")),
            .worldLimit = document.at("worldLimit").get<double>(),
            .coreBuildRadius = document.at("coreBuildRadius").get<int>(),
            .enemySpawnAnchors = {},
            .resources = {},
            .obstacles = {},
        };
        for (const auto& anchor : document.at("enemySpawnAnchors")) {
            parsed.enemySpawnAnchors.push_back(parsePosition(anchor));
        }
        for (const auto& resource : document.at("resources")) {
            parsed.resources.push_back(parseResource(resource));
        }
        for (const auto& obstacle : document.at("obstacles")) {
            parsed.obstacles.push_back(parseObstacle(obstacle));
        }
        if (parsed.worldLimit <= 5.0 || parsed.worldLimit > 48.0 ||
            parsed.coreBuildRadius <= 0 ||
            parsed.enemySpawnAnchors.empty() || parsed.resources.empty()) {
            throw std::runtime_error("invalid map definition");
        }
        const auto insideWorld = [&parsed](Vec3 position) {
            return position.x >= -parsed.worldLimit && position.x <= parsed.worldLimit &&
                   position.z >= -parsed.worldLimit && position.z <= parsed.worldLimit;
        };
        if (!insideWorld(parsed.playerSpawn)) {
            throw std::runtime_error("player spawn outside world");
        }
        for (const Vec3 anchor : parsed.enemySpawnAnchors) {
            if (!insideWorld(anchor)) {
                throw std::runtime_error("enemy spawn anchor outside world");
            }
        }
        for (const auto& resource : parsed.resources) {
            if (!insideWorld(resource.position)) {
                throw std::runtime_error("resource outside world");
            }
        }
        for (const auto& obstacle : parsed.obstacles) {
            if (obstacle.collision.minX < -parsed.worldLimit ||
                obstacle.collision.maxX > parsed.worldLimit ||
                obstacle.collision.minZ < -parsed.worldLimit ||
                obstacle.collision.maxZ > parsed.worldLimit) {
                throw std::runtime_error("static obstacle outside world");
            }
        }
        result.map = std::move(parsed);
    } catch (const std::exception& error) {
        result.errors.push_back("map: " + std::string(error.what()));
    }
    return result;
}

MapLoadResult loadMapDefinition(std::string_view path) {
    try {
        std::ifstream stream{std::string(path)};
        if (!stream) {
            throw std::runtime_error("cannot open " + std::string(path));
        }
        const std::string json{std::istreambuf_iterator<char>{stream},
                               std::istreambuf_iterator<char>{}};
        return parseMapDefinition(json);
    } catch (const std::exception& error) {
        return {.map = MapDefinition::defaults(), .errors = {error.what()}};
    }
}

std::vector<CollisionBox> mapCollisionBoxes(const MapDefinition& map) {
    std::vector<CollisionBox> result;
    result.reserve(map.obstacles.size());
    for (const auto& obstacle : map.obstacles) {
        result.push_back(obstacle.collision);
    }
    return result;
}

} // namespace ian
