#pragma once

#include "graphics/Renderer.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace ian {

// Render-only spring motion, shared by individual draws, instanced crowds,
// shadows and selection masks. Foot pivot and gameplay positions stay intact.
inline Matrix enemyPresentationTransform(
    EnemyModelVisual visual, EnemyAnimationVisual animation, float seconds,
    Vector3 position, float yaw, float scale,
    Vector3 bodyStretch = {1, 1, 1}, float bodyRoll = 0) {
    const float time = std::isfinite(seconds) ? std::max(0.0F, seconds) : 0.0F;
    const bool heavy = visual == EnemyModelVisual::Warrior ||
        visual == EnemyModelVisual::Boss || visual == EnemyModelVisual::Splitter;
    const float weight = heavy ? 0.48F : 1.0F;
    float spring = 0.0F;
    float roll = 0.0F;
    float pitch = 0.0F;
    float hop = 0.0F;
    if (animation == EnemyAnimationVisual::Walk || animation == EnemyAnimationVisual::Run) {
        const float phase = time * 2.0F * PI / 0.72F;
        spring = std::sin(phase * 2.0F) * 0.055F * weight;
        roll = std::sin(phase) * 0.045F * weight;
        pitch = animation == EnemyAnimationVisual::Run ? -0.08F : -0.025F;
        if (visual != EnemyModelVisual::Bee && visual != EnemyModelVisual::BeeElite && visual != EnemyModelVisual::Flying && visual != EnemyModelVisual::Boss &&
            visual != EnemyModelVisual::BossSummon)
            hop = std::pow(std::max(0.0F, std::sin(phase * 2.0F)), 2.0F) * 0.045F * weight;
    } else if (animation == EnemyAnimationVisual::MeleeAttack ||
               animation == EnemyAnimationVisual::RangedAttack ||
               animation == EnemyAnimationVisual::SapperAttack) {
        // Cooldown time starts at zero after a hit. Snap then settle; load
        // the spring again near the end of the existing attack animation.
        const float snap = std::exp(-time * 12.0F);
        const float load = std::clamp((time - 0.65F) / 0.35F, 0.0F, 1.0F);
        const float windup = load * load * (3.0F - 2.0F * load);
        spring = (snap * 0.09F - windup * 0.055F) * weight;
        pitch = (-snap * 0.13F + windup * 0.06F) * weight;
    } else if (animation == EnemyAnimationVisual::Hit) {
        spring = -0.10F * std::exp(-time * 14.0F) * weight;
        pitch = 0.09F * std::exp(-time * 14.0F) * weight;
    }
    const float tall = 1.0F + spring;
    const float wide = 1.0F / std::sqrt(tall);
    const Matrix deform = MatrixMultiply(
        MatrixScale(scale * wide * bodyStretch.x, scale * tall * bodyStretch.y,
            scale * wide * bodyStretch.z),
        MatrixMultiply(MatrixRotateX(pitch), MatrixRotateZ(roll + bodyRoll)));
    position.y += hop * scale;
    return MatrixMultiply(deform, MatrixMultiply(MatrixRotateY(yaw),
        MatrixTranslate(position.x, position.y, position.z)));
}

} // namespace ian
