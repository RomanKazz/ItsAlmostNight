#pragma once

#include "game/Simulation.hpp"
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

    void update(const SimulationSnapshot& snapshot);
    void playEvent(const GameEvent& event,
                   const SimulationSnapshot& snapshot);
    void queueBuildingPlaced(
        Vec3 position, double delaySeconds = 0.0);
    void playHarvestSwing();
    void playUiConfirm();
    void playUiError();
    [[nodiscard]] AudioSettings& settings();
    [[nodiscard]] const AudioSettings& settings() const;
    void applySettings();

  private:
    struct Clip {
        Sound sound{};
        bool loaded{};
    };
    struct MusicTrack {
        Music music{};
        bool loaded{};
    };
    struct PendingBuildingPlacedSound {
        Vec3 position;
        double delayRemaining{};
    };

    void load(Clip& clip, std::string_view path);
    void unload(Clip& clip);
    void load(MusicTrack& track, std::string_view path);
    void unload(MusicTrack& track);
    void updateMusic();
    void playNextMusicTrack();
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
    std::array<Clip, 5> woodHits_;
    std::array<Clip, 5> stoneHits_;
    std::array<Clip, 5> grassFootsteps_;
    std::array<Clip, 4> harvestSwishes_;
    Clip woodBreak_;
    Clip stoneBreak_;
    Clip critical_;
    Clip rifleShot_;
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
    Clip playerHit_;
    Clip enemyHit_;
    Clip turretHit_;
    Clip uiError_;
    Clip uiConfirm_;
    Clip waveWarning_;
    std::array<MusicTrack, 3> musicTracks_;
    std::optional<std::size_t> currentMusicTrack_;
    std::optional<Vec3> previousPlayerPosition_;
    std::deque<PendingBuildingPlacedSound>
        pendingBuildingPlacedSounds_;
    double buildingPlacedSoundCooldown_{};
    double footstepDistance_{};
    double iceHitSoundCooldown_{};
    float lowHealthAmount_{};
};

} // namespace ian
