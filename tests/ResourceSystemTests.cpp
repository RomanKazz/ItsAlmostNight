#include "TestHarness.hpp"
#include "resources/ResourceSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

void runResourceSystemTests() {
    ian::ResourceSystem resources;
    const ian::EntityId firstRunResource =
        resources.nodes().front().id;
    resources.reset();
    require(
        resources.nodes().front().id.index ==
                firstRunResource.index &&
            resources.nodes().front().id.generation !=
                firstRunResource.generation,
        "resource reset preserves slot but changes run generation");
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

    const std::vector<ian::ResourceNodeDefinition> yieldDefinitions{
        {ian::ResourceType::Wood, {0.0, 1.0, 0.0},
         1.0, 3.0, 15, 1.0},
        {ian::ResourceType::Stone, {6.0, 0.8, 0.0},
         0.9, 4.0, 12, 1.0},
        {ian::ResourceType::Crystal, {12.0, 0.0, 0.0},
         0.72, 12.0, 12, 1.0},
        {ian::ResourceType::Crate, {18.0, 0.0, 0.0},
         0.8, 3.0, 5, 1.0},
    };
    ian::ResourceSystem normalYield(yieldDefinitions);
    ian::ResourceSystem richYield(yieldDefinitions);
    const auto partialWood = richYield.damage(
        richYield.nodes()[0].id, 1.0);
    require(partialWood.has_value() && !partialWood->collected,
            "rich node fixture can be partially harvested");
    richYield.setGatheringYieldMultiplier(1.15);
    for (std::size_t index = 0; index < 3; ++index) {
        const auto& normal = normalYield.nodes()[index];
        const auto& rich = richYield.nodes()[index];
        require(rich.yield == static_cast<int>(std::lround(
                    static_cast<double>(normal.yield) * 1.15)),
                "rich nodes increase wood, stone and crystal yield");
    }
    require(
        richYield.nodes()[0].yieldRemaining ==
            richYield.nodes()[0].yield - partialWood->amount,
        "yield upgrade preserves resources already harvested");
    require(richYield.nodes()[3].yield == normalYield.nodes()[3].yield,
            "yield upgrade does not affect other destructible props");
    const ian::EntityId stoneId = richYield.nodes()[1].id;
    require(richYield.damage(stoneId, 100.0)->collected &&
                normalYield.damage(stoneId, 100.0)->collected,
            "stone fixtures deplete before respawn");
    richYield.tick(1.0);
    normalYield.tick(1.0);
    require(
        richYield.nodes()[1].yield == static_cast<int>(std::lround(
            static_cast<double>(normalYield.nodes()[1].yield) * 1.15)),
        "rich node bonus applies again after resource respawn");

    const auto slopedGround = [](double x, double z) {
        return 2.0 + x * 0.15 - z * 0.08;
    };
    ian::ResourceSystem terrainResources(
        {{ian::ResourceType::Wood,
          {0.0, slopedGround(0.0, 0.0) + 1.0, 0.0},
          1.0, 3.0, 15, 1.0}},
        slopedGround);
    require(
        terrainResources.damage({0, 1}, 100.0)->collected,
        "terrain resource fixture is collected");
    terrainResources.tick(1.0);
    const auto& relocated = terrainResources.nodes().front();
    require(
        relocated.active &&
            (relocated.position.x != 0.0 ||
             relocated.position.z != 0.0),
        "terrain resource respawns at a new position");
    requireNear(
        relocated.position.y,
        slopedGround(
            relocated.position.x,
            relocated.position.z) + 1.0,
        1e-9,
        "respawned resource collider follows terrain height");

    const auto dryGround = [](double x, double, double radius) {
        return x >= radius + 0.8;
    };
    ian::ResourceSystem waterAwareResources(
        {{ian::ResourceType::Stone,
          {0.0, 0.8, 0.0}, 0.9, 4.0, 12, 1.0}},
        {}, dryGround);
    waterAwareResources.tick(0.0, {}, 48.0);
    const auto& dryResource = waterAwareResources.nodes().front();
    require(
        dryResource.active &&
            dryGround(
                dryResource.position.x,
                dryResource.position.z,
                dryResource.radius),
        "resource relocation rejects unsafe terrain");

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

    std::vector<ian::ResourceNodeDefinition> crystalDefinitions;
    for (int index = 0; index < 32; ++index) {
        crystalDefinitions.push_back({
            ian::ResourceType::Crystal,
            {static_cast<double>(index) * 3.0, 0.0, 0.0},
            0.72, 12.0, 12, 28.0});
    }
    const ian::ResourceSystem variedCrystals{
        std::move(crystalDefinitions)};
    const bool hasRichCrystal = std::ranges::any_of(
        variedCrystals.nodes(), [](const ian::ResourceNode& node) {
            return node.yield > 12;
        });
    const bool hasStandardCrystal = std::ranges::any_of(
        variedCrystals.nodes(), [](const ian::ResourceNode& node) {
            return node.yield == 12;
        });
    require(
        hasRichCrystal && hasStandardCrystal,
        "crystal deposits vary between standard and rich yields");

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
