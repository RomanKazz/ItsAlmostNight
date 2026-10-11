#pragma once

#include "core/DeterministicRandom.hpp"
#include "core/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ian {
enum class PowerupType : std::uint8_t { Magnet, Fury, GoldRush, Count };
inline constexpr std::size_t PowerupTypeCount = static_cast<std::size_t>(PowerupType::Count);
struct PowerupDefinition {
    std::string_view name;
    std::string_view description;
    double duration;
};
inline constexpr std::array<PowerupDefinition, PowerupTypeCount> PowerupDefinitions{{
    {"MAGNET", "Pulls all XP and coins", 6.0},
    {"FURY", "+50% defense damage and attack speed", 15.0},
    {"GOLD RUSH", "Triple coins from enemy kills", 20.0},
}};
struct PowerupPickup {
    Vec3 position{};
    PowerupType type{PowerupType::Magnet};
    double age{};
    bool active{};
};
struct PowerupRunState {
    std::array<double, PowerupTypeCount> remaining{};
    std::array<PowerupPickup, 32> pickups{};
};
// Fixed slots: no allocations during mass kills, no unbounded spawn containers.
class PowerupSystem {
  public:
    static constexpr double PickupLifetime = 45.0;
    static constexpr double CollectionRadius = 1.4;
    [[nodiscard]] static std::optional<PowerupType> rollDrop(std::uint64_t seed, bool elite, bool boss) {
        const double chance = boss ? 0.20 : elite ? 0.05 : 0.01;
        if (unitRandom(seed ^ 0xa8e77a492e871d61ULL) >= chance) return std::nullopt;
        return static_cast<PowerupType>(mixBits64(seed ^ 0x434152544f4f4eULL) % PowerupTypeCount);
    }
    void reset() { state_ = {}; }
    [[nodiscard]] bool spawn(PowerupType type, Vec3 position) {
        if (type >= PowerupType::Count || !std::isfinite(position.x) ||
            !std::isfinite(position.y) || !std::isfinite(position.z)) return false;
        const auto slot = std::ranges::find(state_.pickups, false, &PowerupPickup::active);
        if (slot == state_.pickups.end()) return false;
        *slot = {position, type, 0.0, true};
        return true;
    }
    void activate(PowerupType type) {
        if (type >= PowerupType::Count) return;
        // Refresh, never multiply strength or accumulate infinite duration.
        state_.remaining[static_cast<std::size_t>(type)] = PowerupDefinitions[static_cast<std::size_t>(type)].duration;
    }
    // Return collected types; presentation reads timers, gameplay rebuilds buffs only on transitions.
    [[nodiscard]] std::array<bool, PowerupTypeCount> tick(double seconds, Vec3 playerFeet, bool collect = true) {
        std::array<bool, PowerupTypeCount> collected{};
        if (!std::isfinite(seconds) || seconds < 0) return collected;
        for (auto& timer : state_.remaining) timer = std::max(0.0, timer - seconds);
        for (auto& pickup : state_.pickups) {
            if (!pickup.active) continue;
            pickup.age += seconds;
            if (pickup.age >= PickupLifetime) { pickup = {}; continue; }
            const double dx = pickup.position.x - playerFeet.x, dz = pickup.position.z - playerFeet.z;
            const double dy = pickup.position.y - playerFeet.y;
            if (collect && pickup.age >= .25 && dx*dx + dz*dz <= CollectionRadius*CollectionRadius && std::abs(dy) < 2.0) {
                activate(pickup.type);
                collected[static_cast<std::size_t>(pickup.type)] = true;
                pickup.active = false;
            }
        }
        return collected;
    }
    [[nodiscard]] double remaining(PowerupType type) const {
        return type < PowerupType::Count ? state_.remaining[static_cast<std::size_t>(type)] : 0.0;
    }
    [[nodiscard]] bool active(PowerupType type) const { return remaining(type) > 0; }
    [[nodiscard]] double defenseDamageMultiplier() const { return active(PowerupType::Fury) ? 1.5 : 1.0; }
    [[nodiscard]] double defenseSpeedMultiplier() const { return active(PowerupType::Fury) ? 1.5 : 1.0; }
    [[nodiscard]] int enemyCoinMultiplier() const { return active(PowerupType::GoldRush) ? 3 : 1; }
    [[nodiscard]] const PowerupRunState& state() const { return state_; }
    [[nodiscard]] static bool validState(const PowerupRunState& state) {
        for (std::size_t i=0; i<PowerupTypeCount; ++i)
            if (!std::isfinite(state.remaining[i]) || state.remaining[i] < 0 || state.remaining[i] > PowerupDefinitions[i].duration) return false;
        for (const auto& p : state.pickups)
            if (p.type >= PowerupType::Count || !std::isfinite(p.age) || p.age < 0 || p.age > PickupLifetime ||
                !std::isfinite(p.position.x) || !std::isfinite(p.position.y) || !std::isfinite(p.position.z)) return false;
        return true;
    }
    [[nodiscard]] bool loadState(const PowerupRunState& state) {
        if (!validState(state)) return false;
        state_ = state; return true;
    }
  private:
    PowerupRunState state_{};
};
} // namespace ian
