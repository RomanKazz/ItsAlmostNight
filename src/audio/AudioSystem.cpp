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

constexpr std::array<std::string_view, 3> MusicPaths{
    "assets/audio/music_day_0.mp3",
    "assets/audio/music_day_1.mp3",
    "assets/audio/music_day_2.mp3",
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
    load(crystals_, "assets/audio/crystals.ogg");
    load(playerHit_, "assets/audio/player_hit.ogg");
    load(enemyHit_, "assets/audio/enemy_hit.ogg");
    load(turretHit_, "assets/audio/turret_hit.ogg");
    load(uiError_, "assets/audio/ui_error.wav");
    load(uiConfirm_, "assets/audio/ui_confirm.wav");
    load(waveWarning_, "assets/audio/wave_warning.wav");
    for (std::size_t index = 0; index < musicTracks_.size(); ++index) {
        load(musicTracks_[index], MusicPaths[index]);
    }
    initialized_ = true;
    playNextMusicTrack();
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
    unload(crystals_);
    unload(playerHit_);
    unload(enemyHit_);
    unload(turretHit_);
    unload(uiError_);
    unload(uiConfirm_);
    unload(waveWarning_);
    for (MusicTrack& track : musicTracks_) {
        unload(track);
    }
    currentMusicTrack_.reset();
    initialized_ = false;
    previousPlayerPosition_.reset();
    footstepDistance_ = 0.0;
    iceHitSoundCooldown_ = 0.0;
    lowHealthAmount_ = 0.0F;
    if (ownsAudioDevice_ && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    ownsAudioDevice_ = false;
}

void AudioSystem::update(const SimulationSnapshot& snapshot) {
    updateMusic();
    const bool activeRun =
        snapshot.state != RunState::MainMenu &&
        snapshot.state != RunState::Defeat &&
        !snapshot.playerRespawning &&
        snapshot.playerHealth > 0.0 &&
        snapshot.playerMaxHealth > 0.0;
    const double healthRatio = activeRun
        ? snapshot.playerHealth / snapshot.playerMaxHealth
        : 1.0;
    const float lowHealthTarget = static_cast<float>(
        std::clamp((0.35 - healthRatio) / 0.25, 0.0, 1.0));
    const float frameSeconds = std::max(GetFrameTime(), 0.0F);
    const float lowHealthBlend = 1.0F - std::exp(
        -(lowHealthTarget > lowHealthAmount_ ? 7.5F : 4.0F) *
        frameSeconds);
    lowHealthAmount_ +=
        (lowHealthTarget - lowHealthAmount_) * lowHealthBlend;
    iceHitSoundCooldown_ = std::max(
        0.0, iceHitSoundCooldown_ - frameSeconds);
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
                *event.resourceType == ResourceType::Wood
                    ? woodBreak_
                    : stoneBreak_,
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
        play(rifleShot_, 0.72F, variedPitch(0.025F));
        break;
    case GameEventType::IceWandChargeStarted:
    case GameEventType::FireWandChargeStarted:
        // Reuse the short enemy-hit texture as a quiet crystalline charge
        // cue until a dedicated wand sample is added to the audio pack.
        play(enemyHit_, 0.18F, 1.62F * variedPitch(0.018F));
        break;
    case GameEventType::IceWandFired:
    case GameEventType::FireWandFired:
        play(rifleShot_, 0.34F, 0.66F * variedPitch(0.015F));
        break;
    case GameEventType::IceWandImpact:
    case GameEventType::FireWandImpact:
        playAt(
            explosion_, event.position, snapshot, 0.42F,
            1.38F * variedPitch(0.025F), 42.0F);
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
    case GameEventType::ChainLightningHit: {
        const Vec3 impact =
            event.targetPosition.value_or(event.position);
        if (event.amount == 0) {
            // Layer a sharp electrical crack over a short low transient.
            // Following jumps are quieter so a long chain stays readable.
            playAt(
                rifleShot_, impact, snapshot, 0.24F,
                1.72F * variedPitch(0.022F), 38.0F);
            playAt(
                critical_, impact, snapshot, 0.18F,
                1.34F * variedPitch(0.018F), 36.0F);
        } else if (iceHitSoundCooldown_ <= 0.0) {
            playAt(
                enemyHit_, impact, snapshot, 0.13F,
                (1.76F + static_cast<float>(event.amount) * 0.035F) *
                    variedPitch(0.02F),
                30.0F);
            iceHitSoundCooldown_ = 0.035;
        }
        break;
    }
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
    case GameEventType::AnvilRepairShockwave:
        playAt(
            explosion_, event.position, snapshot, 0.46F,
            0.72F * variedPitch(0.025F), 48.0F);
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
    case GameEventType::CrystalProduced:
        playAt(
            crystals_, event.position, snapshot, 0.34F,
            variedPitch(0.04F), 16.0F);
        break;
    case GameEventType::CoinCollected:
        play(
            crystals_,
            std::min(
                0.78F,
                0.42F + static_cast<float>(event.amount) * 0.055F),
            variedPitch(0.025F) +
                std::min(
                    0.22F,
                    static_cast<float>(event.amount) * 0.018F));
        break;
    case GameEventType::WaveRewardGranted:
    case GameEventType::EarlyWaveBonusGranted:
        play(crystals_, 0.58F, variedPitch(0.04F));
        break;
    case GameEventType::ResourceGranted:
        break;
    case GameEventType::PlayerDamaged:
    case GameEventType::BossRamImpact:
        play(playerHit_, 0.72F, variedPitch(0.06F));
        break;
    case GameEventType::BossGroundSlam:
        playAt(
            structureHit_, event.position, snapshot, 0.92F,
            0.78F, 45.0F);
        break;
    case GameEventType::BossPhaseChanged:
    case GameEventType::BossWarCry:
        playAt(
            upgrade_, event.position, snapshot, 0.78F,
            event.type == GameEventType::BossWarCry
                ? 0.68F
                : 0.82F,
            45.0F);
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
    case GameEventType::RopeFallSaved:
        play(upgrade_, 0.72F, 1.08F);
        break;
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
        break;
    case GameEventType::ChestOpened:
        play(gate_, 0.62F, 0.78F);
        break;
    case GameEventType::ChestRerolled:
    case GameEventType::ChestRevealed:
    case GameEventType::BombPurchased:
    case GameEventType::AllBuildingsRepaired:
        play(uiConfirm_, 0.52F, 1.08F);
        break;
    case GameEventType::LootCollected:
        play(upgrade_, 0.78F, 1.08F);
        break;
    case GameEventType::BattlePotionActivated:
        play(upgrade_, 0.86F, 0.82F);
        break;
    case GameEventType::BuildingRejected:
    case GameEventType::BuildingUpgradeRejected:
    case GameEventType::BuildingRepairRejected:
    case GameEventType::BuildingSellRejected:
    case GameEventType::WeaponUpgradeRejected:
    case GameEventType::GateToggleRejected:
    case GameEventType::ChestOpenRejected:
    case GameEventType::EconomyPurchaseRejected:
    case GameEventType::CrystalStorageFull:
    case GameEventType::ResourceStorageFull:
        play(uiError_, 0.55F, 1.0F);
        break;
    default:
        break;
    }
}

