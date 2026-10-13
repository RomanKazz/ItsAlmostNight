#include "enemies/EnemySystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {
constexpr double Pi = 3.14159265358979323846;
double horizontalDistance(Vec3 a, Vec3 b) { return std::hypot(a.x-b.x,a.z-b.z); }
}

bool EnemySystem::advanceBossAbility(EnemyInstance& enemy,double deltaSeconds,
                                   std::optional<Vec3> playerPosition,Vec3 corePosition,
                                   const TerrainHeightfield* terrain,EnemyNavigationView navigation) {
    if (enemy.type != EnemyType::Boss) return false;
    const auto& definition = definitions_[static_cast<std::size_t>(EnemyType::Boss)];
    const auto& config = definition.bossBattle;
    const Vec3 impulse=enemy.knockbackVelocity;
    const double impulseDecay=std::max(0.0,1-deltaSeconds*5);
    enemy.knockbackVelocity.x*=impulseDecay;
    enemy.knockbackVelocity.z*=impulseDecay;
    const double ratio = enemy.maxHealth>0 ? enemy.health/enemy.maxHealth : 0;
    const int phase = ratio<=config.phaseThree ? 3 : ratio<=config.phaseTwo ? 2 : 1;
    const auto core=std::find_if(structureBuffer_.begin(),structureBuffer_.end(),[](const EnemyStructureTarget& s) {
        return s.attackable && s.buildingType==BuildingType::Core;
    });
    const bool hasCore=core!=structureBuffer_.end();
    const int sequence=enemy.bossAttackIndex%6;
    const bool siege=hasCore && (sequence==2 || sequence==4 || sequence==5);
    const bool targetPlayer = !siege && playerPosition && horizontalDistance(*playerPosition,enemy.position)<config.attackRange;
    Vec3 target = targetPlayer ? *playerPosition : corePosition;
    target.y = terrain ? terrain->getHeight(target.x,target.z) : corePosition.y;
    if(navigation.collisionWorld) {
        const double ceiling = targetPlayer ? playerPosition->y-1.60 : corePosition.y+.1;
        const auto floor = navigation.collisionWorld->modularSurfaceHeight(target.x,target.z,ceiling);
        if(floor) target.y = std::max(target.y,*floor);
    }
    const double targetYaw = std::atan2(target.x-enemy.position.x,target.z-enemy.position.z);
    const double turn = std::atan2(std::sin(targetYaw-enemy.yaw),std::cos(targetYaw-enemy.yaw));
    enemy.yaw += std::clamp(turn,-enemy.turnRate*deltaSeconds,enemy.turnRate*deltaSeconds);
    const auto ground = [&]() {return Vec3{enemy.position.x,enemy.worldSurfaceHeight,enemy.position.z};};
    const auto notifyPhase = [&]() {
        bossActionEventBuffer_.push_back({.type=BossActionType::PhaseChanged,.bossId=enemy.id,
            .position=ground(),.phase=enemy.bossPhase});
    };
    const auto warning = [&](Vec3 position,double radius,double seconds,RoyalAttack attack) {
        bossActionEventBuffer_.push_back({.type=BossActionType::Telegraph,.bossId=enemy.id,
            .position=position,.phase=enemy.bossPhase,.radius=radius,.attack=attack,.warningSeconds=seconds});
    };
    const auto impact = [&](Vec3 position,double radius,double damage,RoyalAttack attack) {
        bossActionEventBuffer_.push_back({.type=BossActionType::RoyalImpact,.bossId=enemy.id,
            .position=position,.phase=enemy.bossPhase,.radius=radius,.damage=damage,.attack=attack});
        // Use the shared structure damage/defeat pipeline, including modular
        // targets. A given blast emits one hit per target, never per visual puff.
        for(const auto& structure:structureBuffer_) {
            if (!structure.attackable || std::abs(structure.position.y-position.y)>2.5) continue;
            if (horizontalDistance(structure.position,position)<=radius+structure.radius*.35)
                attackBuffer_.push_back({enemy.id,structure.id,damage*config.structureDamageScale,false});
        }
    };
    if(enemy.state==EnemyState::Spawn || phase>enemy.bossPhase) {
        const bool intro=enemy.state==EnemyState::Spawn;
        enemy.bossPhase=phase;
        enemy.state=EnemyState::BossPhaseTransition;
        enemy.bossAbilityWindupRemaining=intro?config.introSeconds:config.transitionSeconds;
        enemy.bossWindupDuration=enemy.bossAbilityWindupRemaining;
        enemy.bossAbilityCooldownRemaining=0;
        enemy.bossStrikesRemaining=0;
        enemy.bossStormDamageTaken=0;
        enemy.bossStormInterruptDamage=0;
        enemy.bossInterruptRemaining=0;
        enemy.bossHoverOffset=AlpakingHover;
        enemy.position.y=AlpakingBodyCenter+enemy.bossHoverOffset;
        notifyPhase();
        return true;
    }
    if(enemy.state==EnemyState::BossPhaseTransition) {
        enemy.bossAbilityWindupRemaining=std::max(0.0,enemy.bossAbilityWindupRemaining-deltaSeconds);
        if(enemy.bossAbilityWindupRemaining<=0) enemy.state=EnemyState::MoveToCore;
        return true;
    }
    if(enemy.bossInterruptRemaining>0) {
        enemy.bossInterruptRemaining=std::max(0.0,enemy.bossInterruptRemaining-deltaSeconds);
        return true;
    }
    if(enemy.bossStrikesRemaining>0) {
        enemy.bossStrikeTimer-=deltaSeconds;
        // At most three pending strikes. Catch up safely for a coarse/debug tick.
        while(enemy.bossStrikesRemaining>0 && enemy.bossStrikeTimer<=0) {
            const int index=3-enemy.bossStrikesRemaining;
            impact(enemy.bossStrikeTargets[static_cast<std::size_t>(index)],
                config.stormRadius+(enemy.bossPhase-1)*.15,
                enemy.damage*config.stormDamageScale,RoyalAttack::Storm);
            --enemy.bossStrikesRemaining;
            enemy.bossStrikeTimer+=.3;
        }
        return true;
    }
    const bool winding = enemy.state==EnemyState::BossVolleyWindup ||
        enemy.state==EnemyState::BossSlamWindup || enemy.state==EnemyState::BossStormWindup ||
        enemy.state==EnemyState::BossWarCryWindup;
    if(winding) {
        enemy.bossAbilityWindupRemaining=std::max(0.0,enemy.bossAbilityWindupRemaining-deltaSeconds);
        if(enemy.state==EnemyState::BossSlamWindup) {
            const double p=1-enemy.bossAbilityWindupRemaining/std::max(.01,enemy.bossWindupDuration);
            const double travel=std::clamp((p-.55)/.45,0.0,1.0);
            const double ease=travel*travel*(3-2*travel);
            enemy.position.x=enemy.bossAttackOrigin.x+(enemy.bossAttackTarget.x-enemy.bossAttackOrigin.x)*ease;
            enemy.position.z=enemy.bossAttackOrigin.z+(enemy.bossAttackTarget.z-enemy.bossAttackOrigin.z)*ease;
            const double floor=terrain ? terrain->getHeight(enemy.position.x,enemy.position.z) : enemy.worldSurfaceHeight;
            enemy.bossHoverOffset=AlpakingHover+std::sin(p*Pi)*3.0+
                p*(enemy.bossAttackTarget.y-floor-.5-AlpakingHover);
            enemy.position.y=AlpakingBodyCenter+enemy.bossHoverOffset;
        }
        if(enemy.bossAbilityWindupRemaining>0) return true;
        if(enemy.state==EnemyState::BossVolleyWindup) {
            Vec3 origin{enemy.position.x+std::sin(enemy.yaw)*1.05,
                enemy.worldSurfaceHeight+enemy.position.y,enemy.position.z+std::cos(enemy.yaw)*1.05};
            const double angle=std::atan2(enemy.bossAttackTarget.x-origin.x,enemy.bossAttackTarget.z-origin.z);
            const double distance=std::max(.1,horizontalDistance(enemy.bossAttackTarget,origin));
            const double slope=(enemy.bossAttackTarget.y-origin.y)/distance;
            const int count=5+(enemy.bossPhase-1)*2;
            std::optional<EntityId> targetId;
            if(!enemy.bossAttackPlayer && hasCore) targetId=core->id;
            for(int bullet=0;bullet<count;++bullet) {
                const double a=angle+(bullet-(count-1)*.5)*.16;
                const double normalize=1/std::sqrt(1+slope*slope);
                projectiles_.push_back({.id={nextProjectileIndex_++,1U},.ownerId=enemy.id,.targetId=targetId,
                    .position=origin,.targetPosition=enemy.bossAttackTarget,.previousPosition=origin,
                    .velocity={std::sin(a)*normalize*config.projectileSpeed,slope*normalize*config.projectileSpeed,
                               std::cos(a)*normalize*config.projectileSpeed},
                    .damage=enemy.damage*config.volleyDamageScale*(enemy.bossAttackPlayer?1.0:config.structureDamageScale),.radius=.22,.targetRadius=1.2,.lifetimeRemaining=4.2,
                    .targetsPlayer=enemy.bossAttackPlayer,.royal=true,.duration=4.2});
            }
            magicEventBuffer_.push_back({enemy.id,origin,false,false,true});
            bossActionEventBuffer_.push_back({.type=BossActionType::Volley,.bossId=enemy.id,
                .position=origin,.phase=enemy.bossPhase,.count=count});
        } else if(enemy.state==EnemyState::BossSlamWindup) {
            const double floor=terrain ? terrain->getHeight(enemy.position.x,enemy.position.z) : enemy.worldSurfaceHeight;
            enemy.bossHoverOffset=enemy.bossAttackTarget.y-floor-.5;
            enemy.position.y=AlpakingBodyCenter+enemy.bossHoverOffset;
            impact(enemy.bossAttackTarget,config.diveRadius,enemy.damage*config.diveDamageScale,RoyalAttack::Dive);
        } else if(enemy.state==EnemyState::BossStormWindup) {
            enemy.bossStrikesRemaining=3;
            enemy.bossStrikeTimer=0;
        } else {
            const int active=static_cast<int>(std::count_if(enemies_.begin(),enemies_.end(),[&](const EnemyInstance& unit){
                return unit.active && unit.summonerId==enemy.id;}));
            const int count=std::clamp(3+(enemy.bossPhase-1),0,std::max(0,config.summonLimit-active));
            bossActionEventBuffer_.push_back({.type=BossActionType::WarCry,.bossId=enemy.id,
                .position=ground(),.phase=enemy.bossPhase,.radius=5.2,.attack=RoyalAttack::Summon,.count=count,
                .healthScale=config.summonHealthMultiplier*enemy.maxHealth/std::max(1.0,definition.health)/overtime_.health,
                .damageScale=config.summonDamageMultiplier*enemy.damage/std::max(1.0,definition.damage)/overtime_.damage});
        }
        enemy.state=EnemyState::MoveToCore;
        enemy.bossAbilityCooldownRemaining=config.recoverySeconds/(1+(enemy.bossPhase-1)*.32);
        return true;
    }
    if(enemy.bossAbilityCooldownRemaining<=0 && horizontalDistance(target,enemy.position)<config.attackRange) {
        constexpr std::array pattern{RoyalAttack::Summon,RoyalAttack::Volley,RoyalAttack::Storm,
                                     RoyalAttack::Dive,RoyalAttack::Volley,RoyalAttack::Storm};
        RoyalAttack attack=pattern[static_cast<std::size_t>(enemy.bossAttackIndex++)%pattern.size()];
        if(attack==RoyalAttack::Dive && !targetPlayer && hasCore) attack=RoyalAttack::Storm;
        const auto livingSummons=std::count_if(enemies_.begin(),enemies_.end(),[&](const EnemyInstance& unit){
            return unit.active && unit.summonerId==enemy.id;});
        if(attack==RoyalAttack::Summon && livingSummons>=config.summonLimit) attack=RoyalAttack::Volley;
        enemy.bossAttackOrigin=enemy.position;
        enemy.bossAttackTarget=target;
        enemy.bossAttackPlayer=targetPlayer;
        if(attack==RoyalAttack::Volley) {
            enemy.state=EnemyState::BossVolleyWindup;
            enemy.bossWindupDuration=config.volleyWindup*(1-.12*(enemy.bossPhase-1));
            enemy.bossAttackTarget.y=targetPlayer ? playerPosition->y-.62 : corePosition.y+1.0;
            if(targetPlayer) {
                const double lead=std::min(1.35,enemy.bossWindupDuration+
                    horizontalDistance(target,enemy.position)/config.projectileSpeed*.65);
                enemy.bossAttackTarget.x+=estimatedPlayerVelocity_.x*lead;
                enemy.bossAttackTarget.z+=estimatedPlayerVelocity_.z*lead;
            }
        } else if(attack==RoyalAttack::Dive) {
            enemy.state=EnemyState::BossSlamWindup;
            enemy.bossWindupDuration=config.diveWindup*(1-.12*(enemy.bossPhase-1));
            enemy.bossAttackRadius=config.diveRadius;
            // The king needs room for his wide wings at touchdown. Choose the
            // landing point before warning it, rather than clipping a building
            // or letting the contact solver move the attack after telegraphing.
            if(targetPlayer) {
                target.x+=estimatedPlayerVelocity_.x*enemy.bossWindupDuration*.8;
                target.z+=estimatedPlayerVelocity_.z*enemy.bossWindupDuration*.8;
            }
            const double bodyRadius=enemyCollisionCylinder(enemy).radius;
            for(int pass=0;pass<4;++pass) for(const auto& structure:structureBuffer_) {
                if(structure.traversable || std::abs(structure.position.y-target.y)>2.5) continue;
                const double radius=bodyRadius+structure.radius+.06;
                double dx=target.x-structure.position.x,dz=target.z-structure.position.z;
                const double distance=std::hypot(dx,dz);
                if(distance>=radius) continue;
                if(distance<1e-6) {dx=enemy.position.x-structure.position.x;dz=enemy.position.z-structure.position.z;}
                if(std::hypot(dx,dz)<1e-6) {dx=std::sin(enemy.yaw);dz=std::cos(enemy.yaw);}
                const double length=std::max(.001,std::hypot(dx,dz));
                target.x=structure.position.x+dx/length*radius;
                target.z=structure.position.z+dz/length*radius;
            }
            if(terrain) target.y=terrain->getHeight(target.x,target.z);
            if(navigation.collisionWorld) {
                const auto floor=navigation.collisionWorld->modularSurfaceHeight(target.x,target.z,enemy.bossAttackTarget.y+.15);
                if(floor) target.y=std::max(target.y,*floor);
            }
            enemy.bossAttackTarget=target;
            warning(target,config.diveRadius,enemy.bossWindupDuration,attack);
        } else if(attack==RoyalAttack::Storm) {
            enemy.state=EnemyState::BossStormWindup;
            enemy.bossStormDamageTaken=0;
            enemy.bossStormInterruptDamage=enemy.maxHealth*config.stormInterruptHealthFraction;
            enemy.bossWindupDuration=config.stormWindup*(1-.12*(enemy.bossPhase-1));
            enemy.bossAttackRadius=config.stormRadius+(enemy.bossPhase-1)*.15;
            const double dx=target.x-enemy.position.x,dz=target.z-enemy.position.z;
            const double length=std::max(.1,std::hypot(dx,dz));
            for(int i=0;i<3;++i) {
                const double side=(i-1)*3.5;
                enemy.bossStrikeTargets[static_cast<std::size_t>(i)]={target.x-dz/length*side,target.y,
                    target.z+dx/length*side};
                auto& strike=enemy.bossStrikeTargets[static_cast<std::size_t>(i)];
                if(hasCore && !targetPlayer) {
                    if(i==0) strike=corePosition;
                    else {
                        strike={target.x+(i==1?6.5:-6.5),target.y,target.z};
                        if(i==1 && playerPosition && horizontalDistance(*playerPosition,corePosition)>6 &&
                            horizontalDistance(*playerPosition,enemy.position)<config.attackRange) {
                            strike={playerPosition->x+estimatedPlayerVelocity_.x*enemy.bossWindupDuration*.8,
                                target.y,playerPosition->z+estimatedPlayerVelocity_.z*enemy.bossWindupDuration*.8};
                        }
                        if(i==2) {
                            double nearest=16;
                            for(const auto& structure:structureBuffer_) {
                                const double range=horizontalDistance(structure.position,corePosition);
                                if(structure.attackable && structure.id!=core->id && range>6 && range<nearest) {
                                    nearest=range;strike=structure.position;
                                }
                            }
                        }
                    }
                } else {
                    strike.x+=estimatedPlayerVelocity_.x*enemy.bossWindupDuration*.8;
                    strike.z+=estimatedPlayerVelocity_.z*enemy.bossWindupDuration*.8;
                }
                if(terrain) strike.y=terrain->getHeight(strike.x,strike.z);
                if(navigation.collisionWorld) {
                    const auto floor=navigation.collisionWorld->modularSurfaceHeight(strike.x,strike.z,target.y+.15);
                    if(floor) strike.y=std::max(strike.y,*floor);
                }
                warning(enemy.bossStrikeTargets[static_cast<std::size_t>(i)],
                    config.stormRadius+(enemy.bossPhase-1)*.15,enemy.bossWindupDuration+i*.3,attack);
            }
        } else {
            enemy.state=EnemyState::BossWarCryWindup;
            enemy.bossWindupDuration=config.summonWindup*(1-.12*(enemy.bossPhase-1));
        }
        enemy.bossAbilityWindupRemaining=enemy.bossWindupDuration;
        return true;
    }
    // Flying pursuit and lateral orbit keep the king engaged without the old
    // stationary melee/instant ram. Attacks lock targets only at windup start.
    enemy.position.x+=impulse.x*deltaSeconds;
    enemy.position.z+=impulse.z*deltaSeconds;
    const Vec3 pursuit=hasCore?corePosition:target;
    const double dx=pursuit.x-enemy.position.x,dz=pursuit.z-enemy.position.z;
    const double distance=std::max(.1,std::hypot(dx,dz));
    const double radial=distance>11 ? 1.0 : distance<7 ? -.55 : 0.0;
    const double side=(enemy.id.index%2 ? 1.0 : -1.0)*.45;
    const double speed=enemy.speed*enemy.movementMultiplier*1.35*(1+(enemy.bossPhase-1)*.15);
    enemy.position.x+=(dx/distance*radial-dz/distance*side)*speed*deltaSeconds;
    enemy.position.z+=(dz/distance*radial+dx/distance*side)*speed*deltaSeconds;
    enemy.state=EnemyState::MoveToCore;
    enemy.bossHoverOffset+=(AlpakingHover-enemy.bossHoverOffset)*std::min(1.0,deltaSeconds*2.2);
    enemy.position.y=AlpakingBodyCenter+enemy.bossHoverOffset;
    return true;
}
} // namespace ian
