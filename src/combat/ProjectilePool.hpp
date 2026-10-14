#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>

namespace ian {
// High safety limit for runaway builds, not a stat cap. Saturation drops new
// instances and is observable via dropped(). No allocation/destruction per shot
// after the pool reaches its high-water mark. Free slots are acquired in O(1).
template <typename Projectile> class ProjectilePool {
  public:
    static constexpr std::size_t Capacity = 16384;
    explicit ProjectilePool(std::uint32_t firstId) : nextId_(firstId) { items_.reserve(512); free_.reserve(512); }
    Projectile* acquire() {
        std::size_t slot;
        if (!free_.empty()) { slot = free_.back(); free_.pop_back(); }
        else {
            if (items_.size() >= Capacity) { ++dropped_; return nullptr; }
            slot = items_.size(); items_.emplace_back(); items_.back().id = {nextId_++, 0};
        }
        auto& item = items_[slot];
        item.resetForReuse();
        ++item.id.generation;
        item.active = true;
        return &item;
    }
    void release(std::size_t slot) {
        if (!items_[slot].active) return;
        items_[slot].resetForReuse();
        items_[slot].active = false;
        free_.push_back(slot);
    }
    void clear() { for (std::size_t i = 0; i < items_.size(); ++i) release(i); dropped_ = 0; }
    std::vector<Projectile>& items() { return items_; }
    const std::vector<Projectile>& items() const { return items_; }
    std::size_t dropped() const { return dropped_; }
  private:
    std::vector<Projectile> items_;
    std::vector<std::size_t> free_;
    std::uint32_t nextId_;
    std::size_t dropped_{};
};
} // namespace ian
