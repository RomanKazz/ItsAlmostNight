#include "audio/AudioSystem.hpp"
#include "TestHarness.hpp"
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace ian {
struct AudioSystemTestAccess {
    static void resetHitAndExplosion(AudioSystem& audio) {
        if (audio.playerHit_.loaded) StopSound(audio.playerHit_.sound);
        audio.playerHitSoundCooldown_ = 0;
        if (audio.explosion_.loaded) StopSound(audio.explosion_.sound);
        for (const auto& clip : audio.explosionAliases_) if (clip.loaded) StopSound(clip.sound);
        audio.explosionSoundBudget_.reset();
    }
    static bool playerHitPlaying(const AudioSystem& audio) {
        return audio.playerHit_.loaded && IsSoundPlaying(audio.playerHit_.sound);
    }
    static std::size_t activeExplosions(const AudioSystem& audio) {
        std::size_t count = audio.explosion_.loaded && IsSoundPlaying(audio.explosion_.sound) ? 1U : 0U;
        for (const auto& clip : audio.explosionAliases_) count += clip.loaded && IsSoundPlaying(clip.sound) ? 1U : 0U;
        return count;
    }
    static void stopShotVoices(AudioSystem& audio) {
        for (const auto& clip : audio.playerRifleVoices_) if (clip.loaded) StopSound(clip.sound);
        for (const auto& clip : audio.defenseRifleVoices_) if (clip.loaded) StopSound(clip.sound);
        audio.defenseSoundBudget_.reset();
        audio.pendingDefenseSounds_.fill({});
    }
    static void flushDefense(AudioSystem& audio, const SimulationSnapshot& snapshot, double seconds = 0) {
        audio.flushDefenseSounds(snapshot, seconds);
    }
    static void resetDefense(AudioSystem& audio) {
        stopShotVoices(audio);
        for (const auto& clip : audio.turretHits_) if (clip.loaded) StopSound(clip.sound);
        for (auto type : {AudioCue::CannonShot, AudioCue::FireShot, AudioCue::CatapultShot,
                AudioCue::Lightning, AudioCue::BowShot, AudioCue::WindGust, AudioCue::Trap}) {
            const auto& clip = audio.cue(type);
            if (clip.loaded) StopSound(clip.sound);
        }
        audio.nextDefenseSoundGroup_ = 0;
    }
    static std::size_t defenseVoices(const AudioSystem& audio, DefenseSoundGroup group) {
        return audio.activeDefenseVoices(group);
    }
    static Vec3 pendingPosition(const AudioSystem& audio, DefenseSoundGroup group) {
        return audio.pendingDefenseSounds_[static_cast<std::size_t>(group)].position;
    }
    static bool defenseTransientLoaded(const AudioSystem& audio) {
        return audio.defenseGunShot_.loaded &&
            std::abs(static_cast<double>(audio.defenseGunShot_.sound.frameCount) /
                audio.defenseGunShot_.sound.stream.sampleRate - .18) < .002;
    }
    static std::size_t activePlayerShots(const AudioSystem& audio) {
        return static_cast<std::size_t>(std::count_if(audio.playerRifleVoices_.begin(),
            audio.playerRifleVoices_.end(),[](const auto& clip) {return clip.loaded && IsSoundPlaying(clip.sound);}));
    }
    static std::size_t activeDefenseShots(const AudioSystem& audio) {
        return static_cast<std::size_t>(std::count_if(audio.defenseRifleVoices_.begin(),
            audio.defenseRifleVoices_.end(),[](const auto& clip) {return clip.loaded && IsSoundPlaying(clip.sound);}));
    }
    static bool shotPoolsReset(const AudioSystem& audio) {
        return !audio.defenseGunShot_.loaded &&
            std::none_of(audio.playerRifleVoices_.begin(),audio.playerRifleVoices_.end(),
            [](const auto& clip) {return clip.loaded;}) &&
            std::none_of(audio.defenseRifleVoices_.begin(),audio.defenseRifleVoices_.end(),
            [](const auto& clip) {return clip.loaded;});
    }
    static bool muckMusicLoaded(const AudioSystem& audio) {
        return std::all_of(audio.musicTracks_.begin(), audio.musicTracks_.end(),
            [](const auto& track) { return track.loaded && !track.music.looping; }) &&
            audio.forestAmbience_.loaded && audio.forestAmbience_.music.looping;
    }
    static std::optional<std::size_t> currentMusicTrack(const AudioSystem& audio) {
        return audio.currentMusicTrack_;
    }
};
}

