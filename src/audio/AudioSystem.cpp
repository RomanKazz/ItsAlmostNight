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
    load(woodBreak_, "assets/audio/wood_break.ogg");
    load(stoneBreak_, "assets/audio/stone_hit_4.ogg");
    load(critical_, "assets/audio/critical.ogg");
    load(rifleShot_, "assets/audio/rifle_shot.wav");
    load(explosion_, "assets/audio/explosion.wav");
    load(buildPlace_, "assets/audio/build_place.ogg");
    load(structureHit_, "assets/audio/structure_hit.ogg");
    load(structureBreak_, "assets/audio/structure_break.ogg");
    load(repair_, "assets/audio/repair.ogg");
    load(upgrade_, "assets/audio/upgrade.wav");
    load(gate_, "assets/audio/gate.ogg");
    load(gold_, "assets/audio/gold.ogg");
    load(playerHit_, "assets/audio/player_hit.ogg");
    load(enemyHit_, "assets/audio/enemy_hit.ogg");
    load(turretHit_, "assets/audio/turret_hit.ogg");
    load(uiError_, "assets/audio/ui_error.wav");
    load(uiConfirm_, "assets/audio/ui_confirm.wav");
    load(waveWarning_, "assets/audio/wave_warning.wav");
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
    unload(woodBreak_);
    unload(stoneBreak_);
    unload(critical_);
    unload(rifleShot_);
    unload(explosion_);
    unload(buildPlace_);
    unload(structureHit_);
    unload(structureBreak_);
    unload(repair_);
    unload(upgrade_);
    unload(gate_);
    unload(gold_);
    unload(playerHit_);
    unload(enemyHit_);
    unload(turretHit_);
    unload(uiError_);
    unload(uiConfirm_);
    unload(waveWarning_);
    initialized_ = false;
    previousPlayerPosition_.reset();
    footstepDistance_ = 0.0;
    iceHitSoundCooldown_ = 0.0;
    insightSoundCooldown_ = 0.0;
    if (ownsAudioDevice_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    ownsAudioDevice_ = false;
}

