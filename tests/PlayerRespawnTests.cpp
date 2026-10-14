#include "TestHarness.hpp"
#include "game/PlayerRespawn.hpp"
#include "game/Simulation.hpp"
#include <memory>

int main() {
    using namespace ian;
    WorldConfig config = WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainAmplitude = 0;
    config.terrainSurfaceNoiseAmplitude = 0;
    config.pondMinimumCount = config.pondMaximumCount = 0;
    TerrainHeightfield terrain(config);
    terrain.generate(73);
    CollisionWorld crowded(64, {{-4,4,-4,4}});
    const Vec3 fallback{20,0,20};
    const auto expanded = findPlayerRespawn(fallback, Vec3{}, terrain, crowded, {}, 1.7);
    require(crowded.canOccupyCircle(expanded,.43) && std::hypot(expanded.x,expanded.z)<16,
            "blocked four legacy offsets search further around the core");
    require(!crowded.canOccupyCircle({0,1.7,0},.43), "spawn query rejects occupied geometry");
    require(!crowded.canOccupyCircle({64,1.7,0},.43), "spawn query respects map boundaries");
    CollisionWorld cylinders(64, {});
    ResourceNode barrel{};
    barrel.type=ResourceType::Barrel;barrel.position={8,0,8};barrel.radius=1;barrel.visualScale=1;barrel.active=true;
    const std::array resourceNodes{barrel};
    cylinders.syncResourceCylinders(resourceNodes,{});
    require(!cylinders.canOccupyCircle({8,1.0,8},.43),
        "spawn query includes resource cylinders, not only building boxes");
    require(cylinders.canOccupyCircle({8,3.0,8},.43),
        "spawn query respects the resource collider's vertical interval");
    CollisionWorld open(64, {});
    std::array<EnemyInstance,1> enemies{};
    enemies[0].type = EnemyType::Basic;
    enemies[0].health = 100;
    enemies[0].active = true;
    enemies[0].position = {0,0,3};
    const auto safe = findPlayerRespawn(fallback, Vec3{}, terrain, open, enemies, 1.7);
    require(std::hypot(safe.x,safe.z-3)>3,
            "respawn prefers clearance from living enemy cylinders");
    const auto withoutCore = findPlayerRespawn(fallback, {}, terrain, open, {}, 1.7);
    require(withoutCore.x==20 && withoutCore.z==20, "before a core, respawn uses the map spawn");

    auto balance=GameBalance::defaults();
    balance.gameplay.playerRespawnSeconds=.25;
    auto storage=std::make_unique<Simulation>(balance);
    auto& simulation=*storage;
    simulation.startRun();
    PlayerCommand kill;
    kill.damagePlayer=DamagePlayerCommand{10000};
    simulation.tick(.016,kill);
    require(simulation.snapshot().playerRespawning,"lethal hit starts respawn");
    const Vec3 death=simulation.snapshot().playerPosition;
    simulation.tick(.10,PlayerCommand{.moveForward=1,.lookYaw=1,.jump=true});
    require(simulation.snapshot().playerPosition.x==death.x &&
            simulation.snapshot().playerPosition.y==death.y &&
            simulation.snapshot().playerPosition.z==death.z,
            "dead body does not continue falling or moving");
    simulation.tick(.16);
    const auto& alive=simulation.snapshot();
    require(!alive.playerRespawning && alive.playerHealth==alive.playerMaxHealth &&
            alive.playerRecoverableArmor==alive.playerMaxRecoverableArmor && alive.playerInvulnerable &&
            alive.playerRespawnProtectionRemaining==PlayerRespawnProtectionSeconds,
            "respawn restores health and shield with separate temporary protection");
    simulation.tick(.016,kill);
    require(!simulation.snapshot().playerRespawning,"spawn protection prevents an immediate death loop");
    const double protection=simulation.snapshot().playerRespawnProtectionRemaining;
    simulation.togglePause();simulation.tick(1);
    require(simulation.snapshot().playerRespawnProtectionRemaining==protection,"protection freezes during pause");
    simulation.togglePause();simulation.tick(2.6);
    require(!simulation.snapshot().playerInvulnerable,"spawn protection expires without altering god mode");
    simulation.tick(.016,kill);
    require(simulation.snapshot().playerRespawning,"normal damage resumes after protection");
    simulation.restartRun();
    require(!simulation.snapshot().playerRespawning &&
            simulation.snapshot().playerRespawnProtectionRemaining==0 && !simulation.snapshot().playerInvulnerable,
            "restart clears death and immunity state");
}
