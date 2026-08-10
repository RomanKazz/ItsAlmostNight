#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double LootProximityPickupRadius = 2.0;

constexpr int PreCoreWoodCapacity = 60;
constexpr int PreCoreStoneCapacity = 30;
constexpr int PreCoreCrystalCapacity = 10;
constexpr int CoreWoodCapacity = 100;
constexpr int CoreStoneCapacity = 75;
constexpr int CoreCrystalCapacity = 25;

} // namespace

int Simulation::resourceCapacity(BuildingType storageType) const {
    int capacity = 0;
    int perLevel = 0;
    if (storageType == BuildingType::WoodStorage) {
        capacity = buildings_.hasCore() ? CoreWoodCapacity : PreCoreWoodCapacity;
        perLevel = buildingStorageCapacityPerLevel(storageType);
    } else if (storageType == BuildingType::StoneStorage) {
        capacity = buildings_.hasCore() ? CoreStoneCapacity : PreCoreStoneCapacity;
        perLevel = buildingStorageCapacityPerLevel(storageType);
    } else {
        capacity = buildings_.hasCore() ? CoreCrystalCapacity : PreCoreCrystalCapacity;
        perLevel = buildingStorageCapacityPerLevel(storageType);
    }
    for (const BuildingInstance& building : buildings_.buildings()) {
        if (building.type == storageType) {
            capacity = saturatingAdd(
                capacity,
                perLevel * static_cast<int>(building.level));
        }
    }
    return static_cast<int>(std::min(
        static_cast<double>(std::numeric_limits<int>::max()),
        static_cast<double>(std::lround(
            static_cast<double>(capacity) * std::max(
                0.05, 1.0 + skillTree_.effectValue(
                    "storage.capacity"))))));
}

void Simulation::addWood(int amount) {
    if (unlimitedResources_) {
        wood_ = saturatingAdd(wood_, std::max(0, amount));
        return;
    }
    wood_ = std::min(
        resourceCapacity(BuildingType::WoodStorage),
        saturatingAdd(wood_, std::max(0, amount)));
}

void Simulation::addStone(int amount) {
    if (unlimitedResources_) {
        stone_ = saturatingAdd(stone_, std::max(0, amount));
        return;
    }
    stone_ = std::min(
        resourceCapacity(BuildingType::StoneStorage),
        saturatingAdd(stone_, std::max(0, amount)));
}

void Simulation::addGold(int amount) {
    if (unlimitedResources_) {
        gold_ = saturatingAdd(gold_, std::max(0, amount));
        return;
    }
    gold_ = std::min(
        resourceCapacity(BuildingType::CrystalStorage),
        saturatingAdd(gold_, std::max(0, amount)));
}

bool Simulation::hasStorageSpace(ResourceType resource) const {
    if (!isHarvestableResource(resource)) {
        return true;
    }
    if (unlimitedResources_) {
        return true;
    }
    int pending = 0;
    for (const PendingResourceGrant& grant : pendingResourceGrants_) {
        if (grant.type == resource) {
            pending = saturatingAdd(pending, grant.amount);
        }
    }
    if (resource == ResourceType::Wood) {
        return saturatingAdd(wood_, pending) <
            resourceCapacity(BuildingType::WoodStorage);
    }
    return saturatingAdd(stone_, pending) <
        resourceCapacity(BuildingType::StoneStorage);
}

double Simulation::resourceToolEfficiency(
    PlayerWeapon tool, ResourceType resource) const {
    if (isDestructibleProp(resource)) {
        return 1.0;
    }
    if (tool == PlayerWeapon::BareHands) {
        // Bare Hands already receives its 25% multiplier in the common melee
        // damage calculation.
        return 1.0;
    }
    if ((tool == PlayerWeapon::Axe &&
         resource == ResourceType::Wood) ||
        (tool == PlayerWeapon::Pickaxe &&
         resource == ResourceType::Stone)) {
        return 1.0;
    }
    if (tool == PlayerWeapon::Axe) {
        return gameplay_.axeStoneEfficiency;
    }
    if (tool == PlayerWeapon::Pickaxe) {
        return gameplay_.pickaxeWoodEfficiency;
    }
    return 0.0;
}

