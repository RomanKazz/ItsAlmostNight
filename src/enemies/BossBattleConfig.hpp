#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {
struct BossBattleConfig {
    double phaseTwo{.60};
    double phaseThree{.30};
    double introSeconds{1.6};
    double transitionSeconds{1.0};
    double transitionDamageMultiplier{.35};
    double volleyWindup{.85};
    double diveWindup{1.35};
    double stormWindup{1.45};
    double summonWindup{1.15};
    double recoverySeconds{2.6};
    double projectileSpeed{8.5};
    double volleyDamageScale{.55};
    double diveRadius{2.7};
    double diveDamageScale{1.2};
    double stormRadius{1.9};
    double stormDamageScale{.85};
    double structureDamageScale{.35};
    double summonHealthMultiplier{7.0};
    double summonDamageMultiplier{1.4};
    int summonLimit{12};
    [[nodiscard]] bool valid() const {
        const std::array values{introSeconds,transitionSeconds,volleyWindup,diveWindup,stormWindup,
            summonWindup,recoverySeconds,projectileSpeed,volleyDamageScale,diveRadius,diveDamageScale,
            stormRadius,stormDamageScale,structureDamageScale,summonHealthMultiplier,summonDamageMultiplier};
        return std::ranges::all_of(values,[](double v){return std::isfinite(v) && v>0;}) &&
            std::isfinite(phaseTwo) && std::isfinite(phaseThree) &&
            std::isfinite(transitionDamageMultiplier) && transitionDamageMultiplier>0 && transitionDamageMultiplier<=1 &&
            phaseThree>0 && phaseThree<phaseTwo && phaseTwo<1 && summonLimit>0 && summonLimit<=32;
    }
};
enum class RoyalAttack { Volley, Dive, Storm, Summon };
inline constexpr double AlpakingHover = .7;
inline constexpr double AlpakingBodyCenter = 2.325;
} // namespace ian
