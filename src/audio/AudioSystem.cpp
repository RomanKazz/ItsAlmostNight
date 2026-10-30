#include "audio/AudioSystem.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

constexpr std::array<std::string_view, 5> WoodHitPaths{
    "assets/audio/wood_hit_0.ogg",
    "assets/audio/wood_hit_1.ogg",
    "assets/audio/wood_hit_2.ogg",
    "assets/audio/wood_hit_3.ogg",
    "assets/audio/wood_hit_4.ogg",
};

constexpr std::array<std::string_view, 5> StoneHitPaths{
    "assets/audio/stone_hit_0.ogg",
    "assets/audio/stone_hit_1.ogg",
    "assets/audio/stone_hit_2.ogg",
    "assets/audio/stone_hit_3.ogg",
    "assets/audio/stone_hit_4.ogg",
};

constexpr std::array<std::string_view, 5> GrassFootstepPaths{
    "assets/audio/footstep_grass_0.ogg",
    "assets/audio/footstep_grass_1.ogg",
    "assets/audio/footstep_grass_2.ogg",
    "assets/audio/footstep_grass_3.ogg",
    "assets/audio/footstep_grass_4.ogg",
};

constexpr std::array<std::string_view, 4> HarvestSwishPaths{
    "assets/audio/tool_swish_0.wav",
    "assets/audio/tool_swish_1.wav",
    "assets/audio/tool_swish_2.wav",
    "assets/audio/tool_swish_3.wav",
};

// Day0 doubles as the menu theme. Tracks within each context play in sequence.
constexpr std::array<std::string_view, 8> MusicPaths{
    "assets/audio/temporary/muck/Day0.mp3",
    "assets/audio/temporary/muck/Day1.mp3",
    "assets/audio/temporary/muck/Day2.mp3",
    "assets/audio/temporary/muck/Night0.mp3",
    "assets/audio/temporary/muck/Night1.mp3",
    "assets/audio/temporary/muck/Big_Chunks_Theme.mp3",
    "assets/audio/temporary/muck/Bob_theme.mp3",
    "assets/audio/temporary/muck/Gronk_Stonks.mp3",
};

AudioCue defenseCue(DefenseSoundGroup group) {
    switch (group) {
    case DefenseSoundGroup::Artillery: return AudioCue::CannonShot;
    case DefenseSoundGroup::Rockets: return AudioCue::FireShot;
    case DefenseSoundGroup::Catapult: return AudioCue::CatapultShot;
    case DefenseSoundGroup::Lightning: return AudioCue::Lightning;
    case DefenseSoundGroup::Bow: return AudioCue::BowShot;
    case DefenseSoundGroup::Wind: return AudioCue::WindGust;
    case DefenseSoundGroup::Trap: return AudioCue::Trap;
    default: return AudioCue::Count;
    }
}

} // namespace

AudioSystem::~AudioSystem() {
    shutdown();
}

void AudioSystem::initialize() {
    if (initialized_) {
        return;
    }
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        ownsAudioDevice_ = true;
    }
    if (!IsAudioDeviceReady()) {
        ownsAudioDevice_ = false;
        return;
    }

    applySettings();
    for (std::size_t index = 0; index < woodHits_.size(); ++index) {
        load(woodHits_[index], WoodHitPaths[index]);
        load(stoneHits_[index], StoneHitPaths[index]);
        load(grassFootsteps_[index], GrassFootstepPaths[index]);
    }
    for (std::size_t index = 0; index < harvestSwishes_.size(); ++index) {
        load(harvestSwishes_[index], HarvestSwishPaths[index]);
    }
    load(woodBreak_, "assets/audio/wood_break.ogg");
    load(stoneBreak_, "assets/audio/stone_hit_4.ogg");
    load(critical_, "assets/audio/critical.ogg");
    load(rifleShot_, "assets/audio/rifle_shot.wav");
    if (rifleShot_.loaded) {
        loadDefenseGunTransient();
        const auto initializeVoices = [](auto& voices, Sound source) {
            for (Clip& voice : voices) {
                voice.sound = LoadSoundAlias(source);
                voice.loaded = IsSoundValid(voice.sound);
                if (!voice.loaded) {
                    UnloadSoundAlias(voice.sound);
                    voice.sound = {};
                }
            }
        };
        initializeVoices(playerRifleVoices_, rifleShot_.sound);
        initializeVoices(defenseRifleVoices_,
            defenseGunShot_.loaded ? defenseGunShot_.sound : rifleShot_.sound);
    }
    load(explosion_, "assets/audio/explosion.wav");
    if (explosion_.loaded) {
        for (Clip& alias : explosionAliases_) {
            alias.sound = LoadSoundAlias(explosion_.sound);
            alias.loaded = IsSoundValid(alias.sound);
            if (!alias.loaded) {
                UnloadSoundAlias(alias.sound);
                alias.sound = {};
            }
        }
    }
    load(buildPlace_, "assets/audio/build_place.ogg");
    load(structureHit_, "assets/audio/structure_hit.ogg");
    load(structureBreak_, "assets/audio/structure_break.ogg");
    load(repair_, "assets/audio/repair.ogg");
    load(upgrade_, "assets/audio/upgrade.wav");
    load(levelupAfter_, "assets/audio/temporary/levelup_after.ogg");
    load(gate_, "assets/audio/gate.ogg");
    load(crystals_, "assets/audio/crystals.ogg");
    load(xpPickup_, "assets/audio/xp_pickup.wav");
    load(playerHit_, "assets/audio/player_hit.ogg");
    load(enemyHit_, "assets/audio/enemy_hit.ogg");
    load(turretHits_.front(), "assets/audio/turret_hit.ogg");
    if (turretHits_.front().loaded) {
        for (std::size_t index = 1; index < turretHits_.size(); ++index) {
            Clip& alias = turretHits_[index];
            alias.sound = LoadSoundAlias(turretHits_.front().sound);
            alias.loaded = IsSoundValid(alias.sound);
            if (!alias.loaded) {
                UnloadSoundAlias(alias.sound);
                alias.sound = {};
            }
        }
    }
    load(uiError_, "assets/audio/ui_error.wav");
    load(uiConfirm_, "assets/audio/ui_confirm.wav");
    load(feedbackCapture_, "assets/audio/feedback_shutter.wav");
    load(waveWarning_, "assets/audio/wave_warning.wav");
    for (std::size_t index = 0; index < cues_.size(); ++index) {
        loadCue(cues_[index], AudioCueAssets[index].name, AudioCueAssets[index].fallback);
    }
    for (std::size_t index = 0; index < musicTracks_.size(); ++index) {
        load(musicTracks_[index], MusicPaths[index]);
        if (musicTracks_[index].loaded) musicTracks_[index].music.looping = false;
    }
    load(forestAmbience_, "assets/audio/temporary/muck/Ambience.mp3");
    load(flamethrowerLoop_, "assets/audio/flamethrower_loop.wav");
    load(laserLoop_, "assets/audio/laser_loop.wav");
    initialized_ = true;
}

