# Enemy pressure and explosion audio

Wave one retains the previous HP multiplier (0.85 × 0.92 in the current build).
For `n = max(1, wave) - 1`, the wave health growth is:

```
1 + 0.24 * n + 0.035 * n * n
```

Compared with wave one, waves 3/6/12/20 have 1.62/3.075/7.875/18.195 times
HP before rounding. This replaces `1 + 0.10 * n` and removes the late-wave
regular-enemy HP discount. Bosses use this curve plus their existing boss-cycle
HP multiplier. Ordinary damage growth, compositions and active-enemy limits are
unchanged. Polynomial growth remains finite across the supported integer wave IDs.

All elite affixes share **4× HP and 1.5× damage**, replacing 1.6×/1.1×. The actual
HP follows existing enemy-health rounding. Affix mechanics and status resistance
remain unchanged. Elite splitters' ordinary children inherit wave scaling, without
elite multipliers, for both direct and area damage kills.

Tuning lives in `src/enemies/EnemyDifficulty.hpp`. These are initial stronger
balance values, not measured proof of difficulty. Playtest ordinary and strong
builds through waves 3, 6 and 12; assess time to kill, core damage and elite
threat. Check first-boss durability especially. Do not retune based on godmode runs.

## Explosion audio

`ExplosionSoundBudget` admits one onset per **0.14 seconds**, with **three** voices
shared between cannon/bomb explosions, fire/ice impacts and ground slams. Loud
explosions have a maximum pre-attenuation gain of **0.40**. With already active
voices, the new voice gain is divided by `sqrt(active + 1)`.

Explosions use two sound aliases plus the original clip. Occupied voices are never
restarted or stolen. Full pools skip audio only. Inaudible distant impacts and
muted audio do not consume budget. Alias buffers are released before their source
on shutdown. Projectile counts, damage and visual effects remain untouched.

Validation: `ian_enemy_pressure_tests` covers actual wave plans, every elite affix,
ordinary split children and dense explosion admission. `ian_audio_smoke` additionally
exercises real audio loading/playback/shutdown. Full Debug and sanitizer CTest are
also required.
