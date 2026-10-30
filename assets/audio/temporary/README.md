# Temporary local audio bank

User-provided **Mega Bonk (Linux, Steam, Windows) (gamerip) (2025) (OGG)**
files are used as placeholders for local gameplay tests. These are not covered by
the Kenney licenses of the original audio folder. Replace this bank with original
audio before distributing a build.

`manifest.json` records each cue's original filename, duration, rate and channels.
The copies retain their original OGG encoding. Stable filenames are independent
of the source's numbered track names.

## Replacing the bank

Replace any `.ogg` here with an original sound bearing the same filename. Rebuild
to refresh the runtime asset copy. Audio volume uses the existing master, music
and effects controls. Short, mono files are preferable for positional effects;
music and ambience can be stereo. Keep the cue names and update the manifest's
metadata when changing files so the audio smoke test remains accurate.

`AudioSystem` prefers these files over the existing sounds. Removing this folder
restores the original files as fallbacks; forest ambience then becomes silent.
This folder does not alter resource costs, combat timing or drop probabilities.

## Cue groups

- `ui_confirm`, `ui_error`: clicks, keyboard confirmation and rejected actions.
  Hovering is silent; confirmation uses `066. Enter5.ogg`.
- `ui_select_0`, `ui_select_1`: `242. Select.ogg` and `243. Select2.ogg` for
  changing a selection by keyboard or clicking a different tab/item. A clamped
  navigation step that leaves the selection unchanged stays silent.
- `purchase`, `achievement`, `challenge_complete`, `level_up`, `loot_take`:
  shop purchases, completed quests/challenges, upgrade choices and collected loot.
- `chest_open`, `chest_reroll`, `consume`, `heal`, `potion`: chests and consumables.
- `structure_hit`, `structure_break`, `repair`, `upgrade`, `gate`:
  buildings and gates. Placement remains synchronized with the placement pop.
- `critical`, `rifle_shot`, `explosion`, `enemy_hit`, `turret_hit`,
  `player_hit`, `enemy_death`, `ice_*`, `fire_*`, `lightning`, `bow_shot`,
  `cannon_shot`, `catapult_shot`, `trap`: melee, weapons and defense combat.
- `crystals`, `crystal_hit`, `crystal_break`, `coin_pickup`, `xp_pickup`: production,
  mining, currency and experience. Wood, stone and crystal mining use the
  original randomized impacts and breaks.
- `jump`, `dash`, `landing`, `shield_break`, `shield_recharge`:
  player movement and protection.
- `run_start`, `defeat`, `victory`, `wave_warning`, `boss_spawn`, `boss_roar`,
  `ground_slam`: run, wave and boss events.
- Contextual music and forest ambience now come from `muck/*.mp3`, described in
  `muck/README.md`. Pauses and upgrade choices lower the music level.
- `levelup_after`: `112. Levelup After.ogg`, played once when the final upgrade
  choice finishes closing the window and gameplay resumes.

Original grass footsteps, weapon/tool sweeps and building placement sound are
preserved, including their volume and playback speed.

Dense repeated impacts/pickups are throttled; positional effects retain distance
attenuation and stereo panning. Footsteps pause with gameplay. The pack is a first
mix for testing: subjective volume and cue choices can be tuned after listening
in an actual run.