void Simulation::updatePlayerActions(
    double deltaSeconds, const PlayerCommand& command) {
    const auto production = goldMines_.tick(deltaSeconds);
    for (const auto& produced : production) {
        const auto building = std::find_if(
            buildings_.buildings().begin(),
            buildings_.buildings().end(),
            [&produced](const BuildingInstance& candidate) {
                return candidate.id == produced.mineId;
            });
        const Vec3 productionPosition =
            building != buildings_.buildings().end()
                ? buildingWorldPosition(*building)
                : Vec3{};
        if (produced.buildingType == BuildingType::GoldMine) {
            addGold(produced.amount);
            events_.push_back({
                .type = GameEventType::GoldProduced,
                .entityId = produced.mineId,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
                .night = state_ == RunState::Sunset || state_ == RunState::Wave,
            });
        } else {
            const ResourceType resourceType =
                produced.buildingType ==
                        BuildingType::LumberMill
                    ? ResourceType::Wood
                    : ResourceType::Stone;
            if (resourceType == ResourceType::Wood) {
                addWood(produced.amount);
            } else {
                addStone(produced.amount);
            }
            events_.push_back({
                .type = GameEventType::ResourceGranted,
                .entityId = produced.mineId,
                .resourceType = resourceType,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
            });
        }
    }

    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    aimedBuilding_ = buildings_.raycast(playerPosition_, direction, 4.0);
    aimedModularBuilding_ = foundations_.raycast(
        playerPosition_, direction, 6.0);
    if (command.overrideAimedBuilding) {
        aimedBuilding_ =
            command.aimedBuildingOverride;
    }
    if (command.overrideAimedModularBuilding) {
        aimedModularBuilding_ =
            command.aimedModularBuildingOverride;
    }
    PlayerWeapon heldTool = playerWeapons_.selectedWeapon();
    const bool canGather = heldTool == PlayerWeapon::BareHands ||
                           heldTool == PlayerWeapon::Axe ||
                           heldTool == PlayerWeapon::Pickaxe;
    aimedResource_ = canGather
        ? resources_.raycast(playerPosition_, direction, gameplay_.resourceGatherRange)
        : std::nullopt;
    if (command.overrideAimedResource && canGather) {
        aimedResource_ = command.aimedResourceOverride;
    }
    constexpr bool automaticToolSwitch = true;
    // Smart Tools chooses between already-held real tools. Bare Hands is an
    // intentional gathering mode: keep it selected so its 25% coefficients,
    // animation and VFX remain the ones used for the hit.
    if (automaticToolSwitch && !selectedBuilding_ && aimedResource_ &&
        heldTool != PlayerWeapon::BareHands) {
        const auto node = std::ranges::find(
            resources_.nodes(), *aimedResource_,
            &ResourceNode::id);
        if (node != resources_.nodes().end() &&
            isHarvestableResource(node->type)) {
            const PlayerWeapon desiredTool =
                node->type == ResourceType::Wood
                    ? PlayerWeapon::Axe
                    : PlayerWeapon::Pickaxe;
            const bool desiredToolUnlocked = unlimitedResources_ ||
                skillTree_.hasEffect(
                    desiredTool == PlayerWeapon::Axe
                        ? "unlock.axe" : "unlock.pickaxe");
            if (heldTool != desiredTool && desiredToolUnlocked) {
                playerWeapons_.selectWeapon(desiredTool);
                heldTool = desiredTool;
                selectedBuilding_.reset();
                buildingPreview_.reset();
            }
        }
    }
    const double enemyAimRange =
        playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
            ? playerWeapons_.rifleRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::IceWand
                ? iceWand_.maximumRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::FireWand
                ? fireWand_.maximumRange()
                : gameplay_.pickaxeRange;
    aimedEnemy_ = enemies_.raycast(
        playerPosition_, direction, enemyAimRange, &terrain_);
    const bool bombsUnlocked = unlimitedResources_ ||
        skillTree_.hasEffect("unlock.bombs");
    if (command.useConsumable && bombsUnlocked && bombs_.throwBomb(
            playerPosition_, direction, !unlimitedResources_)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(
            playerPosition_, direction, enemies_,
            playerDamageMultiplier_ * std::max(
                0.05, 1.0 + skillTree_.effectValue(
                    "player.damage")));
        if (fire) {
            events_.push_back({
                .type = GameEventType::WeaponFired,
                .position = fire->hitPosition,
            });
            if (fire->targetId) {
                events_.push_back({
                    .type = GameEventType::ProjectileHit,
                    .entityId = fire->targetId,
                    .position = fire->hitPosition,
                });
                if (fire->killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = fire->targetId,
                        .position = fire->hitPosition,
                    });
                    aimedEnemy_.reset();
                }
            }
        }
    }
    if (command.fireIceWand && heldTool == PlayerWeapon::IceWand &&
        !selectedBuilding_ && iceWand_.requestFire(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::IceWandChargeStarted,
            .position = playerPosition_,
        });
    }
    if (command.fireFireWand && heldTool == PlayerWeapon::FireWand &&
        !selectedBuilding_ && fireWand_.requestFire(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::FireWandChargeStarted,
            .position = playerPosition_,
        });
    }
    constexpr double PickaxeInputBufferSeconds = 0.14;
    const bool meleeTool = heldTool != PlayerWeapon::Rifle &&
                           heldTool != PlayerWeapon::IceWand &&
                           heldTool != PlayerWeapon::FireWand;
    if (command.usePickaxe && meleeTool && !selectedBuilding_) {
        pickaxeInputBufferRemaining_ = PickaxeInputBufferSeconds;
    } else {
        pickaxeInputBufferRemaining_ = std::max(
            0.0,
            pickaxeInputBufferRemaining_ - deltaSeconds);
    }
    if (pickaxeInputBufferRemaining_ > 0.0 &&
        meleeTool &&
        !selectedBuilding_ && pickaxeCooldownRemaining_ <= 0.0) {
        pickaxeInputBufferRemaining_ = 0.0;
        pickaxeCooldownRemaining_ = gameplay_.pickaxeCooldown;
        const std::uint64_t attackSeed = mixBits64(
            tick_ ^ (pickaxeAttackSequence_++ *
                     0x9e3779b97f4a7c15ULL));
        const double variation =
            (unitRandom(attackSeed) * 2.0 - 1.0) *
            gameplay_.pickaxeDamageVariation;
        const bool critical =
            unitRandom(
                attackSeed ^ 0xd1b54a32d192ed03ULL) <
            gameplay_.pickaxeCriticalChance;
        const bool clubAttack = heldTool == PlayerWeapon::Club;
        double toolMultiplier = 1.0;
        if (heldTool == PlayerWeapon::BareHands) toolMultiplier = 0.25;
        else if (clubAttack) toolMultiplier = club_.damageMultiplier;
        else if (heldTool == PlayerWeapon::Hammer) toolMultiplier = 0.75;
        const double skillDamageMultiplier = std::max(
            0.05, 1.0 + skillTree_.effectValue("player.damage"));
        if (clubAttack) {
            toolMultiplier *= std::max(
                0.05, 1.0 + skillTree_.effectValue("club.damage"));
        }
        const double damage = playerDamageMultiplier_ *
            skillDamageMultiplier * toolMultiplier *
            gameplay_.pickaxeDamage * (1.0 + variation) *
            (critical ? 2.0 : 1.0);
        if (clubAttack) {
            Vec3 impactPosition{
                playerPosition_.x + direction.x * gameplay_.pickaxeRange,
                playerPosition_.y + direction.y * gameplay_.pickaxeRange,
                playerPosition_.z + direction.z * gameplay_.pickaxeRange,
            };
            if (aimedEnemy_) {
                if (const auto target = enemies_.enemy(*aimedEnemy_)) {
                    impactPosition = target->position;
                }
            }
            const auto results = enemies_.damageInRadius(
                impactPosition, club_.areaRadius * std::max(
                    0.05, 1.0 + skillTree_.effectValue("club.area")),
                damage, club_.knockbackStrength * std::max(
                    0.05, 1.0 + skillTree_.effectValue("club.knockback")),
                playerPosition_,
                club_.maxDamagePerAttack);
            for (const auto& result : results) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result.id,
                    .position = result.position,
                    .damage = result.damage,
                    .critical = critical,
                });
                if (result.killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = result.id,
                        .position = result.position,
                    });
                }
                if (aimedEnemy_ && result.id == *aimedEnemy_ &&
                    result.killed) {
                    aimedEnemy_.reset();
                }
            }
        } else if (aimedEnemy_) {
            const auto result = enemies_.damage(*aimedEnemy_, damage);
            if (result) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result->id,
                    .position = result->position,
                    .damage = damage,
                    .critical = critical,
                });
            }
            if (result && result->killed) {
                events_.push_back({
                    .type = GameEventType::EnemyKilled,
                    .entityId = result->id,
                    .position = result->position,
                });
                aimedEnemy_.reset();
            }
        } else if (aimedResource_) {
            const EntityId primaryTarget = *aimedResource_;
            std::vector<EntityId> resourceTargets{
                primaryTarget};
            const bool powerSwingUnlocked =
                skillTree_.hasEffect("gather.power_swing");
            const bool powerSwing = powerSwingUnlocked &&
                (++powerSwingResourceHits_ % 3U == 0U);
            if (powerSwing) {
                const double powerSwingRadius = 3.0 * std::max(
                    0.05, 1.0 + skillTree_.effectValue(
                        "gather.power_swing_radius"));
                const auto primary = std::ranges::find(
                    resources_.nodes(), primaryTarget,
                    &ResourceNode::id);
                if (primary != resources_.nodes().end()) {
                    for (const ResourceNode& node :
                         resources_.nodes()) {
                        if (!node.active ||
                            node.id == primaryTarget) {
                            continue;
                        }
                        const double deltaX =
                            node.position.x -
                            primary->position.x;
                        const double deltaZ =
                            node.position.z -
                            primary->position.z;
                        if (deltaX * deltaX +
                                deltaZ * deltaZ <=
                            powerSwingRadius * powerSwingRadius) {
                            resourceTargets.push_back(
                                node.id);
                        }
                    }
                }
            }
            for (const EntityId targetId : resourceTargets) {
                const auto targetBeforeHit = std::ranges::find(
                    resources_.nodes(), targetId, &ResourceNode::id);
                const bool largeDeposit =
                    targetBeforeHit != resources_.nodes().end() &&
                    targetBeforeHit->yield >= 30;
                if (targetBeforeHit != resources_.nodes().end() &&
                    isHarvestableResource(targetBeforeHit->type) &&
                    !hasStorageSpace(targetBeforeHit->type)) {
                    continue;
                }
                Vec3 impactPosition = resourceImpactPosition(
                    resources_.nodes(), targetId,
                    playerPosition_, direction);
                const double resourceDamage =
                    targetBeforeHit != resources_.nodes().end()
                    ? damage * std::max(
                          0.05, 1.0 + skillTree_.effectValue(
                              "gather.damage")) *
                          resourceToolEfficiency(
                          heldTool, targetBeforeHit->type)
                    : 0.0;
                const auto hit =
                    resources_.damage(targetId, resourceDamage);
                if (!hit) {
                    continue;
                }
                events_.push_back({
                    .type =
                        hit->collected ? GameEventType::ResourceCollected : GameEventType::ResourceHit,
                    .entityId = hit->nodeId,
                    .resourceType = hit->type,
                    .position = impactPosition,
                    .amount = hit->amount,
                    .damage = resourceDamage,
                    .critical = critical,
                    .bareHands = heldTool == PlayerWeapon::BareHands,
                    .largeDeposit = largeDeposit,
                    .night = state_ == RunState::Sunset || state_ == RunState::Wave,
                });
                if (hit->amount > 0) {
                    pendingResourceGrants_.push_back({
                        .type = hit->type,
                        .position = impactPosition,
                        .amount = hit->amount,
                        .remaining =
                            ResourcePickupFlightSeconds,
                    });
                    if (heldTool == PlayerWeapon::BareHands &&
                        !introSkillObjectiveCompleted_) {
                        if (hit->type == ResourceType::Wood)
                            bareHandsWoodGathered_ += hit->amount;
                        else
                            bareHandsStoneGathered_ += hit->amount;
                        if (bareHandsWoodGathered_ >= 15 && bareHandsStoneGathered_ >= 10) {
                            introSkillObjectiveCompleted_ = true;
                            events_.push_back({.type = GameEventType::IntroSkillObjectiveCompleted});
                        }
                    }
                }
                if (hit->collected) {
                    if (isDestructibleProp(hit->type)) {
                        const std::uint64_t rewardSeed = mixBits64(
                            attackSeed ^
                            (static_cast<std::uint64_t>(targetId.index)
                             << 17U));
                        const double rewardRoll = unitRandom(rewardSeed);
                        const int baseCoins = hit->type == ResourceType::Barrel
                            ? 3 + static_cast<int>(rewardSeed % 5ULL)
                            : hit->type == ResourceType::Crate
                                ? 5 + static_cast<int>(rewardSeed % 7ULL)
                                : 7 + static_cast<int>(rewardSeed % 9ULL);
                        const int coins = static_cast<int>(std::lround(
                            static_cast<double>(baseCoins) * std::max(
                                0.0, 1.0 + skillTree_.effectValue(
                                    "prop.coins"))));
                        bool droppedItem = false;
                        LootRarity rarity = LootRarity::Common;
                        if (hit->type == ResourceType::Crate) {
                            droppedItem = rewardRoll < 0.16;
                            rarity = rewardRoll < 0.025
                                ? LootRarity::Rare : LootRarity::Common;
                        } else if (hit->type == ResourceType::ItemCrate) {
                            droppedItem = rewardRoll < 0.62;
                            rarity = rewardRoll < 0.025
                                ? LootRarity::Legendary
                                : rewardRoll < 0.16
                                    ? LootRarity::Rare
                                    : LootRarity::Common;
                        }
                        if (droppedItem) {
                            lootChests_.spawnLooseLoot(
                                impactPosition, rarity, rewardSeed);
                        } else {
                            coinPickups_.spawnValue(
                                impactPosition, coins, rewardSeed, terrain_,
                                0.5);
                        }
                        if (hit->type == ResourceType::Barrel) {
                            grantConfiguredInsight(
                                1.0 + unitRandom(rewardSeed ^ 0x94d049bbULL),
                                InsightSource::Other,
                                InsightCategory::Exploration, {});
                        }
                    }
                    if (targetId == primaryTarget) {
                        aimedResource_.reset();
                    }
                }
            }
        } else if (canGather) {
            events_.push_back({
                .type = GameEventType::ResourceGatherMissed,
                .position = playerPosition_,
                .bareHands = heldTool == PlayerWeapon::BareHands,
                .night = state_ == RunState::Sunset || state_ == RunState::Wave,
            });
        }
    }

    const auto bombExplosions = bombs_.tick(deltaSeconds, enemies_, &terrain_);
    for (const auto& explosion : bombExplosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
            .intensity = explosion.radius,
        });
    }

    iceWand_.tick(
        deltaSeconds, enemies_, &terrain_,
        std::span<const BuildingInstance>{buildings_.buildings()},
        std::span<const MapObstacle>{map_.obstacles});
    for (const auto& launch : iceWand_.launches()) {
        events_.push_back({
            .type = GameEventType::IceWandFired,
            .entityId = launch.projectileId,
            .position = launch.position,
        });
    }
    for (const auto& hit : iceWand_.hits()) {
        events_.push_back({
            .type = GameEventType::IceWandHit,
            .entityId = hit.enemyId,
            .sourceId = hit.projectileId,
            .position = hit.position,
            .damage = hit.damage,
            .critical = hit.alreadyFrozen,
        });
        if (hit.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.enemyId,
                .sourceId = hit.projectileId,
                .position = hit.position,
            });
            if (aimedEnemy_ && *aimedEnemy_ == hit.enemyId) {
                aimedEnemy_.reset();
            }
        }
    }
    for (const auto& impact : iceWand_.impacts()) {
        events_.push_back({
            .type = GameEventType::IceWandImpact,
            .entityId = impact.projectileId,
            .position = impact.position,
            .amount = impact.hitCount,
            .intensity = static_cast<double>(impact.killedCount),
        });
    }

    fireWand_.tick(
        deltaSeconds, enemies_, &terrain_,
        std::span<const BuildingInstance>{buildings_.buildings()},
        std::span<const MapObstacle>{map_.obstacles});
    for (const auto& launch : fireWand_.launches()) {
        events_.push_back({
            .type = GameEventType::FireWandFired,
            .entityId = launch.projectileId,
            .position = launch.position,
        });
    }
    for (const auto& hit : fireWand_.hits()) {
        events_.push_back({
            .type = GameEventType::FireWandHit,
            .entityId = hit.enemyId,
            .sourceId = hit.projectileId,
            .position = hit.position,
            .damage = hit.damage,
            .intensity = hit.periodicBurn
                ? 0.0
                : fireWand_.burnDuration(),
        });
        if (hit.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.enemyId,
                .sourceId = hit.projectileId,
                .position = hit.position,
            });
            if (aimedEnemy_ && *aimedEnemy_ == hit.enemyId) {
                aimedEnemy_.reset();
            }
        }
    }
    for (const auto& impact : fireWand_.impacts()) {
        events_.push_back({
            .type = GameEventType::FireWandImpact,
            .entityId = impact.projectileId,
            .position = impact.position,
            .amount = impact.hitCount,
            .intensity = static_cast<double>(impact.killedCount),
        });
    }

    lootChests_.tick(deltaSeconds);
    aimedChest_ = lootChests_.raycastChest(
        playerPosition_, direction, 4.5);
    aimedLoot_ = lootChests_.raycastLoot(
        playerPosition_, direction, 5.0);
    if (command.interact) {
        if (aimedLoot_) {
            if (const auto pickup = lootChests_.collect(*aimedLoot_))
                applyLootPickup(*pickup);
        } else if (aimedChest_) {
            int availableGold = unlimitedResources_
                ? std::numeric_limits<int>::max()
                : coins_;
            const ChestOpenResult result =
                lootChests_.open(*aimedChest_, availableGold);
            if (!unlimitedResources_) {
                coins_ = availableGold;
            }
            events_.push_back({
                .type = result == ChestOpenResult::Opened
                    ? GameEventType::ChestOpened
                    : GameEventType::ChestOpenRejected,
                .entityId = aimedChest_,
            });
            if (result == ChestOpenResult::Opened)
                aimedChest_.reset();
        }
    }
    if (!playerRespawning_) {
        if (const auto pickup =
                lootChests_.collectNearby(
                    playerPosition_, LootProximityPickupRadius))
            applyLootPickup(*pickup);
    }
}

} // namespace ian
