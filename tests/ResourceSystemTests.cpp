#include "TestHarness.hpp"
#include "resources/ResourceSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

void runResourceSystemTests() {
    ian::ResourceSystem resources;
    const ian::Vec3 origin{0.0, 1.7, 6.0};
    const ian::Vec3 direction{0.0, 0.0, -1.0};

    const auto target = resources.raycast(origin, direction, 4.0);
    require(target.has_value(), "raycast finds resource inside pickaxe range");

    const auto firstHit = resources.damage(*target, 1.0);
    require(firstHit.has_value() && !firstHit->collected, "partial damage keeps node active");
    require(firstHit->amount == 5,
            "first tree hit grants proportional wood");
    const auto secondHit = resources.damage(*target, 1.0);
    require(secondHit.has_value() && secondHit->amount == 5,
            "second tree hit grants proportional wood");
    const auto finalHit = resources.damage(*target, 1.0);
    require(finalHit.has_value() && finalHit->collected, "lethal damage collects node");
    require(finalHit->amount == 5,
            "final tree hit grants remaining configured yield");
    require(firstHit->amount + secondHit->amount + finalHit->amount == 15,
            "all tree hits grant exact configured capacity");
    require(!resources.raycast(origin, direction, 4.0), "collected node leaves raycast");

    resources.tick(12.0);
    const auto respawned = std::find_if(
        resources.nodes().begin(), resources.nodes().end(),
        [&target](const ian::ResourceNode& node) {
            return node.id == *target;
        });
    require(
        respawned != resources.nodes().end() &&
            respawned->active &&
            (respawned->position.x != 0.0 ||
             respawned->position.z != 2.5),
        "node respawns at a new map position");
    const int expectedRespawnGrant =
        static_cast<int>(std::floor(
            static_cast<double>(respawned->yield) /
            respawned->maxHealth));
    const auto respawnHit = resources.damage(*target, 1.0);
    require(
        respawnHit.has_value() &&
            respawnHit->amount == expectedRespawnGrant,
            "respawn restores full resource capacity");

    std::vector<ian::ResourceNodeDefinition>
        variedDefinitions;
    for (int index = 0; index < 32; ++index) {
        variedDefinitions.push_back({
            ian::ResourceType::Wood,
            {static_cast<double>(index) * 3.0, 1.0, 0.0},
            1.0, 3.0, 15, 12.0});
    }
    const ian::ResourceSystem variedResources{
        std::move(variedDefinitions)};
    const bool hasRichNode = std::any_of(
        variedResources.nodes().begin(),
        variedResources.nodes().end(),
        [](const ian::ResourceNode& node) {
            return node.yield > 15;
        });
    const bool hasStandardNode = std::any_of(
        variedResources.nodes().begin(),
        variedResources.nodes().end(),
        [](const ian::ResourceNode& node) {
            return node.yield == 15;
        });
    require(
        hasRichNode && hasStandardNode,
        "resource population contains standard and rich nodes");
    const bool gainRateIsStable = std::all_of(
        variedResources.nodes().begin(),
        variedResources.nodes().end(),
        [](const ian::ResourceNode& node) {
            return std::abs(
                       static_cast<double>(node.yield) /
                           node.maxHealth -
                       5.0) <
                   1e-9;
        });
    require(
        gainRateIsStable,
        "rich nodes require more hits without increasing gain per damage");

    ian::BuildingSystem buildings;
    require(
        buildings
            .place(
                ian::BuildingType::Core, {0, 0}, 0,
                30, 0)
            .has_value(),
        "resource exclusion fixture creates core");
    ian::ResourceSystem blockedResource{{
        {ian::ResourceType::Wood, {0.0, 1.0, 0.0},
         1.0, 3.0, 15, 12.0},
    }};
    blockedResource.tick(
        0.0, buildings.buildings(), 48.0,
        ian::Vec3{0.0, 1.7, 6.0});
    require(
        blockedResource.nodes()[0].active &&
            (std::abs(
                 blockedResource.nodes()[0].position.x) >
                 4.5 ||
             std::abs(
                 blockedResource.nodes()[0].position.z) >
                 4.5),
        "resource relocates away from base buildings");
}