void AudioSystem::shutdown() {
    if (!initialized_ && !ownsAudioDevice_) {
        return;
    }
    for (auto& clip : woodHits_) {
        unload(clip);
    }
    for (auto& clip : stoneHits_) {
        unload(clip);
    }
    for (auto& clip : grassFootsteps_) {
        unload(clip);
    }
    for (auto& clip : harvestSwishes_) {
        unload(clip);
    }
    unload(woodBreak_);
    unload(stoneBreak_);
    unload(critical_);
    const auto unloadVoices = [](auto& voices) {
        for (Clip& voice : voices) {
            if (voice.loaded) UnloadSoundAlias(voice.sound);
            voice = {};
        }
    };
    unloadVoices(playerRifleVoices_);
    unloadVoices(defenseRifleVoices_);
    unload(defenseGunShot_);
    unload(rifleShot_);
    for (Clip& alias : explosionAliases_) {
        if (alias.loaded) UnloadSoundAlias(alias.sound);
        alias = {};
    }
    unload(explosion_);
    explosionSoundBudget_.reset();
    unload(buildPlace_);
    unload(structureHit_);
    unload(structureBreak_);
    unload(repair_);
    unload(upgrade_);
    unload(levelupAfter_);
    levelupChoiceWasPending_ = false;
    unload(gate_);
    unload(crystals_);
    unload(xpPickup_);
    unload(playerHit_);
    playerHitSoundCooldown_ = 0.0;
    unload(enemyHit_);
    for (std::size_t index = 1; index < turretHits_.size(); ++index) {
        Clip& alias = turretHits_[index];
        if (alias.loaded) {
            UnloadSoundAlias(alias.sound);
            alias = {};
        }
    }
    unload(turretHits_.front());
    unload(uiError_);
    unload(uiConfirm_);
    unload(feedbackCapture_);
    unload(waveWarning_);
    for (auto& clip : cues_) unload(clip);
    cueCooldowns_.fill(0.0);
    defenseSoundBudget_.reset();
    pendingDefenseSounds_.fill({});
    nextDefenseSoundGroup_ = 0;
    uiErrorSoundCooldown_ = 0.0;
    upgradeChoiceWasVisible_ = false;
    playerWasGrounded_ = true;
    bossWasPresent_ = false;
    for (MusicTrack& track : musicTracks_) unload(track);
    unload(forestAmbience_);
    unload(levelupAmbience_);
    unload(flamethrowerLoop_);
    unload(laserLoop_);
    currentMusicTrack_.reset();
    currentMusicGroup_.reset();
    currentMusicTrackStarted_ = false;
    nextBossTheme_ = 0;
    initialized_ = false;
    previousPlayerPosition_.reset();
    pendingBuildingPlacedSounds_.clear();
    buildingPlacedSoundCooldown_ = 0.0;
    footstepDistance_ = 0.0;
    iceHitSoundCooldown_ = 0.0;
    xpPickupSoundCooldown_ = 0.0;
    lowHealthAmount_ = 0.0F;
    if (ownsAudioDevice_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    ownsAudioDevice_ = false;
}

void AudioSystem::update(const SimulationSnapshot& snapshot, float upgradeOverlayOpacity) {
    if (!initialized_) return;
    updateMusic(snapshot, upgradeOverlayOpacity);
    const bool activeRun =
        snapshot.state != RunState::MainMenu &&
        snapshot.state != RunState::StageClear &&
        snapshot.state != RunState::Victory &&
        snapshot.state != RunState::Defeat &&
        !snapshot.playerRespawning &&
        snapshot.playerHealth > 0.0 &&
        snapshot.playerMaxHealth > 0.0;
    // The choice is applied only after the panel's exit animation finishes.
    // A run reset/menu transition must not produce a level-up closing sound.
    if (levelupChoiceWasPending_ && !snapshot.runUpgradeChoicePending && activeRun) {
        play(levelupAfter_, 0.62F, 1.0F, 0.5F, false);
    }
    levelupChoiceWasPending_ = activeRun && snapshot.runUpgradeChoicePending;
    const double healthRatio = activeRun
        ? snapshot.playerHealth / snapshot.playerMaxHealth
        : 1.0;
    const float lowHealthTarget = static_cast<float>(
        std::clamp((0.35 - healthRatio) / 0.25, 0.0, 1.0));
    const float frameSeconds = std::max(GetFrameTime(), 0.0F);
    if (flamethrowerLoop_.loaded) {
        float targetGain = 0.0F;
        float nearestPan = 0.5F;
        if (activeRun && snapshot.state != RunState::Paused &&
            !snapshot.runUpgradeChoicePending && !snapshot.runShop.pending) {
            for (const auto& tower : snapshot.towers) {
                if (tower.type != BuildingType::Flamethrower || tower.flameRemaining <= 0.0) continue;
                const auto center = tower.flameOrigin;
                const double x = center.x - snapshot.playerPosition.x;
                const double z = center.z - snapshot.playerPosition.z;
                const float distance = static_cast<float>(std::hypot(x, z));
                const float gain = 0.24F * std::pow(std::clamp(1.0F - distance / 22.0F, 0.0F, 1.0F), 2.0F);
                if (gain > targetGain) {
                    targetGain = gain;
                    nearestPan = std::clamp(0.5F + static_cast<float>(
                        (x * std::cos(snapshot.playerYaw) + z * std::sin(snapshot.playerYaw)) /
                        std::max(1.0, static_cast<double>(distance))) * 0.28F, 0.0F, 1.0F);
                }
            }
        }
        // One shared loop, regardless of tower count. No per-heat-pulse voices.
        flamethrowerLoop_.volume += (targetGain - flamethrowerLoop_.volume) *
            (1.0F - std::exp(-12.0F * frameSeconds));
        if (targetGain > 0.001F || flamethrowerLoop_.volume > 0.001F) {
            if (!IsMusicStreamPlaying(flamethrowerLoop_.music)) PlayMusicStream(flamethrowerLoop_.music);
            UpdateMusicStream(flamethrowerLoop_.music);
            SetMusicVolume(flamethrowerLoop_.music, flamethrowerLoop_.volume * settings_.sfxVolume);
            SetMusicPan(flamethrowerLoop_.music, nearestPan);
        } else if (IsMusicStreamPlaying(flamethrowerLoop_.music)) {
            StopMusicStream(flamethrowerLoop_.music);
        }
    }
    if (laserLoop_.loaded) {
        float targetGain = 0.0F;
        float nearestPan = 0.5F;
        if (activeRun && snapshot.state != RunState::Paused &&
            !snapshot.runUpgradeChoicePending && !snapshot.runShop.pending) {
            for (const auto& tower : snapshot.towers) {
                if (tower.type != BuildingType::LaserTower || tower.laserRemaining <= 0.0) continue;
                const auto center = tower.laserOrigin;
                const double x = center.x - snapshot.playerPosition.x;
                const double z = center.z - snapshot.playerPosition.z;
                const float distance = static_cast<float>(std::hypot(x, z));
                const float gain = 0.16F * std::pow(std::clamp(1.0F - distance / 22.0F, 0.0F, 1.0F), 2.0F);
                if (gain > targetGain) {
                    targetGain = gain;
                    nearestPan = std::clamp(0.5F + static_cast<float>(
                        (x * std::cos(snapshot.playerYaw) + z * std::sin(snapshot.playerYaw)) /
                        std::max(1.0, static_cast<double>(distance))) * 0.28F, 0.0F, 1.0F);
                }
            }
        }
        // One shared loop, regardless of tower count. No per-heat-pulse voices.
        laserLoop_.volume += (targetGain - laserLoop_.volume) *
            (1.0F - std::exp(-12.0F * frameSeconds));
        if (targetGain > 0.001F || laserLoop_.volume > 0.001F) {
            if (!IsMusicStreamPlaying(laserLoop_.music)) PlayMusicStream(laserLoop_.music);
            UpdateMusicStream(laserLoop_.music);
            SetMusicVolume(laserLoop_.music, laserLoop_.volume * settings_.sfxVolume);
            SetMusicPan(laserLoop_.music, nearestPan);
        } else if (IsMusicStreamPlaying(laserLoop_.music)) {
            StopMusicStream(laserLoop_.music);
        }
    }
    explosionSoundBudget_.tick(frameSeconds);
    playerHitSoundCooldown_ = std::max(0.0, playerHitSoundCooldown_ - frameSeconds);
    uiErrorSoundCooldown_ = std::max(0.0, uiErrorSoundCooldown_ - frameSeconds);
    flushDefenseSounds(snapshot, frameSeconds);
    for (double& cooldown : cueCooldowns_) cooldown = std::max(0.0, cooldown - frameSeconds);
    if (snapshot.runUpgradeChoicePending && !upgradeChoiceWasVisible_) {
        play(cue(AudioCue::LevelUp), 0.62F, 1.0F, 0.5F, false);
    }
    upgradeChoiceWasVisible_ = snapshot.runUpgradeChoicePending;
    if (activeRun && playerWasGrounded_ && !snapshot.playerGrounded &&
        snapshot.playerVerticalVelocity > 0.1) {
        play(cue(AudioCue::Jump), 0.20F, variedPitch(0.025F));
    }
    playerWasGrounded_ = snapshot.playerGrounded;
    const bool bossPresent = std::ranges::any_of(snapshot.enemies, [](const EnemyInstance& enemy) {
        return enemy.type == EnemyType::Boss;
    });
    if (bossPresent && !bossWasPresent_) play(cue(AudioCue::BossSpawn), 0.45F, 1.0F, 0.5F, false);
    bossWasPresent_ = bossPresent;
    const float lowHealthBlend = 1.0F - std::exp(
        -(lowHealthTarget > lowHealthAmount_ ? 7.5F : 4.0F) *
        frameSeconds);
    lowHealthAmount_ +=
        (lowHealthTarget - lowHealthAmount_) * lowHealthBlend;
    iceHitSoundCooldown_ = std::max(
        0.0, iceHitSoundCooldown_ - frameSeconds);
    xpPickupSoundCooldown_ = std::max(
        0.0, xpPickupSoundCooldown_ - frameSeconds);
    buildingPlacedSoundCooldown_ = std::max(
        0.0, buildingPlacedSoundCooldown_ - frameSeconds);
    for (PendingBuildingPlacedSound& pending :
         pendingBuildingPlacedSounds_) {
        pending.delayRemaining -= frameSeconds;
    }
    bool playedBuildingPlacedSound = false;
    while (!pendingBuildingPlacedSounds_.empty() &&
           pendingBuildingPlacedSounds_.front()
                   .delayRemaining <= 0.0) {
        const PendingBuildingPlacedSound pending =
            pendingBuildingPlacedSounds_.front();
        pendingBuildingPlacedSounds_.pop_front();
        if (playedBuildingPlacedSound ||
            buildingPlacedSoundCooldown_ > 0.0) {
            // Never retain an overdue placement sound. Dropping it keeps the
            // rhythm attached to the visual pop instead of creating a tail.
            continue;
        }
        playAt(
            buildPlace_, pending.position, snapshot, 0.8F,
            variedPitch(0.05F));
        buildingPlacedSoundCooldown_ = 0.10;
        playedBuildingPlacedSound = true;
    }
    const bool movementAudible =
        snapshot.state != RunState::MainMenu &&
        snapshot.state != RunState::Paused &&
        snapshot.state != RunState::StageClear &&
        snapshot.state != RunState::Victory &&
        snapshot.state != RunState::Defeat &&
        snapshot.playerGrounded;
    if (!movementAudible || !previousPlayerPosition_) {
        previousPlayerPosition_ = snapshot.playerPosition;
        footstepDistance_ = 0.0;
        return;
    }

    const double deltaX =
        snapshot.playerPosition.x -
        previousPlayerPosition_->x;
    const double deltaZ =
        snapshot.playerPosition.z -
        previousPlayerPosition_->z;
    const double distance =
        std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    previousPlayerPosition_ = snapshot.playerPosition;
    if (distance > 2.0) {
        footstepDistance_ = 0.0;
        return;
    }
    footstepDistance_ += distance;
    const double movementSpeed = std::hypot(
        snapshot.playerHorizontalVelocity.x,
        snapshot.playerHorizontalVelocity.z);
    const double speedAmount = std::clamp(
        (movementSpeed - 0.4) / 5.6, 0.0, 1.0);
    const double stepDistance =
        1.35 + speedAmount * 0.32;
    if (footstepDistance_ < stepDistance) {
        return;
    }
    footstepDistance_ =
        std::fmod(footstepDistance_, stepDistance);
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const std::size_t index =
        static_cast<std::size_t>(sequence_) %
        grassFootsteps_.size();
    play(
        grassFootsteps_[index],
        static_cast<float>(0.18 + speedAmount * 0.14),
        variedPitch(0.045F) *
            static_cast<float>(0.96 + speedAmount * 0.08));
}

void AudioSystem::playEvent(
    const GameEvent& event,
    const SimulationSnapshot& snapshot) {
    if (!initialized_) {
        return;
    }
    if (event.type == GameEventType::TowerFired && event.buildingType == BuildingType::WindBlower) {
        queueDefenseSound(DefenseSoundGroup::Wind, event.position, snapshot, .18F, 28.0F);
        return;
    }
    switch (event.type) {
    case GameEventType::EnemyMagicFired:
        if(event.enemyType==EnemyType::Bee)playCue(AudioCue::BowShot,event.position,snapshot,.16F,32.0F,.10);
        break;
    case GameEventType::SawSplinterLaunched:
        playAt(
            woodBreak_, event.position, snapshot, 0.22F,
            1.48F * variedPitch(0.09F), 28.0F);
        break;
    case GameEventType::ResourceHit:
        if (event.resourceType) {
            playResourceHit(
                *event.resourceType, event.position, snapshot);
        }
        break;
    case GameEventType::ResourceCollected:
        if (event.resourceType) {
            playAt(
                (*event.resourceType == ResourceType::Wood || isDestructibleProp(*event.resourceType))
                    ? woodBreak_ : stoneBreak_,
                event.position, snapshot, 0.95F,
                variedPitch(0.07F));
        }
        break;
    case GameEventType::PickaxeHit:
        playAt(
            enemyHit_, event.position, snapshot, 0.8F,
            variedPitch(0.08F));
        if (event.critical) {
            playAt(
                critical_, event.position, snapshot, 0.48F,
                1.12F);
        }
        break;
    case GameEventType::WeaponFired:
        {
        const auto active=std::count_if(playerRifleVoices_.begin(),playerRifleVoices_.end(),
            [](const Clip& voice) {return voice.loaded && IsSoundPlaying(voice.sound);});
        for (const Clip& voice : playerRifleVoices_) {
            if (voice.loaded && !IsSoundPlaying(voice.sound)) {
                // Retain the normal shot gain; dense rapid fire leaves mix headroom.
                play(voice, 0.44F/(1.0F+2.0F*static_cast<float>(active)), variedPitch(0.025F));
                break;
            }
        }
        }
        break;
    case GameEventType::IceWandChargeStarted:
        play(cue(AudioCue::IceCharge), 0.24F, variedPitch(0.02F));
        break;
    case GameEventType::FireWandChargeStarted:
        play(cue(AudioCue::FireCharge), 0.22F, variedPitch(0.02F));
        break;
    case GameEventType::IceWandFired:
        if (cue(AudioCue::IceCharge).loaded) StopSound(cue(AudioCue::IceCharge).sound);
        play(cue(AudioCue::IceShot), 0.46F, variedPitch(0.025F));
        break;
    case GameEventType::FireWandFired:
        if (cue(AudioCue::FireCharge).loaded) StopSound(cue(AudioCue::FireCharge).sound);
        play(cue(AudioCue::FireShot), 0.46F, variedPitch(0.025F));
        break;
    case GameEventType::IceWandImpact:
        playExplosion(cue(AudioCue::IceImpact), event.position, snapshot, 0.32F, 42.0F);
        break;
    case GameEventType::FireWandImpact:
        playExplosion(cue(AudioCue::FireImpact), event.position, snapshot, 0.32F, 42.0F);
        break;
    case GameEventType::IceWandHit:
    case GameEventType::FireWandHit:
        // One short cue per burst is enough; the impact event already owns
        // the group hit sound, so a cluster cannot flood the audio mixer.
        if (iceHitSoundCooldown_ <= 0.0) {
            playAt(
                event.critical ? critical_ : enemyHit_, event.position,
                snapshot, event.critical ? 0.28F : 0.20F,
                (event.critical ? 1.42F : 1.58F) *
                    variedPitch(0.025F),
                30.0F);
            iceHitSoundCooldown_ = 0.045;
        }
        break;
    case GameEventType::ChainLightningHit:
        if (event.buildingType) {
            // A discharge owns one sound; its bounces do not add more layers.
            if (event.amount == 0)
                queueDefenseSound(DefenseSoundGroup::Lightning, event.position, snapshot, .24F, 32.0F);
        } else {
            playCue(AudioCue::Lightning, event.targetPosition.value_or(event.position),
                snapshot, event.amount == 0 ? 0.34F : 0.14F, 38.0F, 0.08);
        }
        break;
    case GameEventType::CannonFired:
        playDefenseShot(event.buildingType.value_or(BuildingType::Cannon), event.position, snapshot);
        break;
    case GameEventType::ProjectileHit:
        if (event.buildingType == BuildingType::Flamethrower || event.buildingType == BuildingType::MissileLauncher || event.buildingType == BuildingType::TeslaTower || event.buildingType == BuildingType::LaserTower) break;
        if (event.sourceId) {
            playTurretImpact(
                event.position, snapshot, event.secondaryImpact);
        } else {
            playAt(enemyHit_, event.position, snapshot, 0.48F,
                   variedPitch(0.06F));
        }
        break;
    case GameEventType::MissileExplosion:
    case GameEventType::ItemExplosion:
    case GameEventType::Explosion:
        playExplosion(explosion_, event.position, snapshot, 0.40F, 55.0F);
        break;
    case GameEventType::AnvilRepairShockwave:
        playExplosion(cue(AudioCue::GroundSlam), event.position, snapshot, 0.40F, 48.0F);
        break;
    case GameEventType::IronArmorBroken:
        play(cue(AudioCue::ShieldBreak), 0.48F);
        break;
    case GameEventType::EnemySplit:
        // Layer a soft body impact with a bright transient. This reads as a
        // juicy pop without introducing a new external audio dependency.
        playAt(enemyHit_, event.position, snapshot, 0.72F,
               variedPitch(0.04F) * 0.68F, 34.0F);
        playAt(critical_, event.position, snapshot, 0.22F,
               variedPitch(0.03F) * 1.28F, 30.0F);
        break;
    case GameEventType::EliteEnemySpawned:
        playAt(
            waveWarning_, event.position, snapshot,
            0.30F, 1.32F * variedPitch(0.025F), 42.0F);
        break;
    case GameEventType::EliteVolatilePrimed:
        playAt(
            critical_, event.position, snapshot,
            0.22F, 0.72F * variedPitch(0.02F), 34.0F);
        break;
    case GameEventType::BuildingPlaced:
        queueBuildingPlaced(event.position);
        break;
    case GameEventType::BuildingDamaged:
    case GameEventType::ModularBuildingDamaged:
    case GameEventType::CoreDamaged:
        // Enemy attacks already have visual damage feedback. Keep the
        // generic impact only for damage without an attacker (e.g. debug).
        if (!event.sourceId) {
            playAt(
                structureHit_, event.position, snapshot, 0.34F,
                variedPitch(0.09F));
        }
        break;
    case GameEventType::BuildingDestroyed:
    case GameEventType::ModularBuildingDestroyed:
        // A collapsing row shares one crunch instead of restarting the clip
        // for every piece in the same frame.
        if (structureBreak_.loaded && !IsSoundPlaying(structureBreak_.sound)) {
            playAt(structureBreak_, event.position, snapshot, .70F, variedPitch(.06F));
        }
        break;
    case GameEventType::BuildingSold:
        playCue(AudioCue::Coin, event.position, snapshot, 0.48F);
        break;
    case GameEventType::BuildingRepaired:
    case GameEventType::ModularBuildingRepaired:
        playAt(
            repair_, event.position, snapshot, 0.7F,
            variedPitch(0.045F));
        break;
    case GameEventType::BuildingUpgraded:
        playAt(
            upgrade_, event.position, snapshot, 0.75F, 1.0F,
            45.0F);
        break;
    case GameEventType::WeaponUpgraded:
        play(upgrade_, 0.75F, 1.0F);
        break;
    case GameEventType::CrystalProduced:
        break;
    case GameEventType::MetaSilverCollected:
    case GameEventType::CoinCollected:
        play(
            cue(AudioCue::Coin),
            std::min(
                0.78F,
                0.42F + static_cast<float>(event.amount) * 0.055F),
            variedPitch(0.025F) +
                std::min(
                    0.22F,
                    static_cast<float>(event.amount) * 0.018F));
        break;
    case GameEventType::ExperienceCollected:
        if (xpPickupSoundCooldown_ <= 0.0) {
            play(xpPickup_,
                 std::min(0.42F,
                     0.27F + static_cast<float>(event.intensity) *
                         0.025F),
                 variedPitch(0.045F), 0.5F, false);
            xpPickupSoundCooldown_ = 0.075;
        }
        break;
    case GameEventType::WaveRewardGranted:
    case GameEventType::EarlyWaveBonusGranted:
        play(cue(AudioCue::Purchase), 0.38F);
        break;
    case GameEventType::ResourceGranted:
        break;
    case GameEventType::PlayerDamaged:
        // One audible impact for a group of same-frame hits; never restart
        // an occupied voice or turn a crowd into a continuous stutter.
        if (playerHitSoundCooldown_ <= 0.0 && playerHit_.loaded &&
            !IsSoundPlaying(playerHit_.sound)) {
            play(playerHit_, 0.60F, variedPitch(0.05F), 0.5F, false);
            playerHitSoundCooldown_ = 0.14;
        }
        break;
    case GameEventType::BossRamImpact:
        break;
    case GameEventType::BossRoyalImpact:
    case GameEventType::BossGroundSlam:
        playExplosion(cue(AudioCue::GroundSlam), event.position, snapshot, 0.40F, 50.0F);
        break;
    case GameEventType::BossPhaseChanged:
    case GameEventType::BossWarCry:
        playCue(AudioCue::BossRoar, event.position, snapshot, 0.56F, 60.0F, 1.0);
        break;
    case GameEventType::PlayerLanded:
        play(cue(AudioCue::Landing), static_cast<float>(std::clamp(event.intensity / 25.0, 0.18, 0.42)),
             variedPitch(0.04F));
        footstepDistance_ = 0.0;
        break;
    case GameEventType::RopeFallSaved:
        play(cue(AudioCue::ShieldRecharge), 0.42F);
        break;
    case GameEventType::TrapActivated:
        queueDefenseSound(DefenseSoundGroup::Trap, event.position, snapshot, .22F, 28.0F);
        break;
    case GameEventType::GateToggled:
        playAt(
            gate_, event.position, snapshot, 0.68F,
            event.amount != 0 ? 1.05F : 0.92F);
        break;
    case GameEventType::AttackDirectionWarned:
        play(waveWarning_,
             event.critical ? 0.88F : 0.52F,
             event.critical ? 0.74F : 1.0F);
        break;
    case GameEventType::WaveStarted:
        play(waveWarning_, 0.48F);
        break;
    case GameEventType::WaveCompleted:
        play(uiConfirm_, 0.52F, 0.94F);
        break;
    case GameEventType::StageCleared:
        play(cue(AudioCue::Victory), 0.50F, 1.0F, 0.5F, false);
        break;
    case GameEventType::FinalNightStarted:
        play(waveWarning_, 0.92F, 0.72F);
        break;
    case GameEventType::InsightGranted:
        // The choice-open transition owns the level-up cue, including queued levels.
        break;
    case GameEventType::ChestOpened:
        play(cue(AudioCue::ChestOpen), 0.54F);
        break;
    case GameEventType::ChestRerolled:
        play(cue(AudioCue::ChestReroll), 0.44F);
        break;
    case GameEventType::StumpRootWave:
        playCue(AudioCue::RootEruption,event.position,snapshot,.58F,24.0F,.22);
        break;
    case GameEventType::ChestRevealed:
    case GameEventType::BombPurchased:
        playUiPurchase();
        break;
    case GameEventType::AllBuildingsRepaired:
        play(cue(AudioCue::Heal), 0.48F);
        break;
    case GameEventType::LootCollected:
        play(cue(AudioCue::LootTake), 0.58F);
        break;
    case GameEventType::BattlePotionActivated:
        play(cue(AudioCue::Potion), 0.45F);
        break;
    case GameEventType::AppleConsumed:
    case GameEventType::BreadWellFed:
        play(cue(AudioCue::Consume), 0.50F);
        play(cue(AudioCue::Heal), 0.24F);
        break;
    case GameEventType::BuildingRejected:
    case GameEventType::BuildingUpgradeRejected:
    case GameEventType::BuildingRepairRejected:
    case GameEventType::BuildingSellRejected:
    case GameEventType::WeaponUpgradeRejected:
    case GameEventType::GateToggleRejected:
    case GameEventType::ChestOpenRejected:
    case GameEventType::ChestRerollAlreadyUsed:
    case GameEventType::ChestRerollUnavailable:
    case GameEventType::EconomyPurchaseRejected:
    case GameEventType::CrystalStorageFull:
    case GameEventType::ResourceStorageFull:
        playUiError();
        break;
    case GameEventType::RunStarted:
    case GameEventType::RunRestarted:
        playerHitSoundCooldown_ = 0.0;
        if (playerHit_.loaded) StopSound(playerHit_.sound);
        for (const auto& clip : cues_) if (clip.loaded) StopSound(clip.sound);
        cueCooldowns_.fill(0.0);
        defenseSoundBudget_.reset();
        pendingDefenseSounds_.fill({});
        nextDefenseSoundGroup_ = 0;
        for (const auto& clip : defenseRifleVoices_) if (clip.loaded) StopSound(clip.sound);
        for (const auto& clip : turretHits_) if (clip.loaded) StopSound(clip.sound);
        play(cue(AudioCue::RunStart), 0.38F, 1.0F, 0.5F, false);
        break;
    case GameEventType::RunEnded:
        play(cue(snapshot.state == RunState::Defeat ? AudioCue::Defeat : AudioCue::Victory),
             0.48F, 1.0F, 0.5F, false);
        break;
    case GameEventType::PlayerDied:
        if (snapshot.state != RunState::Defeat) play(cue(AudioCue::Defeat), 0.36F, 1.0F, 0.5F, false);
        break;
    case GameEventType::PlayerRespawned:
        if (cue(AudioCue::Defeat).loaded) StopSound(cue(AudioCue::Defeat).sound);
        play(cue(AudioCue::ShieldRecharge), 0.32F, 1.0F, 0.5F, false);
        break;
    case GameEventType::EnemyKilled:
        playCue(AudioCue::EnemyDeath, event.position, snapshot, 0.20F, 30.0F, 0.12);
        break;
    case GameEventType::ObjectiveCompleted:
        playUiReward();
        break;
    case GameEventType::ChallengeCompleted:
        play(cue(AudioCue::ChallengeComplete), 0.45F, 1.0F, 0.5F, false);
        break;
    case GameEventType::WorldLandmarkActivated:
    case GameEventType::WorldLandmarkUpgraded:
        playCue(AudioCue::Purchase, event.position, snapshot, 0.40F, 24.0F, 0.3);
        break;
    case GameEventType::BuildingFortified:
        playCue(AudioCue::ShieldRecharge, event.position, snapshot, 0.32F, 24.0F, 0.12);
        break;
    case GameEventType::ConsumableUsed:
        play(cue(AudioCue::Potion), 0.34F);
        break;
    default:
        break;
    }
}

void AudioSystem::playTurretImpact(
    Vec3 position, const SimulationSnapshot& snapshot,
    bool secondaryImpact) {
    queueDefenseSound(DefenseSoundGroup::Impact, position, snapshot,
        secondaryImpact ? .10F : .16F, 26.0F);
}

void AudioSystem::queueBuildingPlaced(
    Vec3 position, double delaySeconds) {
    if (!initialized_) {
        return;
    }
    constexpr std::size_t MaximumQueuedSounds = 24U;
    const PendingBuildingPlacedSound pending{
        .position = position,
        .delayRemaining = std::max(0.0, delaySeconds),
    };
    if (pendingBuildingPlacedSounds_.size() >=
        MaximumQueuedSounds) {
        if (pendingBuildingPlacedSounds_.back()
                .delayRemaining <= pending.delayRemaining) {
            return;
        }
        pendingBuildingPlacedSounds_.pop_back();
    }
    const auto insertion = std::upper_bound(
        pendingBuildingPlacedSounds_.begin(),
        pendingBuildingPlacedSounds_.end(),
        pending.delayRemaining,
        [](double delay,
           const PendingBuildingPlacedSound& candidate) {
            return delay < candidate.delayRemaining;
        });
    pendingBuildingPlacedSounds_.insert(insertion, pending);
}

void AudioSystem::playUiSelect() {
    sequence_ = sequence_ * 1664525U + 1013904223U;
    play(cue((sequence_ & 1U) ? AudioCue::UiSelect : AudioCue::UiSelectAlternate),
         0.38F, 1.0F, 0.5F, false);
}

void AudioSystem::playFeedbackCapture() {
    play(feedbackCapture_, 0.55F, 1.0F, 0.5F, false);
}

void AudioSystem::playUiConfirm() {
    play(uiConfirm_, 0.5F, 1.0F, 0.5F, false);
}

void AudioSystem::playUiPurchase() {
    play(cue(AudioCue::Purchase), 0.40F, 1.0F, 0.5F, false);
}

void AudioSystem::playUiReward() {
    play(cue(AudioCue::Achievement), 0.52F, 1.0F, 0.5F, false);
}

void AudioSystem::loadDefenseGunTransient() {
    Wave source = LoadWave("assets/audio/rifle_shot.wav");
    if (!IsWaveValid(source)) { UnloadWave(source); return; }
    float* samples = LoadWaveSamples(source);
    if (samples) {
        // Keep the muzzle attack, fading the long musket tail out of the
        // turret layer. PCM is prepared once; gameplay allocates no voices.
        const auto frames = std::min(source.frameCount,
            static_cast<unsigned int>(source.sampleRate * 0.18));
        const float fadeFrames = static_cast<float>(source.sampleRate) * .045F;
        const float attackFrames = std::max(1.0F, static_cast<float>(source.sampleRate) * .002F);
        for (unsigned int frame = 0; frame < frames; ++frame) {
            const float tail = std::clamp(static_cast<float>(frames - frame - 1U) / fadeFrames, 0.0F, 1.0F);
            const float attack = std::min(1.0F, static_cast<float>(frame) / attackFrames);
            for (unsigned int channel = 0; channel < source.channels; ++channel)
                samples[static_cast<std::size_t>(frame) * source.channels + channel] *= attack * tail * tail;
        }
        const Wave transient{frames, source.sampleRate, 32, source.channels, samples};
        defenseGunShot_.sound = LoadSoundFromWave(transient);
        defenseGunShot_.loaded = IsSoundValid(defenseGunShot_.sound);
        if (!defenseGunShot_.loaded) {
            UnloadSound(defenseGunShot_.sound);
            defenseGunShot_.sound = {};
        }
        UnloadWaveSamples(samples);
    }
    UnloadWave(source);
}

void AudioSystem::queueDefenseSound(DefenseSoundGroup group, Vec3 position,
    const SimulationSnapshot& snapshot, float volume, float maximumDistance) {
    if (!initialized_ || settings_.muted || settings_.sfxVolume <= 0.0F) return;
    const double distance = std::hypot(position.x - snapshot.playerPosition.x,
                                       position.z - snapshot.playerPosition.z);
    if (!std::isfinite(distance) || distance >= maximumDistance) return;
    const float attenuation = 1.0F - std::clamp(static_cast<float>(
        (distance - 3.0) / std::max(0.001, static_cast<double>(maximumDistance) - 3.0)), 0.0F, 1.0F);
    const float audibility = volume * attenuation * attenuation;
    if (audibility < .002F) return;
    auto& pending = pendingDefenseSounds_[static_cast<std::size_t>(group)];
    if (!pending.pending || audibility >= pending.audibility)
        pending = {position, volume, maximumDistance, audibility, 0.0, true};
}

std::size_t AudioSystem::activeDefenseVoices(DefenseSoundGroup group) const {
    const auto count = [](const auto& voices) {
        return static_cast<std::size_t>(std::count_if(voices.begin(), voices.end(),
            [](const Clip& clip) { return clip.loaded && IsSoundPlaying(clip.sound); }));
    };
    if (group == DefenseSoundGroup::Gun) return count(defenseRifleVoices_);
    if (group == DefenseSoundGroup::Impact) return count(turretHits_);
    const auto type = defenseCue(group);
    if (type == AudioCue::Count) return 0;
    const Clip& clip = cue(type);
    return clip.loaded && IsSoundPlaying(clip.sound) ? 1U : 0U;
}

const AudioSystem::Clip* AudioSystem::defenseVoice(DefenseSoundGroup group) const {
    const auto freeVoice = [](const auto& voices) -> const Clip* {
        for (const auto& clip : voices)
            if (clip.loaded && !IsSoundPlaying(clip.sound)) return &clip;
        return nullptr;
    };
    if (group == DefenseSoundGroup::Gun) return freeVoice(defenseRifleVoices_);
    if (group == DefenseSoundGroup::Impact) return freeVoice(turretHits_);
    const auto type = defenseCue(group);
    if (type == AudioCue::Count) return nullptr;
    const Clip& clip = cue(type);
    return clip.loaded && !IsSoundPlaying(clip.sound) ? &clip : nullptr;
}

void AudioSystem::flushDefenseSounds(const SimulationSnapshot& snapshot, double seconds) {
    defenseSoundBudget_.tick(seconds);
    if (settings_.muted || settings_.sfxVolume <= 0 || snapshot.state == RunState::MainMenu ||
        snapshot.state == RunState::Paused || snapshot.state == RunState::Defeat ||
        snapshot.state == RunState::Victory || snapshot.runUpgradeChoicePending || snapshot.runShop.pending) {
        pendingDefenseSounds_.fill({});
        return;
    }
    std::array<std::size_t, DefenseSoundGroupCount> active{};
    std::size_t total = 0;
    for (std::size_t i = 0; i < active.size(); ++i) {
        active[i] = activeDefenseVoices(static_cast<DefenseSoundGroup>(i));
        total += active[i];
    }
    // Rotate families so rapid machine guns cannot starve slower weapons.
    const std::size_t first = nextDefenseSoundGroup_;
    for (std::size_t offset = 0; offset < DefenseSoundGroupCount; ++offset) {
        const auto index = (first + offset) % DefenseSoundGroupCount;
        auto& pending = pendingDefenseSounds_[index];
        if (!pending.pending) continue;
        if (pending.age > .10) { pending = {}; continue; }
        const double distance = std::hypot(pending.position.x - snapshot.playerPosition.x,
                                           pending.position.z - snapshot.playerPosition.z);
        if (distance >= pending.maximumDistance) { pending = {}; continue; }
        const auto group = static_cast<DefenseSoundGroup>(index);
        const Clip* voice = defenseVoice(group);
        if (!voice) continue;
        const auto gain = defenseSoundBudget_.reserve(group, active[index], total);
        if (!gain) continue;
        playAt(*voice, pending.position, snapshot, pending.volume * *gain,
            variedPitch(.025F), pending.maximumDistance);
        pending = {};
        ++active[index]; ++total;
        nextDefenseSoundGroup_ = (index + 1U) % DefenseSoundGroupCount;
    }
    if (std::isfinite(seconds) && seconds > 0)
        for (auto& pending : pendingDefenseSounds_) if (pending.pending) pending.age += seconds;
}

void AudioSystem::playDefenseShot(BuildingType type, Vec3 position,
                                 const SimulationSnapshot& snapshot) {
    switch (type) {
    case BuildingType::GunTurret:
        queueDefenseSound(DefenseSoundGroup::Gun, position, snapshot, .20F, 30.0F); break;
    case BuildingType::Turret:
        queueDefenseSound(DefenseSoundGroup::Bow, position, snapshot, .18F, 30.0F); break;
    case BuildingType::MissileLauncher:
        queueDefenseSound(DefenseSoundGroup::Rockets, position, snapshot, .22F, 42.0F); break;
    case BuildingType::Catapult:
        queueDefenseSound(DefenseSoundGroup::Catapult, position, snapshot, .28F, 42.0F); break;
    case BuildingType::Cannon:
    case BuildingType::Mortar:
        queueDefenseSound(DefenseSoundGroup::Artillery, position, snapshot, .30F, 44.0F); break;
    // Flame/laser have shared continuous loops; Tesla and wind own event cues.
    default: break;
    }
}

void AudioSystem::playHarvestSwing() {
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const std::size_t index =
        static_cast<std::size_t>(sequence_) % harvestSwishes_.size();
    constexpr float SlowedPlaybackPitch = 0.70F;
    play(
        harvestSwishes_[index], 0.20F,
        SlowedPlaybackPitch * variedPitch(0.07F),
        0.5F, false);
}

void AudioSystem::playUiError() {
    if (uiErrorSoundCooldown_ > 0.0) return;
    play(uiError_, 0.45F, 1.0F, 0.5F, false);
    uiErrorSoundCooldown_ = 0.12;
}

AudioSettings& AudioSystem::settings() {
    return settings_;
}

const AudioSettings& AudioSystem::settings() const {
    return settings_;
}

void AudioSystem::applySettings() {
    if (!IsAudioDeviceReady()) {
        return;
    }
    SetMasterVolume(
        settings_.muted
            ? 0.0F
            : std::clamp(
                  settings_.masterVolume, 0.0F, 1.0F));
    for (MusicTrack& track : musicTracks_) {
        if (settings_.musicVolume <= 0) track.volume = 0;
        if (track.loaded) SetMusicVolume(track.music, track.volume);
    }
    if (forestAmbience_.loaded) {
        if (settings_.sfxVolume <= 0) forestAmbience_.volume = 0;
        SetMusicVolume(forestAmbience_.music, forestAmbience_.volume);
    }
    if (flamethrowerLoop_.loaded) {
        SetMusicVolume(flamethrowerLoop_.music, flamethrowerLoop_.volume * settings_.sfxVolume);
    }
    if (levelupAmbience_.loaded) {
        if (settings_.sfxVolume <= 0) levelupAmbience_.volume = 0;
        SetMusicVolume(levelupAmbience_.music, levelupAmbience_.volume);
    }
}

void AudioSystem::load(Clip& clip, std::string_view path) {
    std::string pathString(path);
    const auto slash = pathString.find_last_of('/');
    const auto dot = pathString.find_last_of('.');
    const auto start = slash == std::string::npos ? 0U : slash + 1U;
    const std::string temporary = "assets/audio/temporary/" + pathString.substr(start, dot - start) + ".ogg";
    if (FileExists(temporary.c_str())) pathString = temporary;
    if (!FileExists(pathString.c_str())) {
        return;
    }
    clip.sound = LoadSound(pathString.c_str());
    clip.loaded = IsSoundValid(clip.sound);
    if (!clip.loaded) {
        // A failed format conversion can still leave an allocated raylib
        // AudioBuffer even though IsSoundValid() rejects the Sound.
        UnloadSound(clip.sound);
        clip.sound = {};
    }
}

void AudioSystem::unload(Clip& clip) {
    if (!clip.loaded) {
        return;
    }
    UnloadSound(clip.sound);
    clip = {};
}

void AudioSystem::load(
    MusicTrack& track, std::string_view path) {
    const std::string pathString(path);
    if (!FileExists(pathString.c_str())) {
        return;
    }
    track.music = LoadMusicStream(pathString.c_str());
    track.loaded = IsMusicValid(track.music);
    if (!track.loaded) {
        UnloadMusicStream(track.music);
        track.music = {};
        return;
    }
    track.music.looping = true;
    SetMusicVolume(track.music, 0.0F);
}

void AudioSystem::unload(MusicTrack& track) {
    if (!track.loaded) {
        return;
    }
    StopMusicStream(track.music);
    UnloadMusicStream(track.music);
    track = {};
}

void AudioSystem::updateMusic(const SimulationSnapshot& snapshot, float upgradeOverlayOpacity) {
    std::size_t group = 1;
    if (snapshot.state == RunState::MainMenu || snapshot.state == RunState::Defeat ||
        snapshot.state == RunState::Victory) group = 0;
    else if (snapshot.state == RunState::Wave || snapshot.state == RunState::Sunset) {
        group = std::ranges::any_of(snapshot.enemies, [](const EnemyInstance& enemy) {
            return enemy.type == EnemyType::Boss;
        }) ? 3U : 2U;
    } else if (snapshot.state == RunState::Paused && currentMusicGroup_) group = *currentMusicGroup_;

    constexpr std::array<std::size_t, 4> groupFirst{0, 0, 3, 5};
    constexpr std::array<std::size_t, 4> groupCount{1, 3, 2, 3};
    const std::size_t first = groupFirst[group];
    const std::size_t count = groupCount[group];
    if (!currentMusicGroup_ || *currentMusicGroup_ != group) {
        currentMusicGroup_ = group;
        currentMusicTrack_ = group == 1 ? 1U : group == 3 ? first + nextBossTheme_++ % count : first;
        currentMusicTrackStarted_ = false;
    } else if (currentMusicTrack_ && currentMusicTrackStarted_ &&
               !IsMusicStreamPlaying(musicTracks_[*currentMusicTrack_].music)) {
        currentMusicTrack_ = first + (*currentMusicTrack_ - first + 1U) % count;
        currentMusicTrackStarted_ = false;
    }
    if (currentMusicTrack_ && !musicTracks_[*currentMusicTrack_].loaded) {
        currentMusicTrack_.reset();
        for (std::size_t index = first; index < first + count; ++index) {
            if (musicTracks_[index].loaded) { currentMusicTrack_ = index; break; }
        }
    }
    const float frame = std::max(GetFrameTime(), 0.0F);
    const float blend = 1.0F - std::exp(-3.5F * frame);
    const float duck = snapshot.runUpgradeChoicePending ? 0.32F : snapshot.state == RunState::Paused ? 0.50F : 1.0F;
    for (std::size_t index = 0; index < musicTracks_.size(); ++index) {
        auto& track = musicTracks_[index];
        if (!track.loaded) continue;
        const float target = currentMusicTrack_ && index == *currentMusicTrack_
            ? std::clamp(settings_.musicVolume, 0.0F, 1.0F) * 0.38F * duck : 0.0F;
        if (target > 0 && !IsMusicStreamPlaying(track.music)) {
            PlayMusicStream(track.music);
            if (currentMusicTrack_ && index == *currentMusicTrack_) currentMusicTrackStarted_ = true;
        }
        track.volume += (target - track.volume) * blend;
        SetMusicVolume(track.music, track.volume);
        if (IsMusicStreamPlaying(track.music)) UpdateMusicStream(track.music);
        if (target == 0 && track.volume < 0.001F) {
            if (IsMusicStreamPlaying(track.music)) StopMusicStream(track.music);
            if (currentMusicTrack_ && index == *currentMusicTrack_) currentMusicTrackStarted_ = false;
            track.volume = 0;
        }
    }
    if (forestAmbience_.loaded) {
        const float target = std::clamp(settings_.sfxVolume, 0.0F, 1.0F) *
            (snapshot.state == RunState::MainMenu ? 0.11F : 0.20F) * duck;
        forestAmbience_.volume += (target - forestAmbience_.volume) * blend;
        SetMusicVolume(forestAmbience_.music, forestAmbience_.volume);
        if (!IsMusicStreamPlaying(forestAmbience_.music)) PlayMusicStream(forestAmbience_.music);
        UpdateMusicStream(forestAmbience_.music);
    }
    if (levelupAmbience_.loaded) {
        // Follow the whole panel's opacity so this loop exists only while the
        // level-up window is visible, including queued choices and rerolls.
        levelupAmbience_.volume = snapshot.runUpgradeChoicePending
            ? std::clamp(settings_.sfxVolume, 0.0F, 1.0F) * 0.38F *
                std::clamp(upgradeOverlayOpacity, 0.0F, 1.0F) : 0.0F;
        SetMusicVolume(levelupAmbience_.music, levelupAmbience_.volume);
        if (levelupAmbience_.volume > 0) {
            if (!IsMusicStreamPlaying(levelupAmbience_.music)) PlayMusicStream(levelupAmbience_.music);
            UpdateMusicStream(levelupAmbience_.music);
        } else if (IsMusicStreamPlaying(levelupAmbience_.music)) {
            StopMusicStream(levelupAmbience_.music);
        }
    }
}

void AudioSystem::loadCue(Clip& clip, std::string_view name, std::string_view fallback) {
    const std::string temporary = "assets/audio/temporary/" + std::string(name) + ".ogg";
    load(clip, FileExists(temporary.c_str()) ? temporary : fallback);
}

const AudioSystem::Clip& AudioSystem::cue(AudioCue type) const {
    return cues_[static_cast<std::size_t>(type)];
}

void AudioSystem::playCue(AudioCue type, Vec3 position, const SimulationSnapshot& snapshot,
                          float volume, float maximumDistance, double cooldown) {
    auto& remaining = cueCooldowns_[static_cast<std::size_t>(type)];
    if (remaining > 0.0) return;
    playAt(cue(type), position, snapshot, volume, variedPitch(0.035F), maximumDistance);
    remaining = cooldown;
}

void AudioSystem::playExplosion(
    const Clip& clip, Vec3 position,
    const SimulationSnapshot& snapshot,
    float volume, float maximumDistance) {
    if (!initialized_ || !clip.loaded || settings_.muted ||
        settings_.sfxVolume <= 0.0F ||
        std::hypot(position.x - snapshot.playerPosition.x,
                   position.z - snapshot.playerPosition.z) >= maximumDistance)
        return;

    const auto playing = [](const Clip& voice) {
        return voice.loaded && IsSoundPlaying(voice.sound);
    };
    std::size_t active = playing(explosion_) ? 1U : 0U;
    for (const Clip& alias : explosionAliases_) active += playing(alias) ? 1U : 0U;
    for (const AudioCue type : {AudioCue::IceImpact, AudioCue::FireImpact, AudioCue::GroundSlam})
        active += playing(cue(type)) ? 1U : 0U;

    const Clip* voice = &clip;
    if (playing(*voice)) {
        if (&clip != &explosion_) return;
        voice = nullptr;
        for (const Clip& alias : explosionAliases_) {
            if (alias.loaded && !playing(alias)) {
                voice = &alias;
                break;
            }
        }
        if (!voice) return;
    }
    const auto gain = explosionSoundBudget_.reserve(active);
    if (!gain) return;
    playAt(*voice, position, snapshot,
           *gain * std::clamp(volume / ExplosionSoundBudget::MaximumVolume, 0.0F, 1.0F),
           variedPitch(0.035F), maximumDistance);
}

void AudioSystem::play(
    const Clip& clip, float volume, float pitch, float pan,
    bool affectedByLowHealth) {
    if (!initialized_ || !clip.loaded) {
        return;
    }
    const float healthAmount =
        affectedByLowHealth ? lowHealthAmount_ : 0.0F;
    const float healthVolume =
        1.0F - healthAmount*0.24F;
    const float healthPitch =
        1.0F - healthAmount*0.20F;
    SetSoundVolume(
        clip.sound,
        std::clamp(
            volume * settings_.sfxVolume * healthVolume,
            0.0F, 1.0F));
    SetSoundPitch(
        clip.sound,
        std::clamp(pitch * healthPitch, 0.5F, 2.0F));
    SetSoundPan(clip.sound, std::clamp(pan, 0.0F, 1.0F));
    PlaySound(clip.sound);
}

void AudioSystem::playAt(
    const Clip& clip, Vec3 position,
    const SimulationSnapshot& snapshot, float volume,
    float pitch, float maximumDistance) {
    const double offsetX =
        position.x - snapshot.playerPosition.x;
    const double offsetZ =
        position.z - snapshot.playerPosition.z;
    const double distance =
        std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    const float attenuation = 1.0F - std::clamp(
        static_cast<float>((distance - 3.0) /
                           std::max(
                               static_cast<double>(
                                   maximumDistance) -
                                   3.0,
                               0.001)),
        0.0F, 1.0F);
    if (attenuation <= 0.0F) {
        return;
    }
    float pan = 0.5F;
    if (distance > 0.001) {
        const double rightX = std::cos(snapshot.playerYaw);
        const double rightZ = std::sin(snapshot.playerYaw);
        const double rightAmount =
            (offsetX * rightX + offsetZ * rightZ) / distance;
        pan += static_cast<float>(rightAmount) * 0.38F;
    }
    play(
        clip, volume * attenuation * attenuation, pitch, pan);
}

void AudioSystem::playResourceHit(
    ResourceType type, Vec3 position,
    const SimulationSnapshot& snapshot) {
    auto& clips = type == ResourceType::Wood ? woodHits_ : stoneHits_;
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const std::size_t index =
        static_cast<std::size_t>(sequence_) % clips.size();
    playAt(
        clips[index], position, snapshot, 0.85F,
        variedPitch(0.075F));
}

float AudioSystem::variedPitch(float spread) {
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const float unit = static_cast<float>(
        (sequence_ >> 8U) & 0x00ffffffU) /
        static_cast<float>(0x00ffffffU);
    return 1.0F + (unit * 2.0F - 1.0F) * spread;
}

} // namespace ian
