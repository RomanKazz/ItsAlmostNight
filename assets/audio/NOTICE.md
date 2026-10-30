# Audio asset sources

- `wood_*`, `stone_*`, `footstep_grass_*`, `critical`,
  `structure_*`, `build_place`, `player_hit`, `enemy_hit`, and
  `turret_hit`: Kenney Impact Sounds, CC0.
  See `LICENSE-KENNEY-IMPACT.txt`.
- `repair`, `crystals`, and `gate`: Kenney RPG Audio, CC0.
  See `LICENSE-KENNEY-RPG.txt`.
- `xp_pickup`: shortened and pitched version of `crystals`, Kenney RPG
  Audio, CC0. See `LICENSE-KENNEY-RPG.txt`.
- `rifle_shot`, `upgrade`, `explosion`, `ui_error`, `ui_confirm`, and
  `wave_warning`: user-provided `400 Sounds Pack`.
- `temporary/*.ogg`: user-provided Mega Bonk gamerip (2025), used temporarily
  for local testing. These files are separate from the licensed original sounds;
  see `temporary/README.md` and `temporary/manifest.json` for sources and replacement.
- `temporary/muck/*.mp3`: Muck soundtrack from the [Muck Wiki music page](https://muck.fandom.com/wiki/Music).
  The user reports permission
  to use these tracks in this game as temporary music. See `temporary/muck/README.md`
  for the track-to-context mapping and replacement instructions.

- `flamethrower_loop.wav`: original procedural seamless flame-jet sound created
  for this project; source generator `tools/create_flamethrower_audio.py`.

`laser_loop.wav`: procedural loop authored for this project; no external samples.

`stump_roots.wav`: original procedural wood, earth and leaves effect; no external
samples. Reproduce with `tools/create_stump_audio.py`.

`feedback_shutter.wav`: original procedural mechanical camera shutter; no external
samples. Reproduce with `tools/generate_feedback_shutter.py`.
