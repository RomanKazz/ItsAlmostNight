#include "buildings/FoundationSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

ResourceCost modularRepairCost(
    double health, double maxHealth,
    ResourceCost fullRepairCost) {
    const double missingRatio =
        maxHealth > 0.0
            ? std::clamp(
                  (maxHealth - health) / maxHealth,
                  0.0, 1.0)
            : 0.0;
    const auto scaled =
        [missingRatio](int value) {
            return static_cast<int>(
                std::ceil(value * missingRatio));
        };
    return {
        .wood = scaled(fullRepairCost.wood),
        .stone = scaled(fullRepairCost.stone),
    };
}

} // namespace

bool FoundationSystem::eraseInstance(
    EntityId id, bool releaseFoundationSupports) {
    const auto frame = std::find_if(
        platformFrames_.begin(), platformFrames_.end(),
        [id](const PlatformFrameInstance& instance) {
            return instance.id == id;
        });
    if (frame != platformFrames_.end()) {
        if (releaseFoundationSupports) {
            supports_.release(frame->supportIds);
        }
        grid_.release(id);
        platformFrames_.erase(frame);
        return true;
    }
    const auto wall = std::find_if(
        walls_.begin(), walls_.end(),
        [id](const WallInstance& instance) {
            return instance.id == id;
        });
    if (wall != walls_.end()) {
        grid_.release(id);
        walls_.erase(wall);
        return true;
    }
    const auto ramp = std::find_if(
        ramps_.begin(), ramps_.end(),
        [id](const RampInstance& instance) {
            return instance.id == id;
        });
    if (ramp != ramps_.end()) {
        grid_.release(id);
        ramps_.erase(ramp);
        return true;
    }
    return false;
}

bool FoundationSystem::remove(EntityId id) {
    if (!structuralGraph_.contains(id)) {
        return false;
    }
    static_cast<void>(structuralGraph_.remove(id));
    const bool removed = eraseInstance(id, true);
    syncStructuralStates();
    return removed;
}

std::optional<ModularBuildingDamageResult>
FoundationSystem::damage(
    EntityId id, double amount) {
    if (amount <= 0.0) {
        return std::nullopt;
    }
    const auto frame = std::find_if(
        platformFrames_.begin(), platformFrames_.end(),
        [id](const PlatformFrameInstance& candidate) {
            return candidate.id == id;
        });
    ModularBuildingDamageResult result{
        .id = id,
    };
    if (frame != platformFrames_.end()) {
        frame->health =
            std::max(0.0, frame->health - amount);
        result.platformFrame = *frame;
        result.destroyed = frame->health <= 0.0;
    } else {
        const auto wall = std::find_if(
            walls_.begin(), walls_.end(),
            [id](const WallInstance& candidate) {
                return candidate.id == id;
            });
        if (wall != walls_.end()) {
            wall->health =
                std::max(0.0, wall->health - amount);
            result.wall = *wall;
            result.destroyed = wall->health <= 0.0;
        } else {
            const auto ramp = std::find_if(
                ramps_.begin(), ramps_.end(),
                [id](const RampInstance& candidate) {
                    return candidate.id == id;
                });
            if (ramp == ramps_.end()) {
                return std::nullopt;
            }
            ramp->health =
                std::max(0.0, ramp->health - amount);
            result.ramp = *ramp;
            result.destroyed = ramp->health <= 0.0;
        }
    }
    if (result.destroyed) {
        static_cast<void>(structuralGraph_.remove(id));
        static_cast<void>(eraseInstance(id, true));
        syncStructuralStates();
    }
    return result;
}

