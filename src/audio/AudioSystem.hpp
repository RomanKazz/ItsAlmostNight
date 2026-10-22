#pragma once

#include "game/Simulation.hpp"
#include "audio/AudioCue.hpp"
#include "audio/ExplosionSoundBudget.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string_view>

namespace ian {

struct AudioSettings {
    float masterVolume{0.78F};
    float musicVolume{0.65F};
    float sfxVolume{1.0F};
    bool muted{};

    bool operator==(const AudioSettings&) const = default;
};

class AudioSystem {
  public:
    AudioSystem() = default;
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    void initialize();
    void shutdown();

    void update(const SimulationSnapshot& snapshot, float upgradeOverlayOpacity = 1.0F);
    void playEvent(const GameEvent& event,
                   const SimulationSnapshot& snapshot);
    void playTurretImpact(Vec3 position,
                          const SimulationSnapshot& snapshot,
                          bool secondaryImpact);
    void queueBuildingPlaced(
        Vec3 position, double delaySeconds = 0.0);
    void playHarvestSwing();
    void playUiConfirm();
    void playUiSelect();
    void playUiError();
    void playUiPurchase();
    void playUiReward();
    void playDefenseShot(BuildingType type, Vec3 position,
                         const SimulationSnapshot& snapshot);
    [[nodiscard]] AudioSettings& settings();
    [[nodiscard]] const AudioSettings& settings() const;
    void applySettings();

  private:
    friend struct AudioSystemTestAccess;
    struct Clip {
        Sound sound{};
        bool loaded{};
    };
    struct MusicTrack {
        Music music{};
        bool loaded{};
        float volume{};
    };
    struct PendingBuildingPlacedSound {
        Vec3 position;
        double delayRemaining{};
    };

    void load(Clip& clip, std::string_view path);
    void unload(Clip& clip);
    void load(MusicTrack& track, std::string_view path);
    void unload(MusicTrack& track);
    void updateMusic(const SimulationSnapshot& snapshot, float upgradeOverlayOpacity);
    void loadCue(Clip& clip, std::string_view name, std::string_view fallback);
    [[nodiscard]] const Clip& cue(AudioCue type) const;
    void playCue(AudioCue type, Vec3 position, const SimulationSnapshot& snapshot,
                 float volume = 0.5F, float maximumDistance = 38.0F,
                 double cooldown = 0.06);
    void play(const Clip& clip, float volume = 1.0F,
              float pitch = 1.0F, float pan = 0.5F,
              bool affectedByLowHealth = true);
    void playAt(const Clip& clip, Vec3 position,
                const SimulationSnapshot& snapshot,
                float volume = 1.0F, float pitch = 1.0F,
                float maximumDistance = 38.0F);
    void playResourceHit(ResourceType type, Vec3 position,
                         const SimulationSnapshot& snapshot);
    void playExplosion(const Clip& clip, Vec3 position,
                       const SimulationSnapshot& snapshot,
                       float volume, float maximumDistance);
    [[nodiscard]] float variedPitch(float spread);

    bool levelupChoiceWasPending_{};
    bool initialized_{};
    bool ownsAudioDevice_{};
    AudioSettings settings_;
    std::uint32_t sequence_{0x51f15e1dU};
    double defenseGunSoundCooldown_{};
    std::array<Clip, 5> woodHits_;
    std::array<Clip, 5> stoneHits_;
    std::array<Clip, 5> grassFootsteps_;
    std::array<Clip, 4> harvestSwishes_;
    Clip woodBreak_;
    Clip stoneBreak_;
    Clip critical_;
    Clip rifleShot_;
    // Aliases share PCM data, but player and defenses own separate playback voices.
    std::array<Clip, 8> playerRifleVoices_;
    std::array<Clip, 4> defenseRifleVoices_;
    Clip explosion_;
    std::array<Clip, ExplosionSoundBudget::MaximumVoices - 1U> explosionAliases_;
    ExplosionSoundBudget explosionSoundBudget_;
    Clip buildPlace_;
    Clip structureHit_;
    Clip structureBreak_;
    Clip repair_;
    Clip upgrade_;
    Clip levelupAfter_;
    Clip gate_;
    Clip crystals_;
    Clip xpPickup_;
    Clip playerHit_;
    double playerHitSoundCooldown_{};
    Clip enemyHit_;
    std::array<Clip, 6> turretHits_;
    std::size_t nextTurretHit_{};
    Clip uiError_;
    Clip uiConfirm_;
    Clip waveWarning_;
    std::array<Clip, static_cast<std::size_t>(AudioCue::Count)> cues_;
    std::array<double, static_cast<std::size_t>(AudioCue::Count)> cueCooldowns_{};
    std::array<MusicTrack, 8> musicTracks_;
    MusicTrack forestAmbience_;
    MusicTrack levelupAmbience_;
    MusicTrack flamethrowerLoop_;
    MusicTrack laserLoop_;
    std::optional<std::size_t> currentMusicTrack_;
    std::optional<std::size_t> currentMusicGroup_;
    bool currentMusicTrackStarted_{};
    std::size_t nextBossTheme_{};
    double uiErrorSoundCooldown_{};
    bool upgradeChoiceWasVisible_{};
    bool playerWasGrounded_{true};
    bool bossWasPresent_{};
    std::optional<Vec3> previousPlayerPosition_;
    std::deque<PendingBuildingPlacedSound>
        pendingBuildingPlacedSounds_;
    double buildingPlacedSoundCooldown_{};
    double footstepDistance_{};
    double iceHitSoundCooldown_{};
    double xpPickupSoundCooldown_{};
    float lowHealthAmount_{};
};

} // namespace ian
