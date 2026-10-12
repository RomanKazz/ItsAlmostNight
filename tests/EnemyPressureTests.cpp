#include "TestHarness.hpp"
#include "audio/ExplosionSoundBudget.hpp"
#include "enemies/EnemyDifficulty.hpp"
#include "enemies/EnemySystem.hpp"
#include "waves/WaveDirector.hpp"

#include <array>
#include <cmath>
#include <limits>

int main() {
    using namespace ian;
    WaveDirector director;
    double previousHealth = 0.0;
    for (const int wave : {1, 2, 3, 6, 12, 20, 50, 1000000}) {
        const auto plan = director.buildWave(wave, {0, 0});
        const double health = plan.spawns.front().healthMultiplier;
        require(std::isfinite(health) && health > previousHealth,
                "actual wave HP grows throughout an endless run");
        if (wave >= 6) {
            require(health > .85 * (1 + .1 * (wave - 1)) * 1.8,
                    "late waves materially outscale old linear HP growth");
        }
        previousHealth = health;
    }
    requireNear(enemyWaveHealthGrowth(6), 3.075, 1e-12, "wave six health growth");
    requireNear(enemyWaveHealthGrowth(12), 7.875, 1e-12, "wave twelve health growth");
    require(std::isfinite(enemyWaveHealthGrowth(std::numeric_limits<int>::max())),
            "HP growth does not overflow at maximum wave ID");

    for (const auto affix : {EliteAffix::Berserker, EliteAffix::Warden, EliteAffix::Volatile}) {
        EnemySystem enemies;
        const std::array<EnemySpawn, 2> spawns{{
            {.type = EnemyType::Basic, .position = {0, .8, 0}},
            {.type = EnemyType::Basic, .position = {20, .8, 0},
             .eliteAffixes = eliteAffixMask(affix)},
        }};
        enemies.spawnWave(spawns);
        requireNear(enemies.enemies()[1].maxHealth,
                    enemies.enemies()[0].maxHealth * 4, 1e-9, "each elite affix gains fourfold HP");
        requireNear(enemies.enemies()[1].damage,
                    enemies.enemies()[0].damage * 1.5, 1e-9, "each elite affix gains extra damage");
    }
    for (const bool areaKill : {false, true}) {
        EnemySystem enemies;
        const std::array<EnemySpawn, 1> spawns{{{
            .type = EnemyType::Splitter, .position = {0, 1.05, -5},
            .healthMultiplier = 2.5, .damageMultiplier = 1.25,
            .eliteAffixes = eliteAffixMask(EliteAffix::Volatile),
        }}};
        enemies.spawnWave(spawns);
        if (areaKill) static_cast<void>(enemies.damageInRadius(spawns[0].position, 3, 1000));
        else static_cast<void>(enemies.damage(enemies.enemies()[0].id, 1000));
        FlowField flow;
        flow.rebuild({0, 0}, {});
        enemies.tick(.4, {}, flow);
        require(enemies.activeCount() == 3, "elite splitter still creates three children");
        for (const auto& child : enemies.enemies()) {
            if (!child.active) continue;
            require(child.eliteAffixes == 0, "ordinary children never inherit elite affixes");
            requireNear(child.maxHealth, 8, 1e-9, "child retains wave scaling without elite HP");
            requireNear(child.damage, 8.75, 1e-9, "child retains wave scaling without elite damage");
        }
    }

    ExplosionSoundBudget budget;
    int starts = 0;
    for (int frame = 0; frame < 600; ++frame) {
        budget.tick(1.0 / 60.0);
        for (int explosion = 0; explosion < 30; ++explosion)
            if (budget.reserve(0)) ++starts;
    }
    require(starts >= 60 && starts <= 72, "artillery spam admits at most one onset per cooldown");
    budget.reset();
    require(!budget.reserve(3), "full voice pool preserves all playing tails");
    require(budget.reserve(2).has_value(), "full-pool rejection does not consume budget");
    float sum = 0;
    for (std::size_t voices = 0; voices < 3; ++voices) {
        budget.reset();
        sum += *budget.reserve(voices);
    }
    require(sum < 1, "explosion layers retain audio headroom");
}