namespace {
bool checkAssets() {
    std::ifstream input("assets/audio/temporary/manifest.json");
    const auto manifest = nlohmann::json::parse(input);
    std::set<std::string> names;
    bool valid = true;
    for (const auto& asset : manifest.at("assets")) {
        const std::string name = asset.at("cue");
        const std::string path = "assets/audio/temporary/" + asset.at("file").get<std::string>();
        if (!names.insert(name).second) valid = false;
        Wave wave = LoadWave(path.c_str());
        if (!IsWaveValid(wave)) {
            std::cerr << "Invalid audio: " << path << '\n';
            valid = false;
            UnloadWave(wave);
            continue;
        }
        const bool validBefore = valid;
        const double duration = static_cast<double>(wave.frameCount) / wave.sampleRate;
        valid &= std::abs(duration - asset.at("duration").get<double>()) < 0.03;
        valid &= wave.sampleRate == asset.at("sample_rate").get<unsigned>();
        valid &= wave.channels == asset.at("channels").get<unsigned>();
        float* samples = LoadWaveSamples(wave);
        bool audible = false;
        if (!samples) valid = false;
        else for (std::size_t i = 0; i < static_cast<std::size_t>(wave.frameCount) * wave.channels; ++i) {
            valid &= std::isfinite(samples[i]);
            audible |= std::abs(samples[i]) > .001F;
        }
        valid &= audible;
        if (!valid && validBefore) std::cerr << "Audio metadata/sample mismatch: " << path
            << " duration=" << duration << " expected=" << asset.at("duration")
            << " rate=" << wave.sampleRate << " channels=" << wave.channels << " audible=" << audible << '\n';
        UnloadWaveSamples(samples);
        UnloadWave(wave);
    }
    for (const auto& asset : ian::AudioCueAssets) {
        if(names.contains(std::string(asset.name))) continue;
        // Original cues live outside the temporary gamerip manifest.
        Wave wave=LoadWave(asset.fallback.data());
        bool cueValid=IsWaveValid(wave) && wave.frameCount>0 && wave.sampleRate>0;
        float* samples=cueValid?LoadWaveSamples(wave):nullptr;
        bool audible=false;
        if(!samples) cueValid=false;
        else for(std::size_t i=0;i<static_cast<std::size_t>(wave.frameCount)*wave.channels;++i) {
            cueValid &= std::isfinite(samples[i]);
            audible |= std::abs(samples[i])>.001F;
        }
        cueValid &= audible;
        if(!cueValid) std::cerr<<"Invalid original audio cue: "<<asset.fallback<<'\n';
        valid &= cueValid;
        UnloadWaveSamples(samples);UnloadWave(wave);
    }
    for (const auto& file : std::filesystem::directory_iterator("assets/audio/temporary")) {
        if (file.path().extension() == ".ogg") valid &= names.contains(file.path().stem().string());
    }
    return valid;
}
}
int main() {
    using Group = ian::DefenseSoundGroup;
    {
        ian::DefenseSoundBudget defense;
        require(defense.reserve(Group::Gun, 0, 0).has_value(), "first defense shot is audible");
        require(!defense.reserve(Group::Gun, 1, 1), "same-family bursts collapse into one onset");
        const auto quieter = defense.reserve(Group::Bow, 0, 1);
        require(quieter && *quieter < 1, "overlapping defense voices leave mix headroom");
        require(!defense.reserve(Group::Artillery, 0, 2), "one frame cannot start an unlimited mixed volley");
        defense.tick(.1);
        require(!defense.reserve(Group::Artillery, 0, 5), "global defense polyphony is bounded");
        require(defense.reserve(Group::Artillery, 0, 2).has_value(), "full-pool rejection does not consume onset tokens");
        defense.reset();
        require(!defense.reserve(Group::Gun, 2, 2) && !defense.reserve(Group::Impact, 1, 1),
            "gun and impact pools never restart occupied tails");
        defense.tick(20);
        require(defense.reserve(Group::Gun, 0, 0).has_value() &&
            defense.reserve(Group::Bow, 0, 1).has_value() &&
            !defense.reserve(Group::Artillery, 0, 2), "a long frame cannot accumulate an unlimited burst");
        for (int fps : {30, 60, 240}) {
            defense.reset();
            int onsets = 0;
            for (int frame = 0; frame < fps * 5; ++frame) {
                defense.tick(1.0 / fps);
                for (int shot = 0; shot < 1000; ++shot)
                    if (defense.reserve(static_cast<Group>(static_cast<std::size_t>(shot) %
                        ian::DefenseSoundGroupCount), 0, 0)) ++onsets;
            }
            require(onsets >= 68 && onsets <= 72,
                "thousands of mixed shots obey the same onset rate at every frame rate");
        }
    }
    ian::ExplosionSoundBudget budget;
    require(budget.reserve(0).has_value(), "first explosion is audible");
    for (int i = 0; i < 1000; ++i)
        require(!budget.reserve(0), "same-frame explosions share one sound onset");
    budget.tick(.139);
    require(!budget.reserve(0), "explosion cooldown spans render frames");
    budget.tick(.001);
    require(budget.reserve(1).has_value(), "new voice may enter after cooldown");
    budget.tick(1.0);
    require(!budget.reserve(3), "full voice pool cannot restart or steal a tail");
    require(budget.reserve(2).has_value(), "a rejected full pool does not consume cooldown");
    float summedGain = 0.0F;
    for (std::size_t active = 0; active < 3; ++active) {
        budget.reset();
        summedGain += *budget.reserve(active);
    }
    require(summedGain < 1.0F, "overlapping explosion voices retain mix headroom");
    budget.reset();
    int starts = 0;
    for (int tick = 0; tick < 600; ++tick) {
        budget.tick(1.0 / 60.0);
        for (int burst = 0; burst < 30; ++burst)
            if (budget.reserve(0)) ++starts;
    }
    require(starts >= 60 && starts <= 72, "dense artillery keeps a bounded audible rhythm");
    SetTraceLogLevel(LOG_WARNING);
    if (!checkAssets()) return 1;
    ian::AudioSystem audio;
    // Exercise real loading, streams, aliases, events and repeated shutdown silently.
    audio.settings().muted = true;
    for (int cycle = 0; cycle < 2; ++cycle) {
        audio.settings().sfxVolume = 1;
        audio.initialize();
        if (!IsAudioDeviceReady()) {
            std::cout << "Assets validated; audio device unavailable for lifecycle check.\n";
            return 77;
        }
        require(ian::AudioSystemTestAccess::muckMusicLoaded(audio),
            "all Muck themes and looping ambience load");
        require(ian::AudioSystemTestAccess::defenseTransientLoaded(audio),
            "turret audio has a short faded transient instead of a full musket tail");
        ian::SimulationSnapshot snapshot{};
        snapshot.state = ian::RunState::BuildPhase;
        snapshot.playerHealth = 100;
        snapshot.playerMaxHealth = 100;
        snapshot.playerGrounded = true;
        for (int type = 0; type <= static_cast<int>(ian::GameEventType::ChallengeCompleted); ++type) {
            ian::GameEvent event{};
            event.type = static_cast<ian::GameEventType>(type);
            event.resourceType = ian::ResourceType::Crystal;
            event.buildingType = ian::BuildingType::Cannon;
            audio.playEvent(event, snapshot);
            audio.update(snapshot);
        }
        audio.playEvent({.type=ian::GameEventType::StumpRootWave,.position={}},snapshot);
        std::array<ian::TowerRuntime, 32> flameTowers{};
        for (auto& tower : flameTowers) {
            tower.type = ian::BuildingType::Flamethrower;
            tower.flameRemaining = 1;
            tower.flameOrigin = {0, 1, -2};
        }
        snapshot.towers = flameTowers;
        snapshot.state = ian::RunState::Wave;
        audio.update(snapshot); // 32 emitters still share one loaded flame loop.
        snapshot.runShop.pending = true;
        audio.update(snapshot);
        snapshot.runShop.pending = false;
        snapshot.towers = {};
        for (auto state : {ian::RunState::MainMenu, ian::RunState::BuildPhase,
             ian::RunState::Wave, ian::RunState::Paused, ian::RunState::Defeat, ian::RunState::Victory}) {
            snapshot.state = state;
            audio.update(snapshot);
            if (state == ian::RunState::MainMenu)
                require(ian::AudioSystemTestAccess::currentMusicTrack(audio) == 0U,
                    "Day0 is the menu theme");
            if (state == ian::RunState::BuildPhase)
                require(ian::AudioSystemTestAccess::currentMusicTrack(audio) == 1U,
                    "day starts with Day1");
            if (state == ian::RunState::Wave)
                require(ian::AudioSystemTestAccess::currentMusicTrack(audio) == 3U,
                    "night starts with Night0");
        }
        snapshot.runUpgradeChoicePending = true;
        for (float opacity : {0.0F, 0.4F, 1.0F, 0.4F, 0.0F}) audio.update(snapshot, opacity);
        // Closing must stop the window loop; reopening a queued level starts it again.
        snapshot.runUpgradeChoicePending = false;
        audio.update(snapshot, 0.0F);
        snapshot.runUpgradeChoicePending = true;
        audio.update(snapshot, 1.0F);
        snapshot.playerGrounded = false;
        snapshot.playerVerticalVelocity = 2;
        snapshot.state = ian::RunState::BuildPhase;
        audio.update(snapshot);
        for (auto type : {ian::BuildingType::Turret, ian::BuildingType::GunTurret,
                          ian::BuildingType::Cannon, ian::BuildingType::Catapult}) {
            audio.playDefenseShot(type, {}, snapshot);
        }
        // Start real voices silently, exercising aliases and shared impact
        // admission without using the muted early return.
        snapshot.runUpgradeChoicePending = false;
        audio.settings().muted = false;
        audio.settings().masterVolume = 0;
        audio.applySettings();
        const ian::GameEvent musket{.type=ian::GameEventType::WeaponFired};
        ian::AudioSystemTestAccess::stopShotVoices(audio);
        audio.playEvent(musket,snapshot);
        audio.playDefenseShot(ian::BuildingType::GunTurret,{},snapshot);
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::activePlayerShots(audio)==1 &&
                ian::AudioSystemTestAccess::activeDefenseShots(audio)==1,
                "turret shot has its own voice while the musket tail continues");
        for (int shot=0;shot<7;++shot) audio.playEvent(musket,snapshot);
        require(ian::AudioSystemTestAccess::activePlayerShots(audio)==8,
                "fast musket shots overlap using preloaded aliases");
        std::this_thread::sleep_for(std::chrono::milliseconds(280));
        audio.playEvent(musket,snapshot); // Full pool must not restart an old tail.
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        require(ian::AudioSystemTestAccess::activePlayerShots(audio)==0,
                "a full musket pool skips excess sounds instead of truncating or restarting them");
        ian::AudioSystemTestAccess::resetDefense(audio);
        for (int i = 0; i < 1000; ++i) {
            audio.playDefenseShot(ian::BuildingType::GunTurret, {20, 0, 0}, snapshot);
            audio.playDefenseShot(ian::BuildingType::GunTurret, {1000, 0, 0}, snapshot);
        }
        audio.playDefenseShot(ian::BuildingType::GunTurret, {2, 0, 0}, snapshot);
        require(ian::AudioSystemTestAccess::pendingPosition(audio, Group::Gun).x == 2,
            "the nearest audible turret represents the whole same-frame volley");
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::activeDefenseShots(audio) == 1,
            "a thousand turret events start only one representative voice");
        ian::AudioSystemTestAccess::resetDefense(audio);
        for (auto type : {ian::BuildingType::Flamethrower, ian::BuildingType::LaserTower,
                ian::BuildingType::TeslaTower}) audio.playDefenseShot(type, {}, snapshot);
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::activeDefenseShots(audio) == 0,
            "flame, laser and Tesla cannot fall through to a musket shot");
        for (int bounce = 0; bounce < 20; ++bounce)
            audio.playEvent({.type = ian::GameEventType::ChainLightningHit,
                .buildingType = ian::BuildingType::TeslaTower, .amount = bounce}, snapshot);
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::defenseVoices(audio, Group::Lightning) == 1,
            "a Tesla chain owns one sound regardless of bounce count");
        ian::AudioSystemTestAccess::resetDefense(audio);
        for (int i = 0; i < 1000; ++i) audio.playTurretImpact({}, snapshot, i % 2 == 0);
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::defenseVoices(audio, Group::Impact) == 1,
            "mass projectile impacts share one voice without restarting it");
        ian::AudioSystemTestAccess::resetDefense(audio);
        audio.playDefenseShot(ian::BuildingType::GunTurret, {}, snapshot);
        auto paused = snapshot;
        paused.state = ian::RunState::Paused;
        ian::AudioSystemTestAccess::flushDefense(audio, paused);
        ian::AudioSystemTestAccess::flushDefense(audio, snapshot);
        require(ian::AudioSystemTestAccess::activeDefenseShots(audio) == 0,
            "queued shots cannot burst out after leaving a pause or upgrade menu");
        ian::AudioSystemTestAccess::resetHitAndExplosion(audio);
        const ian::GameEvent mobHit{.type=ian::GameEventType::PlayerDamaged,
            .sourceId=ian::EntityId{1,1},.amount=12};
        audio.playEvent(mobHit,snapshot);
        require(ian::AudioSystemTestAccess::playerHitPlaying(audio),
            "enemy attacks play the actual player impact voice");
        for(int i=0;i<200;++i) audio.playEvent(mobHit,snapshot);
        require(ian::AudioSystemTestAccess::playerHitPlaying(audio),
            "a crowd does not restart or cut off the current player hit");
        const ian::GameEvent coal{.type=ian::GameEventType::ItemExplosion,.position={},.intensity=3};
        audio.playEvent(coal,snapshot);
        require(ian::AudioSystemTestAccess::activeExplosions(audio)==1,
            "burn-death item explosions have real audio");
        for(int i=0;i<200;++i) audio.playEvent(coal,snapshot);
        require(ian::AudioSystemTestAccess::activeExplosions(audio)==1,
            "coal chain reactions share the bounded explosion onset budget");
        for (int burst = 0; burst < 300; ++burst) {
            for (auto type : {ian::GameEventType::Explosion,
                              ian::GameEventType::IceWandImpact,
                              ian::GameEventType::FireWandImpact,
                              ian::GameEventType::BossGroundSlam}) {
                ian::GameEvent event{};
                event.type = type;
                audio.playEvent(event, snapshot);
            }
        }
        audio.playUiSelect();
        audio.playUiSelect();
        audio.playUiConfirm();
        audio.playUiPurchase();
        audio.playUiReward();
        audio.playUiError();
        audio.settings().musicVolume = 0;
        audio.settings().sfxVolume = 0;
        audio.applySettings();
        audio.update(snapshot);
        audio.shutdown();
        audio.shutdown();
        require(ian::AudioSystemTestAccess::shotPoolsReset(audio),"shot aliases unload before shared PCM and reset for reinitialization");
    }
    std::cout << "Audio assets and lifecycle passed.\n";
}