void AudioSystem::playUiConfirm() {
    play(uiConfirm_, 0.5F, 1.0F, 0.5F, false);
}

void AudioSystem::playUiError() {
    play(uiError_, 0.55F, 1.0F, 0.5F, false);
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
    const float musicVolume = std::clamp(
        settings_.musicVolume, 0.0F, 1.0F);
    for (const MusicTrack& track : musicTracks_) {
        if (track.loaded) {
            SetMusicVolume(track.music, musicVolume);
        }
    }
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
    track.music.looping = false;
    SetMusicVolume(
        track.music,
        std::clamp(settings_.musicVolume, 0.0F, 1.0F));
}

void AudioSystem::unload(MusicTrack& track) {
    if (!track.loaded) {
        return;
    }
    StopMusicStream(track.music);
    UnloadMusicStream(track.music);
    track = {};
}

void AudioSystem::updateMusic() {
    if (!initialized_) {
        return;
    }
    if (!currentMusicTrack_ ||
        *currentMusicTrack_ >= musicTracks_.size() ||
        !musicTracks_[*currentMusicTrack_].loaded) {
        playNextMusicTrack();
        return;
    }
    Music& music = musicTracks_[*currentMusicTrack_].music;
    UpdateMusicStream(music);
    const float length = GetMusicTimeLength(music);
    const bool reachedEnd = length > 0.0F &&
        GetMusicTimePlayed(music) >= length - 0.08F;
    if (reachedEnd || !IsMusicStreamPlaying(music)) {
        playNextMusicTrack();
    }
}

void AudioSystem::playNextMusicTrack() {
    std::size_t loadedCount = 0;
    for (const MusicTrack& track : musicTracks_) {
        loadedCount += track.loaded ? 1U : 0U;
    }
    if (loadedCount == 0U) {
        currentMusicTrack_.reset();
        return;
    }
    if (currentMusicTrack_ &&
        *currentMusicTrack_ < musicTracks_.size() &&
        musicTracks_[*currentMusicTrack_].loaded) {
        StopMusicStream(
            musicTracks_[*currentMusicTrack_].music);
    }
    sequence_ = sequence_ * 1664525U + 1013904223U;
    const std::size_t start = static_cast<std::size_t>(
        sequence_) % musicTracks_.size();
    std::optional<std::size_t> selected;
    for (std::size_t offset = 0; offset < musicTracks_.size(); ++offset) {
        const std::size_t index =
            (start + offset) % musicTracks_.size();
        if (!musicTracks_[index].loaded) {
            continue;
        }
        if (loadedCount > 1U && currentMusicTrack_ &&
            index == *currentMusicTrack_) {
            continue;
        }
        selected = index;
        break;
    }
    if (!selected) {
        selected = currentMusicTrack_;
    }
    if (!selected) {
        return;
    }
    currentMusicTrack_ = *selected;
    Music& music = musicTracks_[*selected].music;
    SeekMusicStream(music, 0.0F);
    SetMusicVolume(
        music, std::clamp(settings_.musicVolume, 0.0F, 1.0F));
    PlayMusicStream(music);
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
    auto& clips =
        type == ResourceType::Wood ? woodHits_ : stoneHits_;
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
