#include "TestHarness.hpp"
#include "game/LootChestSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <cmath>

void runLootChestSystemTests() {
    ian::TerrainHeightfield terrain;
    ian::LootChestSystem chests;
    const ian::Vec3 spawn{
        0.0, terrain.getHeight(0.0, 0.0) + 1.7, 0.0};
    chests.reset(terrain.seed(), 120.0, terrain, {}, spawn);
    require(chests.chests().size() == 10,
            "loot chests deterministically populate terrain");
    bool hasWooden = false;
    bool hasStone = false;
    for (const auto& chest : chests.chests()) {
        hasWooden = hasWooden ||
            chest.type == ian::LootChestType::Wooden;
        hasStone = hasStone ||
            chest.type == ian::LootChestType::Stone;
        requireNear(chest.position.y,
                    terrain.getHeight(chest.position.x, chest.position.z),
                    1e-8, "loot chest rests on terrain heightfield");
        require(terrain.waterSignedDistance(
                    chest.position.x, chest.position.z) >= 2.5,
                "loot chest stays clear of ponds and shoreline");
        require(terrain.getNormal(
                    chest.position.x, chest.position.z).y >= 0.82,
                "loot chest avoids steep terrain");
        require(chest.loot.rarity == ian::LootRarity::Common &&
                    (chest.loot.effect == ian::LootUpgradeEffect::Apple ||
                     chest.loot.effect == ian::LootUpgradeEffect::Bread),
                "prototype chests contain only common apple or bread items");
    }
    require(hasWooden && hasStone,
            "terrain population uses both supplied chest models");

    const ian::EntityId id = chests.chests().front().id;
    const int cost = chests.chests().front().goldCost;
    int poorGold = cost - 1;
    require(chests.open(id, poorGold) ==
                ian::ChestOpenResult::InsufficientGold &&
                poorGold == cost - 1,
            "failed chest purchase never spends gold");
    int gold = cost + 4;
    require(chests.open(id, gold) == ian::ChestOpenResult::Opened &&
                gold == 4,
            "chest purchase spends configured gold once");
    require(chests.open(id, gold) ==
                ian::ChestOpenResult::AlreadyOpen &&
                gold == 4,
            "opened chest cannot charge player twice");
    chests.tick(0.25);
    require(chests.chests().front().openingProgress > 0.0 &&
                chests.chests().front().openingProgress < 1.0 &&
                chests.chests().front().loot.revealProgress > 0.0,
            "loot starts rising during the early chest opening");
    chests.tick(0.45);
    require(chests.chests().front().state ==
                ian::LootChestState::Opening &&
                chests.chests().front().loot.available &&
                chests.chests().front().loot.revealProgress == 1.0,
            "loot finishes rising before the lid finishes opening");
    chests.tick(0.55);
    const auto& opened = chests.chests().front();
    require(opened.state == ian::LootChestState::Open &&
                opened.loot.available &&
                opened.loot.revealProgress == 1.0,
            "completed opening reveals one hovering loot item");
    require(chests.collect(opened.loot.id).has_value(),
            "revealed loot can be collected");
    require(!chests.collect(opened.loot.id).has_value(),
            "loot upgrade can only be collected once");
}