void AudioSystem::update(const SimulationSnapshot& snapshot) {
    iceHitSoundCooldown_ = std::max(
        0.0, iceHitSoundCooldown_ - GetFrameTime());
    insightSoundCooldown_ = std::max(
        0.0, insightSoundCooldown_ - GetFrameTime());
    const bool movementAudible =
        snapshot.state != RunState::MainMenu &&
        snapshot.state != RunState::Paused &&
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
    switch (event.type) {
    case GameEventType::ResourceHit:
        if (event.resourceType) {
            playResourceHit(
                *event.resourceType, event.position, snapshot,
                event.critical);
        }
        break;
    case GameEventType::ResourceCollected:
        if (event.resourceType) {
            playAt(
                *event.resourceType == ResourceType::Wood
                    ? woodBreak_
                    : stoneBreak_,
                event.position, snapshot, 0.95F,
                variedPitch(0.07F));
            if (event.critical) {
                playAt(
                    critical_, event.position, snapshot, 0.45F,
                    1.12F);
            }
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
        play(rifleShot_, 0.72F, variedPitch(0.025F));
        break;
    case GameEventType::IceWandChargeStarted:
        // Reuse the short enemy-hit texture as a quiet crystalline charge
        // cue until a dedicated wand sample is added to the audio pack.
        play(enemyHit_, 0.18F, 1.62F * variedPitch(0.018F));
        break;
    case GameEventType::IceWandFired:
        play(rifleShot_, 0.34F, 0.66F * variedPitch(0.015F));
        break;
    case GameEventType::IceWandImpact:
        playAt(
            explosion_, event.position, snapshot, 0.42F,
            1.38F * variedPitch(0.025F), 42.0F);
        break;
    case GameEventType::IceWandHit:
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
    case GameEventType::CannonFired:
        playAt(
            rifleShot_, event.position, snapshot, 0.48F,
            variedPitch(0.03F) * 0.72F, 48.0F);
        break;
    case GameEventType::ProjectileHit:
        playAt(
            event.sourceId ? turretHit_ : enemyHit_,
            event.position, snapshot,
            event.sourceId ? 0.34F : 0.48F,
            variedPitch(0.06F));
        break;
    case GameEventType::Explosion:
        playAt(
            explosion_, event.position, snapshot, 0.92F,
            variedPitch(0.035F), 55.0F);
        break;
    case GameEventType::EnemySplit:
        // Layer a soft body impact with a bright transient. This reads as a
        // juicy pop without introducing a new external audio dependency.
        playAt(enemyHit_, event.position, snapshot, 0.72F,
               variedPitch(0.04F) * 0.68F, 34.0F);
        playAt(critical_, event.position, snapshot, 0.22F,
               variedPitch(0.03F) * 1.28F, 30.0F);
        break;
    case GameEventType::BuildingPlaced:
        playAt(
            buildPlace_, event.position, snapshot, 0.8F,
            variedPitch(0.05F));
        break;
    case GameEventType::BuildingDamaged:
    case GameEventType::ModularBuildingDamaged:
    case GameEventType::CoreDamaged:
        playAt(
            structureHit_, event.position, snapshot, 0.34F,
            variedPitch(0.09F));
        break;
    case GameEventType::BuildingDestroyed:
    case GameEventType::ModularBuildingDestroyed:
    case GameEventType::BuildingSold:
        playAt(
            structureBreak_, event.position, snapshot, 0.82F,
            variedPitch(0.06F));
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
    case GameEventType::GoldProduced:
        playAt(
            gold_, event.position, snapshot, 0.34F,
            variedPitch(0.04F), 16.0F);
        break;
    case GameEventType::CoinCollected:
        play(
            gold_,
            std::min(
                0.78F,
                0.42F + static_cast<float>(event.amount) * 0.055F),
            variedPitch(0.025F) +
                std::min(
                    0.22F,
                    static_cast<float>(event.amount) * 0.018F));
        break;
    case GameEventType::WaveRewardGranted:
        play(gold_, 0.58F, variedPitch(0.04F));
        break;
    case GameEventType::ResourceGranted:
        if (event.entityId && event.buildingType &&
            (*event.buildingType == BuildingType::LumberMill ||
             *event.buildingType == BuildingType::Quarry)) {
            playAt(
                repair_, event.position, snapshot, 0.24F,
                (event.resourceType &&
                         *event.resourceType ==
                             ResourceType::Stone
                     ? 0.9F
                     : 1.05F) *
                    variedPitch(0.035F),
                16.0F);
        } else {
            play(
                repair_, 0.2F,
                (event.resourceType &&
                         *event.resourceType ==
                             ResourceType::Stone
                     ? 0.9F
                     : 1.05F) *
                    variedPitch(0.035F));
        }
        break;
    case GameEventType::PlayerDamaged:
    case GameEventType::BossRamImpact:
        play(playerHit_, 0.72F, variedPitch(0.06F));
        break;
    case GameEventType::PlayerLanded: {
        sequence_ = sequence_ * 1664525U + 1013904223U;
        const std::size_t index =
            static_cast<std::size_t>(sequence_) %
            grassFootsteps_.size();
        const float strength = static_cast<float>(
            std::clamp((event.intensity - 1.0) / 7.0,
                       0.25, 1.0));
        play(grassFootsteps_[index],
             0.2F + strength * 0.2F,
             variedPitch(0.04F) * (0.94F - strength * 0.08F));
        footstepDistance_ = 0.0;
        break;
    }
    case GameEventType::PlayerDashed: {
        sequence_ = sequence_ * 1664525U + 1013904223U;
        const std::size_t index =
            static_cast<std::size_t>(sequence_) %
            grassFootsteps_.size();
        play(grassFootsteps_[index], 0.32F, variedPitch(0.05F) * 1.28F);
        // A quiet low transient gives the dash body without introducing a
        // dedicated asset dependency.
        play(rifleShot_, 0.09F, 0.52F);
        footstepDistance_ = 0.0;
        break;
    }
    case GameEventType::TrapActivated:
        playAt(
            turretHit_, event.position, snapshot, 0.42F,
            0.72F * variedPitch(0.04F));
        break;
    case GameEventType::GateToggled:
        playAt(
            gate_, event.position, snapshot, 0.68F,
            event.amount != 0 ? 1.05F : 0.92F);
        break;
    case GameEventType::AttackDirectionWarned:
        play(waveWarning_, 0.52F, 1.0F);
        break;
    case GameEventType::WaveStarted:
        play(waveWarning_, 0.68F, 0.84F);
        break;
    case GameEventType::WaveCompleted:
        play(uiConfirm_, 0.52F, 0.94F);
        break;
    case GameEventType::InsightGranted:
        if (event.treePointsGranted > 0) {
            play(upgrade_, 0.76F, 1.12F);
            insightSoundCooldown_ = 0.10;
        } else if (insightSoundCooldown_ <= 0.0) {
            play(uiConfirm_, event.insightAmount >= 10.0 ? 0.24F : 0.09F,
                 event.insightAmount >= 10.0 ? 1.18F : 1.34F);
            insightSoundCooldown_ = 0.08;
        }
        break;
    case GameEventType::ChestOpened:
        play(gate_, 0.62F, 0.78F);
        break;
    case GameEventType::LootCollected:
        play(upgrade_, 0.78F, 1.08F);
        break;
    case GameEventType::BuildingRejected:
    case GameEventType::BuildingUpgradeRejected:
    case GameEventType::BuildingRepairRejected:
    case GameEventType::BuildingSellRejected:
    case GameEventType::WeaponUpgradeRejected:
    case GameEventType::GateToggleRejected:
    case GameEventType::ChestOpenRejected:
        play(uiError_, 0.55F, 1.0F);
        break;
    default:
        break;
    }
}

void AudioSystem::playUiConfirm() {
    play(uiConfirm_, 0.5F, 1.0F);
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
}

void AudioSystem::load(Clip& clip, std::string_view path) {
    const std::string pathString(path);
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

void AudioSystem::play(
    const Clip& clip, float volume, float pitch, float pan) {
    if (!initialized_ || !clip.loaded) {
        return;
    }
    SetSoundVolume(
        clip.sound,
        std::clamp(
            volume * settings_.sfxVolume, 0.0F, 1.0F));
    SetSoundPitch(clip.sound, std::clamp(pitch, 0.5F, 2.0F));
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
    const SimulationSnapshot& snapshot, bool critical) {
    auto& clips =
        type == ResourceType::Wood ? woodHits_ : stoneHits_;
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const std::size_t index =
        static_cast<std::size_t>(sequence_) % clips.size();
    playAt(
        clips[index], position, snapshot, 0.85F,
        variedPitch(0.075F));
    if (critical) {
        playAt(
            critical_, position, snapshot, 0.45F, 1.12F);
    }
}

float AudioSystem::variedPitch(float spread) {
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const float unit = static_cast<float>(
        (sequence_ >> 8U) & 0x00ffffffU) /
        static_cast<float>(0x00ffffffU);
    return 1.0F + (unit * 2.0F - 1.0F) * spread;
}

} // namespace ian
