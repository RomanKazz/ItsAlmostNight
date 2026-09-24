#include "enemies/EnemySystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double BossPhaseTransitionDuration = 1.05;
constexpr double BossSlamWindupDuration = 1.25;
constexpr double BossWarCryWindupDuration = 1.35;
constexpr double BossSlamRadius = 5.25;
constexpr double BossPhaseTwoSlamDamage = 28.0;
constexpr double BossPhaseThreeSlamDamage = 36.0;
} // namespace

bool EnemySystem::advanceBossAbility(
    EnemyInstance& enemy, double deltaSeconds,
    std::optional<Vec3> playerPosition) {
    if (enemy.type == EnemyType::Boss) {
        const double healthRatio = enemy.maxHealth > 0.0
            ? enemy.health / enemy.maxHealth
            : 0.0;
        const int desiredPhase = healthRatio <= 1.0 / 3.0
            ? 3
            : healthRatio <= 2.0 / 3.0 ? 2 : 1;
        if (desiredPhase > enemy.bossPhase &&
            enemy.state != EnemyState::BossPhaseTransition) {
            enemy.bossPhase = desiredPhase;
            enemy.state = EnemyState::BossPhaseTransition;
            enemy.target.reset();
            enemy.ramWindupRemaining = 0.0;
            enemy.bossAbilityWindupRemaining =
                BossPhaseTransitionDuration;
            enemy.bossAbilityCooldownRemaining = 0.0;
            enemy.bossWarCryNext = desiredPhase >= 3;
            bossActionEventBuffer_.push_back({
                .type = BossActionType::PhaseChanged,
                .bossId = enemy.id,
                .position = enemy.position,
                .phase = desiredPhase,
            });
        }

        if (enemy.state == EnemyState::BossPhaseTransition) {
            enemy.bossAbilityWindupRemaining = std::max(
                0.0,
                enemy.bossAbilityWindupRemaining - deltaSeconds);
            if (enemy.bossAbilityWindupRemaining <= 0.0) {
                if (enemy.bossPhase >= 3) {
                    enemy.state = EnemyState::BossWarCryWindup;
                    enemy.bossAbilityWindupRemaining =
                        BossWarCryWindupDuration;
                } else {
                    enemy.state = EnemyState::MoveToCore;
                }
            }
            return true;
        }
        if (enemy.state == EnemyState::BossSlamWindup) {
            enemy.bossAbilityWindupRemaining = std::max(
                0.0,
                enemy.bossAbilityWindupRemaining - deltaSeconds);
            if (enemy.bossAbilityWindupRemaining <= 0.0) {
                bossActionEventBuffer_.push_back({
                    .type = BossActionType::GroundSlam,
                    .bossId = enemy.id,
                    .position = enemy.position,
                    .phase = enemy.bossPhase,
                    .radius = BossSlamRadius,
                    .damage = enemy.bossPhase >= 3
                        ? BossPhaseThreeSlamDamage
                        : BossPhaseTwoSlamDamage,
                });
                enemy.state = EnemyState::MoveToCore;
                enemy.bossAbilityCooldownRemaining =
                    enemy.bossPhase >= 3 ? 6.5 : 8.0;
                enemy.bossWarCryNext = enemy.bossPhase >= 3;
            }
            return true;
        }
        if (enemy.state == EnemyState::BossWarCryWindup) {
            enemy.bossAbilityWindupRemaining = std::max(
                0.0,
                enemy.bossAbilityWindupRemaining - deltaSeconds);
            if (enemy.bossAbilityWindupRemaining <= 0.0) {
                bossActionEventBuffer_.push_back({
                    .type = BossActionType::WarCry,
                    .bossId = enemy.id,
                    .position = enemy.position,
                    .phase = enemy.bossPhase,
                    .radius = 3.5,
                });
                enemy.state = EnemyState::MoveToCore;
                enemy.bossAbilityCooldownRemaining = 9.5;
                enemy.bossWarCryNext = false;
            }
            return true;
        }

        if (enemy.bossPhase >= 2 &&
            enemy.bossAbilityCooldownRemaining <= 0.0) {
            const double playerDistance = playerPosition
                ? std::hypot(
                      playerPosition->x - enemy.position.x,
                      playerPosition->z - enemy.position.z)
                : std::numeric_limits<double>::infinity();
            if (enemy.bossPhase >= 3 &&
                enemy.bossWarCryNext) {
                enemy.state = EnemyState::BossWarCryWindup;
                enemy.bossAbilityWindupRemaining =
                    BossWarCryWindupDuration;
                return true;
            }
            if (playerDistance <= BossSlamRadius + 1.25) {
                enemy.state = EnemyState::BossSlamWindup;
                enemy.bossAbilityWindupRemaining =
                    BossSlamWindupDuration;
                return true;
            }
        }
    }
    return false;
}

} // namespace ian
