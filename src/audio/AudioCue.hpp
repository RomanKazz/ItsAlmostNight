#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace ian {

enum class AudioCue {
    UiSelect, UiSelectAlternate, Coin, CrystalHit, CrystalBreak,
    IceCharge, IceShot, IceImpact, FireCharge, FireShot, FireImpact,
    Lightning, CannonShot, BowShot, CatapultShot, WindGust, Landing, Jump,
    ChestOpen, LootTake, ChestReroll, Purchase, Heal, Consume, Potion,
    Achievement, ChallengeComplete, LevelUp, EnemyDeath, BossSpawn,
    BossRoar, GroundSlam, RunStart, Defeat, Victory, ShieldBreak,
    ShieldRecharge, Trap, Count,
};

struct AudioCueAsset {
    std::string_view name;
    std::string_view fallback;
};

// Stable cue names let the temporary bank be replaced without gameplay edits.
inline constexpr std::array<AudioCueAsset, static_cast<std::size_t>(AudioCue::Count)> AudioCueAssets{{
    {"ui_select_0", "assets/audio/ui_confirm.wav"},
    {"ui_select_1", "assets/audio/ui_confirm.wav"},
    {"coin_pickup", "assets/audio/crystals.ogg"},
    {"crystal_hit", "assets/audio/stone_hit_0.ogg"},
    {"crystal_break", "assets/audio/crystals.ogg"},
    {"ice_charge", "assets/audio/crystals.ogg"},
    {"ice_shot", "assets/audio/tool_swish_0.wav"},
    {"ice_impact", "assets/audio/explosion.wav"},
    {"fire_charge", "assets/audio/crystals.ogg"},
    {"fire_shot", "assets/audio/tool_swish_1.wav"},
    {"fire_impact", "assets/audio/explosion.wav"},
    {"lightning", "assets/audio/critical.ogg"},
    {"cannon_shot", "assets/audio/rifle_shot.wav"},
    {"bow_shot", "assets/audio/tool_swish_0.wav"},
    {"catapult_shot", "assets/audio/tool_swish_1.wav"},
    {"wind_gust", "assets/audio/tool_swish_0.wav"},
    {"landing", "assets/audio/footstep_grass_0.ogg"},
    {"jump", "assets/audio/tool_swish_0.wav"},
    {"chest_open", "assets/audio/gate.ogg"},
    {"loot_take", "assets/audio/upgrade.wav"},
    {"chest_reroll", "assets/audio/crystals.ogg"},
    {"purchase", "assets/audio/ui_confirm.wav"},
    {"heal", "assets/audio/repair.ogg"},
    {"consume", "assets/audio/ui_confirm.wav"},
    {"potion", "assets/audio/upgrade.wav"},
    {"achievement", "assets/audio/upgrade.wav"},
    {"challenge_complete", "assets/audio/upgrade.wav"},
    {"level_up", "assets/audio/upgrade.wav"},
    {"enemy_death", "assets/audio/enemy_hit.ogg"},
    {"boss_spawn", "assets/audio/wave_warning.wav"},
    {"boss_roar", "assets/audio/wave_warning.wav"},
    {"ground_slam", "assets/audio/explosion.wav"},
    {"run_start", "assets/audio/ui_confirm.wav"},
    {"defeat", "assets/audio/player_hit.ogg"},
    {"victory", "assets/audio/upgrade.wav"},
    {"shield_break", "assets/audio/structure_break.ogg"},
    {"shield_recharge", "assets/audio/repair.ogg"},
    {"trap", "assets/audio/turret_hit.ogg"},
}};

} // namespace ian
