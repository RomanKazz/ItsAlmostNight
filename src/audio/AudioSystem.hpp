#pragma once

#include "game/Simulation.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
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
    [[nodiscard]] float variedPitch(float spread);

    bool initialized_{};
    bool ownsAudioDevice_{};
    AudioSettings settings_;
    std::uint32_t sequence_{0x51f15e1dU};
    std::array<Clip, 5> woodHits_;
    std::array<Clip, 5> stoneHits_;
    std::array<Clip, 5> grassFootsteps_;
    Clip woodBreak_;
    Clip stoneBreak_;
    Clip critical_;
    Clip rifleShot_;
    Clip explosion_;
    Clip buildPlace_;
    Clip structureHit_;
    Clip structureBreak_;
    Clip repair_;
    Clip upgrade_;
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
    double footstepDistance_{};
    double iceHitSoundCooldown_{};
    float lowHealthAmount_{};
};

} // namespace ian
