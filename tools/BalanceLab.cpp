#include "buildings/BuildingSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "enemies/EnemyDifficulty.hpp"
#include "navigation/FlowField.hpp"
#include "progression/DefenseAttackModifiers.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using namespace ian;
constexpr double Step = 1.0 / 60.0;
constexpr std::array Types{BuildingType::GunTurret, BuildingType::Turret,
    BuildingType::Cannon, BuildingType::Catapult, BuildingType::Flamethrower,
    BuildingType::MissileLauncher, BuildingType::TeslaTower, BuildingType::LaserTower,
    BuildingType::Mortar, BuildingType::SpikeTrap, BuildingType::SlowTrap, BuildingType::WindBlower};
constexpr std::array Names{"gun_turret", "crossbow", "cannon", "catapult", "flamethrower",
    "missile_launcher", "tesla", "laser", "mortar", "spikes", "slow_trap", "wind_blower"};
constexpr std::array Scenes{"heavy", "dense", "fast", "flying"};
constexpr std::array Profiles{"base", "damage_rate", "quantity", "quantity_bounce_pierce"};
constexpr std::array Levels{1, 4, 8};

struct Result {
    double damage{}, slowSeconds{}, pushSeconds{};
    int spawned{}, kills{}, leaked{}, alive{}, hits{}, launches{};
    int peakProjectiles{}, dropped{}, applicableCards{};
    FinalAttackStats stats{};
};

std::array<double, RunUpgradeEffectCount> powers(int profile) {
    std::array<double, RunUpgradeEffectCount> result{};
    const auto add = [&](RunUpgradeEffect effect, double amount) { result[runUpgradeIndex(effect)] = amount; };
    if (profile == 1) {
        // Damage / AttackSpeed tomes apply these defense halves in Simulation.
        add(RunUpgradeEffect::DefenseDamage, 2); add(RunUpgradeEffect::DefenseFireRate, 2);
    } else if (profile == 2) add(RunUpgradeEffect::ProjectileCount, 4);
    else if (profile == 3) {
        add(RunUpgradeEffect::ProjectileCount, 2); add(RunUpgradeEffect::Bounces, 1);
        add(RunUpgradeEffect::Pierce, 1);
    }
    return result;
}

