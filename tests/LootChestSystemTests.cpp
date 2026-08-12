#include "TestHarness.hpp"
#include "game/LootChestSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <array>
#include <cmath>

void runLootChestSystemTests() {
    const std::array placementChests{
        ian::LootChestInstance{
            .id = {1, 1},
            .position = {0.0, 0.0, 0.0},
        },
    };
    require(
        ian::lootChestOverlapsRectangle(
            placementChests, 0.7, 1.7, -0.2, 0.2),
        "chest radius blocks a touching build footprint");
    require(
        !ian::lootChestOverlapsRectangle(
            placementChests, 0.9, 1.9, -0.2, 0.2),
        "chest does not block a separated build footprint");

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
        const ian::Vec3 sampledNormal = terrain.getNormal(
            chest.position.x, chest.position.z);
        requireNear(chest.surfaceNormal.x, sampledNormal.x, 1e-12,
                    "chest stores the placement surface normal");
        requireNear(chest.surfaceNormal.y, sampledNormal.y, 1e-12,
                    "chest normal is deterministic on the same terrain");
        const bool common =
            chest.loot.rarity == ian::LootRarity::Common &&
            (chest.loot.effect == ian::LootUpgradeEffect::Apple ||
             chest.loot.effect == ian::LootUpgradeEffect::Bread ||
             chest.loot.effect == ian::LootUpgradeEffect::IronBar ||
             chest.loot.effect == ian::LootUpgradeEffect::FuelJerrycan ||
             chest.loot.effect == ian::LootUpgradeEffect::Compass ||
             chest.loot.effect == ian::LootUpgradeEffect::Nail ||
             chest.loot.effect == ian::LootUpgradeEffect::Key);
        const bool rare =
            chest.loot.rarity == ian::LootRarity::Rare &&
            (chest.loot.effect == ian::LootUpgradeEffect::Map ||
             chest.loot.effect == ian::LootUpgradeEffect::Anvil ||
             chest.loot.effect == ian::LootUpgradeEffect::Saw ||
             chest.loot.effect == ian::LootUpgradeEffect::Potion ||
             chest.loot.effect == ian::LootUpgradeEffect::Blueprint ||
             chest.loot.effect == ian::LootUpgradeEffect::Hourglass ||
             chest.loot.effect == ian::LootUpgradeEffect::Rope);
        require(common || rare,
                "loot rarity matches the configured common or rare pool");
    }
    require(hasWooden && hasStone,
            "terrain population uses both supplied chest models");
    require(
        std::string_view(ian::lootUpgradeName(
            ian::LootUpgradeEffect::Blueprint)) == "Blueprint" &&
            std::string_view(ian::lootUpgradeName(
                ian::LootUpgradeEffect::Hourglass)) == "Hourglass" &&
            std::string_view(ian::lootUpgradeName(
                ian::LootUpgradeEffect::Rope)) == "Safety Rope",
        "new loot items expose readable names");

    const std::size_t chestCountBeforeDelivery =
        chests.chests().size();
    constexpr double DeliveryRadius = 28.0;
    chests.spawnAdditionalChests(
        1, terrain.seed(), 120.0, terrain, {}, spawn,
        ian::Vec3{0.0, 0.0, 0.0}, DeliveryRadius);
    const auto& deliveredChest = chests.chests().back();
    require(
        chests.chests().size() ==
                chestCountBeforeDelivery + 1U &&
            std::hypot(
                deliveredChest.position.x,
                deliveredChest.position.z) <=
                DeliveryRadius + 1e-9,
        "preferred delivery spawns additional chest near base");

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
    const ian::Vec3 visualPosition = ian::lootVisualPosition(opened);
    require(std::isfinite(visualPosition.x) &&
                std::isfinite(visualPosition.y) &&
                std::isfinite(visualPosition.z),
            "loot world position stays finite after local-to-world launch");
    const ian::Vec3 chestToLoot{
        visualPosition.x - opened.position.x,
        visualPosition.y - opened.position.y,
        visualPosition.z - opened.position.z};
    const double normalProjection =
        chestToLoot.x * opened.surfaceNormal.x +
        chestToLoot.y * opened.surfaceNormal.y +
        chestToLoot.z * opened.surfaceNormal.z;
    require(normalProjection > 0.5,
            "loot exits above the chest along its surface normal");

    ian::LootChestInstance flatChest = opened;
    flatChest.position = {4.0, 1.5, -3.0};
    flatChest.surfaceNormal = {0.0, 1.0, 0.0};
    flatChest.yaw = 1.17;
    flatChest.loot.hoverTime = 0.0;
    const ian::Vec3 flatLootPosition =
        ian::lootVisualPosition(flatChest);
    requireNear(flatLootPosition.x, flatChest.position.x, 1e-12,
                "hovering loot has no permanent forward X offset");
    requireNear(flatLootPosition.z, flatChest.position.z, 1e-12,
                "hovering loot has no permanent forward Z offset");
    requireNear(
        flatLootPosition.y - flatChest.position.y, 1.296, 1e-12,
        "ordinary chest loot hovers twenty percent lower");
    require(chests.collect(opened.loot.id).has_value(),
            "revealed loot can be collected");
    require(!chests.collect(opened.loot.id).has_value(),
            "loot upgrade can only be collected once");
    const std::size_t chestCountBeforeDisappearance =
        chests.chests().size();
    chests.tick(1.0);
    require(
        chests.chests().size() == chestCountBeforeDisappearance &&
            chests.chests().front().disappearanceProgress == 0.0,
        "empty chest remains briefly after loot collection");
    chests.tick(0.30);
    const auto disappearingChest = std::ranges::find(
        chests.chests(), id, &ian::LootChestInstance::id);
    require(
        disappearingChest != chests.chests().end() &&
            disappearingChest->disappearanceProgress > 0.0 &&
            disappearingChest->disappearanceProgress < 1.0,
        "empty chest exposes a visible disappearance animation");
    chests.tick(0.40);
    require(
        std::ranges::find(
            chests.chests(), id,
            &ian::LootChestInstance::id) ==
            chests.chests().end() &&
            chests.chests().size() ==
                chestCountBeforeDisappearance - 1U,
        "empty chest leaves the world after its animation");

    const std::size_t beforeLoose = chests.chests().size();
    chests.spawnLooseLoot(
        {3.0, 2.0, -4.0}, ian::LootRarity::Legendary, 99U);
    const auto& loose = chests.chests().back();
    require(
        chests.chests().size() == beforeLoose + 1U &&
            loose.looseLoot && loose.loot.available &&
            loose.loot.rarity == ian::LootRarity::Legendary &&
            loose.loot.revealProgress == 0.0,
        "destroyed item crates can spawn collectible legendary loot");
    const ian::Vec3 looseStartPosition =
        ian::lootVisualPosition(loose);
    require(!chests.collect(loose.loot.id).has_value(),
            "loose crate loot cannot be collected before it is readable");
    chests.tick(0.19);
    require(
        chests.chests().back().loot.revealProgress > 0.0 &&
            chests.chests().back().loot.revealProgress < 1.0,
        "loose crate loot scales in instead of appearing instantly");
    const ian::Vec3 looseMidRevealPosition =
        ian::lootVisualPosition(chests.chests().back());
    require(
        std::abs(
            looseMidRevealPosition.y -
            looseStartPosition.y) < 0.08,
        "crate scale-in keeps only the normal hover bob, not chest motion");
    chests.tick(1.17);
    requireNear(
        chests.chests().back().loot.revealProgress, 1.0, 1e-12,
        "loose crate loot finishes its scale-in animation");
    require(chests.collect(loose.loot.id).has_value(),
            "loose crate loot unlocks normal pickup after its display delay");
}