ModularBuildingRepairResult FoundationSystem::repair(
    EntityId id, int wood, int stone) {
    ModularBuildingRepairResult result{
        .error = BuildingActionError::NotFound,
        .id = id,
    };
    double* health = nullptr;
    double maxHealth = 0.0;
    ResourceCost fullCost;
    PlatformFrameInstance* targetFrame = nullptr;
    WallInstance* targetWall = nullptr;
    RampInstance* targetRamp = nullptr;
    StructuralSupportState supportState =
        StructuralSupportState::Unsupported;

    const auto frame = std::find_if(
        platformFrames_.begin(), platformFrames_.end(),
        [id](const PlatformFrameInstance& candidate) {
            return candidate.id == id;
        });
    if (frame != platformFrames_.end()) {
        targetFrame = &*frame;
        health = &frame->health;
        maxHealth = frame->maxHealth;
        fullCost = {.wood = 8, .stone = 12};
        supportState = frame->supportState;
    } else {
        const auto wall = std::find_if(
            walls_.begin(), walls_.end(),
            [id](const WallInstance& candidate) {
                return candidate.id == id;
            });
        if (wall != walls_.end()) {
            targetWall = &*wall;
            health = &wall->health;
            maxHealth = wall->maxHealth;
            fullCost = {.wood = 4, .stone = 6};
            supportState = wall->supportState;
        } else {
            const auto ramp = std::find_if(
                ramps_.begin(), ramps_.end(),
                [id](const RampInstance& candidate) {
                    return candidate.id == id;
                });
            if (ramp == ramps_.end()) {
                return result;
            }
            targetRamp = &*ramp;
            health = &ramp->health;
            maxHealth = ramp->maxHealth;
            fullCost = {.wood = 6, .stone = 10};
            supportState = ramp->supportState;
        }
    }

    if (supportState != StructuralSupportState::Supported) {
        result.error = BuildingActionError::Unsupported;
        return result;
    }
    if (*health >= maxHealth) {
        result.error = BuildingActionError::FullHealth;
        return result;
    }
    result.cost =
        modularRepairCost(*health, maxHealth, fullCost);
    if (wood < result.cost.wood ||
        stone < result.cost.stone) {
        result.error =
            BuildingActionError::InsufficientResources;
        return result;
    }

    result.error = BuildingActionError::None;
    result.repairedHealth = maxHealth - *health;
    *health = maxHealth;
    if (targetFrame) {
        result.platformFrame = *targetFrame;
    } else if (targetWall) {
        result.wall = *targetWall;
    } else if (targetRamp) {
        result.ramp = *targetRamp;
    }
    return result;
}

std::size_t FoundationSystem::clear() {
    const std::size_t removed =
        platformFrames_.size() +
        walls_.size() + ramps_.size();
    reset();
    return removed;
}

bool FoundationSystem::updateStructuralSupport(
    double deltaSeconds) {
    collapsedBuildings_.clear();
    const std::vector<EntityId> collapsed =
        structuralGraph_.update(
            deltaSeconds,
            structuralCollapseEnabled_,
            structuralCollapseDelay_);
    if (collapsed.empty()) {
        return false;
    }
    for (const EntityId id : collapsed) {
        ModularBuildingDamageResult result{
            .id = id,
            .destroyed = true,
        };
        const auto frame = std::find_if(
            platformFrames_.begin(),
            platformFrames_.end(),
            [id](const PlatformFrameInstance& candidate) {
                return candidate.id == id;
            });
        const auto wall = std::find_if(
            walls_.begin(), walls_.end(),
            [id](const WallInstance& candidate) {
                return candidate.id == id;
            });
        const auto ramp = std::find_if(
            ramps_.begin(), ramps_.end(),
            [id](const RampInstance& candidate) {
                return candidate.id == id;
            });
        if (frame != platformFrames_.end()) {
            result.platformFrame = *frame;
        } else if (wall != walls_.end()) {
            result.wall = *wall;
        } else if (ramp != ramps_.end()) {
            result.ramp = *ramp;
        }
        collapsedBuildings_.push_back(result);
        static_cast<void>(eraseInstance(id, true));
    }
    syncStructuralStates();
    return true;
}

std::vector<ModularBuildingDamageResult>
FoundationSystem::takeCollapsedBuildings() {
    std::vector<ModularBuildingDamageResult> result;
    result.swap(collapsedBuildings_);
    return result;
}

void FoundationSystem::setStructuralCollapseEnabled(
    bool enabled) {
    structuralCollapseEnabled_ = enabled;
}

bool FoundationSystem::structuralCollapseEnabled() const {
    return structuralCollapseEnabled_;
}

void FoundationSystem::setStructuralCollapseDelay(
    double seconds) {
    structuralCollapseDelay_ = std::max(0.0, seconds);
}

const StructuralSupportGraph&
FoundationSystem::structuralGraph() const {
    return structuralGraph_;
}

void FoundationSystem::syncStructuralStates() {
    const auto sync =
        [this](auto& instances) {
            for (auto& instance : instances) {
                instance.supportState =
                    structuralGraph_.state(instance.id);
            }
        };
    sync(platformFrames_);
    sync(walls_);
    sync(ramps_);
}

} // namespace ian