Result battle(const GameBalance& balance, BuildingType type, int level, int scene,
              int profile, int seed, int ticks) {
    // The stand measures weapons, not structure durability or terrain placement.
    // Enemy navigation sees only the core: the test weapon cannot body-block a lane.
    const std::vector<BuildingInstance> targets{{.id={1000,1}, .type=BuildingType::Core,
        .gridPosition={0,6}, .level=8, .health=1000000, .maxHealth=1000000}};
    std::vector<BuildingInstance> buildings = targets;
    if (type != BuildingType::Core) buildings.push_back({.id={1001,1}, .type=type,
        .gridPosition={0,0}, .level=static_cast<std::uint8_t>(level), .health=1000, .maxHealth=1000});
    auto enemies = std::make_unique<EnemySystem>(balance.enemies);
    auto towers = std::make_unique<TowerSystem>();
    auto cannons = std::make_unique<CannonSystem>();
    auto traps = std::make_unique<TrapSystem>();
    const auto power = powers(profile);
    const auto modifiers = defenseAttackCardModifiers(power);
    towers->setAttackModifiers(modifiers); cannons->setAttackModifiers(modifiers);
    traps->setAttackModifiers(modifiers);
    towers->syncBuildings(buildings); cannons->syncBuildings(buildings); traps->syncBuildings(buildings);
    FlowField flow; flow.rebuild({0,6}, targets);
    Result result;
    const bool ballistic = type == BuildingType::Cannon || type == BuildingType::Catapult || type == BuildingType::Mortar;
    const bool trap = type == BuildingType::SpikeTrap || type == BuildingType::SlowTrap;
    if (type != BuildingType::Core) {
        result.stats = ballistic ? cannons->finalStats(buildings.back()) :
            trap ? traps->finalStats(buildings.back()) : towers->finalStats(buildings.back());
        for (const auto& binding : DefenseAttackUpgradeBindings)
            if (supportsAttackStat(type,binding.stat))
                result.applicableCards += static_cast<int>(power[runUpgradeIndex(binding.effect)]);
    }
    const int population = scene == 0 ? 1 : scene == 1 ? 32 : 12;
    const EnemyType enemyType = scene == 0 ? EnemyType::Heavy : scene == 1 ? EnemyType::Basic :
        scene == 2 ? EnemyType::Fast : EnemyType::Flying;
    const double healthScale = enemyWaveHealthGrowth(level);
    std::vector<EnemySpawn> spawns;
    std::array<double, EnemySystem::MaxEnemies> previousHealth{};
    const auto replenish = [&] {
        spawns.clear();
        const int needed = population - static_cast<int>(enemies->activeCount());
        for (int i=0; i<needed; ++i) {
            const int serial = result.spawned + i;
            const double jitter = (static_cast<double>(seed % 7)-3)*.07;
            const double x = scene == 0 ? jitter : scene == 1 ?
                (static_cast<double>(serial % 4)-1.5)*1.25+jitter :
                (static_cast<double>((serial*5+seed) % 9)-4)*1.05;
            const double z = scene == 0 ? -10 : scene == 1 ?
                -8-static_cast<double>((serial / 4) % 8)*1.35 : -8-static_cast<double>(serial % 6)*2;
            spawns.push_back({.type=enemyType, .position={x,0,z}, .healthMultiplier=healthScale});
        }
        enemies->spawnGroup(spawns); result.spawned += needed;
    };
    const auto hit = [&](const EnemyDamageResult& value) {
        ++result.hits; result.damage += value.damage; result.kills += value.killed ? 1 : 0;
    };
    for (int tick=0; tick<ticks; ++tick) {
        replenish();
        static_cast<void>(enemies->tick(Step,targets,flow));
        for (const auto& enemy : enemies->enemies()) {
            if (!enemy.active) continue;
            result.slowSeconds += Step * std::clamp(1-enemy.movementMultiplier,0.0,1.0);
            if (enemy.knockbackVelocity.x*enemy.knockbackVelocity.x +
                enemy.knockbackVelocity.z*enemy.knockbackVelocity.z > .01) result.pushSeconds += Step;
        }
        // Actual hits are already clamped to remaining HP by EnemySystem::damage.
        // Health deltas independently validate accounting (no summoned actors here).
        std::size_t index=0;
        for (const auto& enemy : enemies->enemies()) previousHealth[index++] = enemy.active ? enemy.health : 0;
        const double damageBefore = result.damage;
        for (const auto& shot : towers->tick(Step,buildings,*enemies)) {
            ++result.hits; result.damage += shot.damage; result.kills += shot.killed ? 1 : 0;
        }
        result.launches += static_cast<int>(towers->launches().size());
        static_cast<void>(cannons->tick(Step,buildings,*enemies));
        result.launches += static_cast<int>(cannons->shots().size());
        for (const auto& value : cannons->hits()) hit(value.result);
        static_cast<void>(traps->tick(Step,buildings,*enemies));
        for (const auto& value : traps->hits()) hit(value.result);
        int activeProjectiles=0;
        for (const auto& projectile : towers->projectiles()) activeProjectiles += projectile.active ? 1 : 0;
        for (const auto& projectile : cannons->projectiles()) activeProjectiles += projectile.active ? 1 : 0;
        result.peakProjectiles = std::max(result.peakProjectiles,activeProjectiles);
        index=0;
        double measuredDamage=0;
        for (const auto& enemy : enemies->enemies()) {
            measuredDamage += std::max(0.0,previousHealth[index++] - (enemy.active ? enemy.health : 0.0));
        }
        // Catch dropped/double-counted combat events independently of hit buffers.
        if (!std::isfinite(measuredDamage) ||
            std::abs(measuredDamage-(result.damage-damageBefore)) > 1e-7 * (1+measuredDamage))
            throw std::runtime_error("hit-buffer/health accounting mismatch");
        for (const auto& enemy : enemies->enemies()) {
            // A leak crosses a fixed transverse line before the core. Removing it
            // is not a weapon kill. Ground/air use the same line, and refill next tick.
            if (enemy.active && (enemy.position.z >= 4.0 || enemy.state == EnemyState::AttackCore)) {
                ++result.leaked;
                static_cast<void>(enemies->damage(enemy.id,enemy.health*100+1));
            }
        }
    }
    result.alive = static_cast<int>(enemies->activeCount());
    result.dropped = static_cast<int>(towers->droppedProjectiles()+cannons->droppedProjectiles());
    if (result.spawned != result.kills+result.leaked+result.alive || !std::isfinite(result.damage))
        throw std::runtime_error("enemy accounting mismatch");
    return result;
}

int integer(std::string_view input, int minimum, int maximum) {
    int result{}; const auto parsed=std::from_chars(input.data(),input.data()+input.size(),result);
    if (parsed.ec!=std::errc{} || parsed.ptr!=input.data()+input.size() || result<minimum || result>maximum)
        throw std::runtime_error("invalid numeric argument");
    return result;
}

