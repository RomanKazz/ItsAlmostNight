#include "game/ChestItemCombatSystem.hpp"
#include "core/DeterministicRandom.hpp"
#include <algorithm>
#include <cmath>

namespace ian {
namespace {
constexpr std::size_t MaximumClouds = 128;
constexpr std::size_t MaximumBurstsPerTick = 64;
double distanceSquared(Vec3 a, Vec3 b) {
    const double x = a.x - b.x, z = a.z - b.z;
    return x * x + z * z;
}
void recordDamage(const EnemyDamageResult& hit, const GameEvent& source,
    GameEventType type, std::vector<GameEvent>& events) {
    events.push_back({.type = type, .entityId = hit.id, .sourceId = source.sourceId,
        .buildingType = source.buildingType, .position = hit.position,
        .damage = hit.damage, .secondaryImpact = true});
    if (hit.killed) {
        auto killed = enemyKilledEvent(hit.id, hit.type, hit.eliteAffixes,
            hit.position, source.sourceId, source.buildingType);
        killed.secondaryImpact = true;
        events.push_back(killed);
    }
}
}
ChestItemCombatSystem::ChestItemCombatSystem() : targets_(EnemySystem::MaxEnemies) {
    clouds_.reserve(MaximumClouds);
    counters_.reserve(128);
}
void ChestItemCombatSystem::reset() {
    std::fill(targets_.begin(), targets_.end(), TargetState{});
    clouds_.clear(); counters_.clear(); serial_ = 0;
}
bool ChestItemCombatSystem::directHit(const GameEvent& e) {
    if (!e.entityId || e.damage <= 0) return false;
    // Physical bounce/pierce hits remain weapon hits. The presentation's
    // secondary flag must not suppress them; item lightning and kill shards
    // are separate sources and cannot recursively reroll item effects.
    if (e.type == GameEventType::ChainLightningHit && e.secondaryImpact) return false;
    if (e.type == GameEventType::ProjectileHit && e.amount < 0) return false;
    return e.type == GameEventType::ChainLightningHit || e.type == GameEventType::PickaxeHit || e.type == GameEventType::IceWandHit ||
        (e.type == GameEventType::FireWandHit && e.intensity > 0) ||
        e.type == GameEventType::CannonHit || e.type == GameEventType::TrapHit ||
        (e.type == GameEventType::ProjectileHit && (e.buildingType || !e.sourceId));
}
ChestItemCombatSystem::TargetState* ChestItemCombatSystem::target(EnemySystem& enemies, EntityId id) {
    const auto slot = enemies.slotIndex(id);
    if (!slot || enemies.enemies()[*slot].id != id) return nullptr;
    auto& value = targets_[*slot];
    if (value.id != id) value = TargetState{.id = id};
    return &value;
}
void ChestItemCombatSystem::ignite(EnemySystem& enemies, EntityId id, const GameEvent& hit, int copies) {
    auto* value = target(enemies, id);
    if (!value) return;
    if (value->burnRemaining <= 0) {
        value->burnBaseDamage = value->burnDamage = value->burnPending = 0;
        value->burnDoses = 0; value->burnTick = 0.5;
    }
    value->burnRemaining = std::max(value->burnRemaining, copies > 0 ? 4.0 :
        (hit.intensity > 0 ? hit.intensity : 4.0));
    if (copies <= 0) {
        value->burnBaseDamage = std::max(value->burnBaseDamage, hit.damage * 0.20);
        return; // Track natural fire without applying a second natural DoT.
    }
    const double base = std::max(hit.damage, hit.projectileDamage) * (0.20 + 0.05 * std::min(8, copies - 1));
    if (base >= value->burnBaseDamage) {
        value->burnBaseDamage = base;
        value->source = hit.sourceId;
        value->buildingType = hit.buildingType;
    }
    value->burnDoses = std::min(8, value->burnDoses + 1);
    value->burnDamage = value->burnBaseDamage * (1.0 + 0.20 * (value->burnDoses - 1));
}
void ChestItemCombatSystem::lightning(EnemySystem& enemies, const GameEvent& hit, int copies,
    std::vector<GameEvent>& events) {
    if(!lightningQueryReady_) {lightningQuery_.rebuild(enemies);lightningQueryReady_=true;}
    std::array<EntityId, 9> visited{};
    std::size_t count = 1;
    visited[0] = *hit.entityId;
    Vec3 origin = hit.position;
    double damage = std::max(hit.damage, hit.projectileDamage) * (0.45 + 0.08 * std::min(8, copies - 1));
    const int jumps = std::min(8, 1 + copies);
    for (int jump = 0; jump < jumps; ++jump) {
        std::optional<EntityId> next;
        double nearest = 6.5 * 6.5;
        lightningQuery_.nearby(enemies,origin,6.5,[&](const EnemyInstance& enemy) {
            if(std::find(visited.begin(),visited.begin()+static_cast<std::ptrdiff_t>(count),enemy.id)
                != visited.begin()+static_cast<std::ptrdiff_t>(count)) return;
            const double distance=distanceSquared(origin,enemy.position);
            if(distance<nearest) {nearest=distance;next=enemy.id;}
        });
        if (!next) break;
        const auto result = enemies.damage(*next, damage);
        if (!result) break;
        Vec3 end = result->position;
        const auto slot = enemies.slotIndex(*next);
        if (slot) end.y += enemies.enemies()[*slot].worldSurfaceHeight + 0.5;
        events.push_back({.type = GameEventType::ChainLightningHit, .entityId = *next,
            .sourceId = hit.sourceId, .buildingType = hit.buildingType, .position = origin,
            .targetPosition = end, .amount = jump, .damage = result->damage,
            .secondaryImpact = true});
        if (result->killed) {
            auto killed = enemyKilledEvent(result->id, result->type, result->eliteAffixes,
                result->position, hit.sourceId, hit.buildingType);
            killed.secondaryImpact = true;
            events.push_back(killed);
        }
        visited[count++] = *next;
        origin = end; damage *= 0.78;
    }
}

double ChestItemCombatSystem::gasMaskProtection(Vec3 player, const EnemySystem& enemies, int stacks) {
    if (stacks <= 0) return 0;
    int poisoned = 0;
    for (const auto& enemy : enemies.enemies()) {
        if (enemy.active && enemyHasStatus(enemy, StatusEffectType::Poison) &&
            distanceSquared(player, enemy.position) <= 12.0 * 12.0) ++poisoned;
    }
    return std::min(0.35, 0.03 * stacks * poisoned);
}

void ChestItemCombatSystem::tick(double seconds, EnemySystem& enemies,
    const std::array<int, LootUpgradeEffectCount>& stacks, std::vector<GameEvent>& events,
    std::size_t firstEvent, std::span<const BuildingInstance> buildings) {
    lightningQueryReady_=false;
    seconds = std::isfinite(seconds) ? std::max(0.0, seconds) : 0.0;
    const auto copies = [&](LootUpgradeEffect e) { return std::clamp(stacks[lootUpgradeIndex(e)], 0, 1000); };
    if (!buildings.empty()) std::erase_if(counters_, [&](const HitCounter& counter) {
        return counter.id != EntityId{} && std::ranges::none_of(buildings,
            [&](const BuildingInstance& b) { return b.id == counter.id; });
    });
    // Established DoT advances before new hits. Secondary damage cannot reroll on-hit items.
    for (auto& value : targets_) {
        if (value.burnRemaining <= 0) continue;
        const auto enemy = enemies.enemy(value.id);
        if (!enemy) continue; // Keep death-frame fire until the death reactions below.
        const double elapsed = std::min(seconds, value.burnRemaining);
        value.burnRemaining -= elapsed;
        value.burnPending += elapsed * value.burnDamage;
        value.burnTick -= elapsed;
        if (value.burnPending > 0 && (value.burnTick <= 1e-9 || value.burnRemaining <= 1e-9)) {
            const GameEvent source{.type = GameEventType::ItemBurnHit, .sourceId = value.source,
                .buildingType = value.buildingType};
            if (const auto hit = enemies.damage(value.id, value.burnPending)) {
                recordDamage(*hit, source, GameEventType::ItemBurnHit, events);
                // Burn deaths are allowed to spread Coal; item burst deaths are not.
                if (hit->killed) { events.back().secondaryImpact = false; value.burnRemaining = 1e-8; }
            }
            value.burnPending = 0; value.burnTick = 0.5;
        }

    }
    for (auto& cloud : clouds_) {
        const double elapsed = std::min(seconds, cloud.remaining);
        cloud.remaining -= elapsed; cloud.tickRemaining -= elapsed;
        if (cloud.tickRemaining > 0) continue;
        cloud.tickRemaining = 0.5;
        for (const auto& enemy : enemies.enemies()) {
            if (!enemy.active || distanceSquared(cloud.position, enemy.position) > cloud.radius * cloud.radius) continue;
            enemies.applyPoison(enemy.id, cloud.source, 2.0, cloud.damagePerSecond, cloud.buildingType);
        }
    }
    std::erase_if(clouds_, [](const ItemPoisonCloud& cloud) { return cloud.remaining <= 0; });
    const std::size_t directLimit = events.size();
    std::size_t bursts = 0;
    for (std::size_t i = firstEvent; i < directLimit; ++i) {
        const GameEvent hit = events[i]; // Appending effects may reallocate the event vector.
        if (!directHit(hit)) continue;
        auto* value = target(enemies, *hit.entityId);
        if (!value) continue;
        value->lastDirectDamage = std::max(hit.damage, hit.projectileDamage);
        const bool naturalFire = hit.type == GameEventType::FireWandHit || hit.buildingType == BuildingType::Flamethrower;
        const int fuel = copies(LootUpgradeEffect::FuelJerrycan);
        const double roll = static_cast<double>(mixBits64(++serial_ ^
            (static_cast<std::uint64_t>(hit.entityId->index) << 32U)) >> 11U) / 9007199254740992.0;
        if (naturalFire || (fuel > 0 && roll < std::min(0.75, 0.30 + 0.05 * (fuel - 1))))
            ignite(enemies, *hit.entityId, hit, fuel);
        const int lens = copies(LootUpgradeEffect::TargetingLens);
        if (lens && !hit.buildingType && enemies.markTarget(*hit.entityId,
            std::min(10.0, 4.0 + lens - 1), 1.0 + std::min(0.75, 0.25 + 0.10 * (lens - 1)))) {
            events.push_back({.type = GameEventType::ItemMarked, .entityId = hit.entityId,
                .position = hit.position, .duration = 4.0});
        }
        const int nail = copies(LootUpgradeEffect::Nail);
        if (nail) {
            const EntityId owner = hit.buildingType ? hit.sourceId.value_or(EntityId{}) : EntityId{};
            auto found = std::find_if(counters_.begin(), counters_.end(), [&](const HitCounter& c) { return c.id == owner; });
            if (found == counters_.end() && counters_.size() < EnemySystem::MaxEnemies) {
                counters_.push_back({owner}); found = std::prev(counters_.end());
            }
            if (found != counters_.end()) {
                if (++found->count >= static_cast<unsigned int>(std::max(2, 6 - nail))) {
                    found->count = 0;
                    if (bursts++ < MaximumBurstsPerTick) lightning(enemies, hit, nail, events);
                }
            }
        }
        const int powder = copies(LootUpgradeEffect::PowderPouch);
        // IceWand's historical critical flag means "already frozen", not a rolled crit.
        if (powder && hit.critical && hit.type != GameEventType::IceWandHit && bursts++ < MaximumBurstsPerTick) {
            const double radius = std::min(4.0, 1.8 + 0.15 * (powder - 1));
            events.push_back({.type = GameEventType::ItemExplosion, .sourceId = hit.sourceId,
                .buildingType = hit.buildingType, .position = hit.position, .intensity = radius});
            for (const auto& enemy : enemies.enemies()) {
                if (!enemy.active || enemy.id == *hit.entityId || distanceSquared(enemy.position, hit.position) > radius * radius) continue;
                if (auto result = enemies.damage(enemy.id, std::max(hit.damage, hit.projectileDamage) *
                    (0.35 + 0.10 * std::min(6, powder - 1)))) recordDamage(*result, hit, GameEventType::ItemHit, events);
            }
        }
    }
    const std::size_t deathLimit = events.size();
    for (std::size_t i = firstEvent; i < deathLimit; ++i) {
        const GameEvent death = events[i];
        if (death.type != GameEventType::EnemyKilled || !death.entityId) continue;
        auto* value = target(enemies, *death.entityId);
        if (!value || value->deathHandled) continue;
        value->deathHandled = true;
        const auto slot = enemies.slotIndex(*death.entityId);
        if (!slot) continue;
        const auto enemy = enemies.enemies()[*slot];
        const auto& poison = enemyStatusEffect(enemy, StatusEffectType::Poison);
        const int coal = copies(LootUpgradeEffect::CoalBell);
        if (coal && value->burnRemaining > 0 && bursts++ < MaximumBurstsPerTick) {
            const double radius = std::min(4.5, 2.3 + 0.25 * (coal - 1));
            const double damage = std::max(1.0, value->burnBaseDamage * (1.8 + 0.4 * (coal - 1)));
            events.push_back({.type = GameEventType::ItemExplosion, .sourceId = death.sourceId,
                .buildingType = death.buildingType, .position = death.position,
                .intensity = radius, .lootUpgradeEffect = LootUpgradeEffect::CoalBell});
            for (const auto& nearby : enemies.enemies()) {
                if (!nearby.active || distanceSquared(nearby.position, enemy.position) > radius * radius) continue;
                auto result = enemies.damage(nearby.id, damage);
                if (!result) continue;
                recordDamage(*result, death, GameEventType::ItemHit, events);
                if (!result->killed) {
                    GameEvent flame = death; flame.damage = damage; flame.projectileDamage = 0;
                    ignite(enemies, result->id, flame, std::max(1, copies(LootUpgradeEffect::FuelJerrycan)));
                }
            }
        }
        const int infected = copies(LootUpgradeEffect::InfectedPouch);
        // Poison's last tick may consume its remaining duration on the killing hit.
        if (infected && poison.intensity > 0 && (poison.remaining > 0 || poison.visualParameter > 0) &&
            clouds_.size() < MaximumClouds) {
            const double radius = std::min(5.0, 2.4 + 0.2 * (infected - 1));
            clouds_.push_back({.position = death.position, .remaining = 3.0,
                .duration = 3.0, .radius = radius,
                .damagePerSecond = poison.basePoisonIntensity * (0.60 + 0.15 * std::min(6, infected - 1)),
                .source = poison.source, .buildingType = poison.sourceBuildingType});
            events.push_back({.type = GameEventType::ItemPoisonCloud, .position = death.position,
                .intensity = radius, .duration = 3.0});
        }
        const int prism = copies(LootUpgradeEffect::IcePrism);
        if (prism && enemyHasStatus(enemy, StatusEffectType::Freeze) && bursts++ < MaximumBurstsPerTick) {
            const double radius = std::min(5.0, 2.8 + 0.2 * (prism - 1));
            events.push_back({.type = GameEventType::ItemIceShatter, .position = death.position, .intensity = radius});
            for (const auto& nearby : enemies.enemies()) {
                if (!nearby.active || distanceSquared(nearby.position, enemy.position) > radius * radius) continue;
                const bool slowed = nearby.slowRemaining > 0 || enemyHasStatus(nearby, StatusEffectType::Slow);
                if (auto result = enemies.damage(nearby.id, std::max(2.0, value->lastDirectDamage * 0.45) *
                    (1.0 + 0.25 * std::min(8, prism - 1)) * (slowed ? 1.75 : 1.0)))
                    recordDamage(*result, death, GameEventType::ItemHit, events);
            }
        }
    }
    for (auto& value : targets_) if (value.deathHandled) value.burnRemaining = 0;
}
} // namespace ian
