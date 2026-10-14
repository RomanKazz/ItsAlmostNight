#pragma once

#include "game/GameEvent.hpp"
#include "combat/AttackCollision.hpp"
#include "enemies/EnemySystem.hpp"
#include <array>
#include <span>
#include <vector>

namespace ian {
struct ItemPoisonCloud {
    Vec3 position;
    double remaining{};
    double duration{};
    double radius{};
    double damagePerSecond{};
    double tickRemaining{};
    std::optional<EntityId> source;
    std::optional<BuildingType> buildingType;
};

// Run-owned item state. Fixed enemy slots, bounded reusable cloud storage and
// explicit secondary events keep effects independent of presentation FPS.
class ChestItemCombatSystem {
public:
    ChestItemCombatSystem();
    void reset();
    void tick(double seconds, EnemySystem& enemies,
        const std::array<int, LootUpgradeEffectCount>& stacks,
        std::vector<GameEvent>& events, std::size_t firstEvent,
        std::span<const BuildingInstance> buildings = {});
    [[nodiscard]] std::span<const ItemPoisonCloud> clouds() const { return clouds_; }
    [[nodiscard]] static bool directHit(const GameEvent& event);
    [[nodiscard]] static double gasMaskProtection(Vec3 player, const EnemySystem& enemies, int stacks);
private:
    struct TargetState {
        EntityId id{};
        double lastDirectDamage{};
        double burnRemaining{};
        double burnBaseDamage{};
        double burnDamage{};
        double burnPending{};
        double burnTick{0.5};
        int burnDoses{};
        std::optional<EntityId> source;
        std::optional<BuildingType> buildingType;
        bool deathHandled{};
    };
    struct HitCounter { EntityId id{}; unsigned int count{}; };
    std::vector<TargetState> targets_;
    std::vector<ItemPoisonCloud> clouds_;
    std::vector<HitCounter> counters_;
    std::uint64_t serial_{};
    AttackCollisionQuery lightningQuery_;
    bool lightningQueryReady_{};
    TargetState* target(EnemySystem& enemies, EntityId id);
    void ignite(EnemySystem& enemies, EntityId id, const GameEvent& hit, int copies);
    void lightning(EnemySystem& enemies, const GameEvent& hit, int copies, std::vector<GameEvent>& events);
};
} // namespace ian