void check(bool valid, const char* message) { if (!valid) throw std::runtime_error(message); }
void selfTest(const GameBalance& balance) {
    const auto base=battle(balance,BuildingType::GunTurret,4,0,0,1,600);
    const auto again=battle(balance,BuildingType::GunTurret,4,0,0,1,600);
    const auto multi=battle(balance,BuildingType::GunTurret,4,0,2,1,600);
    check(base.damage>0 && base.damage==again.damage && base.kills==again.kills && base.leaked==again.leaked,
        "deterministic actual-combat baseline");
    check(multi.damage>base.damage && multi.stats.quantity==base.stats.quantity+4,
        "additional projectiles improve actual single-target throughput");
    const auto flame=battle(balance,BuildingType::Flamethrower,4,1,0,1,300);
    const auto ignored=battle(balance,BuildingType::Flamethrower,4,1,2,1,300);
    check(flame.damage>0 && flame.damage==ignored.damage && ignored.applicableCards==0,
        "unsupported cards do not silently boost flame DPS");
    const auto flying=battle(balance,BuildingType::WindBlower,4,3,0,1,300);
    check(flying.damage==0 && flying.pushSeconds==0,"wind does not affect flying targets");
    const auto slow=battle(balance,BuildingType::SlowTrap,4,1,0,1,600);
    check(slow.damage==0 && slow.slowSeconds>0,"control is measured independently from damage");
    std::cout << "Balance lab actual-combat checks passed\n";
}
}

int main(int argc, char** argv) {
    try {
        std::filesystem::path data="assets/data", output="output/balance_lab/raw.csv";
        int seconds=30, seeds=1;
        bool test=false;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            if (arg=="--self-test") {test=true;continue;}
            if (arg=="--help") {
                std::cout << "ian_balance_lab [--output CSV] [--data DIR] [--seconds 1..120] [--seeds 1..8] [--self-test]\n";
                return 0;
            }
            if (i+1==argc) throw std::runtime_error("missing option value");
            const std::string_view value=argv[++i];
            if (arg=="--output") output=value;
            else if (arg=="--data") data=value;
            else if (arg=="--seconds") seconds=integer(value,1,120);
            else if (arg=="--seeds") seeds=integer(value,1,8);
            else throw std::runtime_error("unknown option");
        }
        const auto path=[&](const char* name) {return (data/name).string();};
        const auto loaded=loadGameBalance(path("enemies.json"),path("waves.json"),path("buildings.json"),
            path("weapons.json"),path("economy.json"),path("gameplay.json"));
        if (!loaded.valid()) {
            for (const auto& error:loaded.errors) std::cerr << error << '\n';
            return 1;
        }
        if (test) {selfTest(loaded.balance);return 0;}
        if (!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());
        std::ofstream csv(output); csv.exceptions(std::ios::badbit|std::ios::failbit);
        csv << "building,level,scene,profile,seed,seconds,bonus_cards,applicable_cards,wood,stone,crystals,"
            "spawned,kills,leaked,alive,damage_hp,dps_hp,slow_enemy_seconds,push_enemy_seconds,hits,launches,"
            "peak_projectiles,dropped_projectiles,quantity,bounces,pierce,attack_interval,damage_per_hit_hp,enemy_health_scale\n";
        csv << std::setprecision(12);
        for (int level:Levels) for (std::size_t scene=0;scene<Scenes.size();++scene) {
            for (int seed=1;seed<=seeds;++seed) for (std::size_t type=0;type<=Types.size();++type) {
                const bool baseline=type==Types.size();
                const auto building=baseline ? BuildingType::Core : Types[type];
                const auto& cost=loaded.balance.buildings[static_cast<std::size_t>(building)];
                for (int profile=0;profile<(baseline ? 1 : 4);++profile) {
                    const auto result=battle(loaded.balance,building,level,static_cast<int>(scene),profile,seed,seconds*60);
                    csv << (baseline ? "no_defense" : Names[type]) << ',' << level << ',' << Scenes[scene] << ','
                        << Profiles[static_cast<std::size_t>(profile)] << ',' << seed << ',' << seconds << ','
                        << (profile ? 4 : 0) << ',' << result.applicableCards << ','
                        << (baseline ? 0 : cost.wood) << ',' << (baseline ? 0 : cost.stone) << ',' << (baseline ? 0 : cost.crystals) << ','
                        << result.spawned << ',' << result.kills << ',' << result.leaked << ',' << result.alive << ','
                        << result.damage*10 << ',' << result.damage*10/seconds << ',' << result.slowSeconds << ','
                        << result.pushSeconds << ',' << result.hits << ',' << result.launches << ',' << result.peakProjectiles << ','
                        << result.dropped << ',' << result.stats.quantity << ',' << result.stats.bounces << ',' << result.stats.pierce << ','
                        << result.stats.attackInterval << ',' << result.stats.damage*10 << ',' << enemyWaveHealthGrowth(level) << '\n';
                }
            }
            std::cerr << "Level " << level << ": " << Scenes[scene] << " complete\n";
        }
        csv.close();
        return 0;
    } catch (const std::exception& error) {std::cerr << "Balance lab: " << error.what() << '\n';return 1;}
}
