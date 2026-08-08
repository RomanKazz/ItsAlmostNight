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

} // namespace

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
            gold_ = saturatingAdd(gold_, produced.amount);
            events_.push_back({
                .type = GameEventType::GoldProduced,
                .entityId = produced.mineId,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
            });
        } else {
            const ResourceType resourceType =
                produced.buildingType ==
                        BuildingType::LumberMill
                    ? ResourceType::Wood
                    : ResourceType::Stone;
            if (resourceType == ResourceType::Wood) {
                wood_ = saturatingAdd(wood_, produced.amount);
            } else {
                stone_ = saturatingAdd(stone_, produced.amount);
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
    const bool automaticToolSwitch =
        skillTree_.hasEffect(SkillEffect::AutoSwitchTools);
    if (automaticToolSwitch && aimedResource_) {
        const auto node = std::ranges::find(
            resources_.nodes(), *aimedResource_,
            &ResourceNode::id);
        if (node != resources_.nodes().end()) {
            const PlayerWeapon desiredTool =
                node->type == ResourceType::Wood
                    ? PlayerWeapon::Axe
                    : PlayerWeapon::Pickaxe;
            if (heldTool != desiredTool) {
                playerWeapons_.selectWeapon(desiredTool);
                heldTool = desiredTool;
                selectedBuilding_.reset();
                buildingPreview_.reset();
            }
        }
    }
    if (aimedResource_ && heldTool != PlayerWeapon::BareHands) {
        const auto node = std::ranges::find(resources_.nodes(), *aimedResource_, &ResourceNode::id);
        const bool matching = node != resources_.nodes().end() &&
            ((heldTool == PlayerWeapon::Axe && node->type == ResourceType::Wood) ||
             (heldTool == PlayerWeapon::Pickaxe && node->type == ResourceType::Stone));
        if (!matching) aimedResource_.reset();
    }
    const double enemyAimRange =
        playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
            ? playerWeapons_.rifleRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::IceWand
                ? iceWand_.maximumRange()
                : gameplay_.pickaxeRange;
    aimedEnemy_ = enemies_.raycast(playerPosition_, direction, enemyAimRange);
    if (command.useConsumable && bombs_.throwBomb(
            playerPosition_, direction, !unlimitedResources_)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(
            playerPosition_, direction, enemies_,
            playerDamageMultiplier_);
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
    constexpr double PickaxeInputBufferSeconds = 0.14;
    const bool meleeTool = heldTool != PlayerWeapon::Rifle &&
                           heldTool != PlayerWeapon::IceWand;
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
        const double damage = playerDamageMultiplier_ * toolMultiplier *
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
                impactPosition, club_.areaRadius, damage,
                club_.knockbackStrength, playerPosition_,
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
            const Vec3 impactPosition = resourceImpactPosition(
                resources_.nodes(), *aimedResource_,
                playerPosition_, direction);
            const auto hit =
                resources_.damage(*aimedResource_, damage);
            if (hit) {
                events_.push_back({
                    .type =
                        hit->collected ? GameEventType::ResourceCollected : GameEventType::ResourceHit,
                    .entityId = hit->nodeId,
                    .resourceType = hit->type,
                    .position = impactPosition,
                    .amount = hit->amount,
                    .damage = damage,
                    .critical = critical,
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
                            grantSkillPoints(1, SkillPointSource::IntroObjective);
                            events_.push_back({.type = GameEventType::IntroSkillObjectiveCompleted});
                        }
                    }
                }
                if (hit->collected) {
                    aimedResource_.reset();
                }
            }
        }
    }

    const auto bombExplosions = bombs_.tick(deltaSeconds, enemies_, &terrain_);
    for (const auto& explosion : bombExplosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
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
                : gold_;
            const ChestOpenResult result =
                lootChests_.open(*aimedChest_, availableGold);
            if (!unlimitedResources_) {
                gold_ = availableGold;
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
