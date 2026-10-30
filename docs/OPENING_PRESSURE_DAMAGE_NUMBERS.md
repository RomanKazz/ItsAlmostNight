# Opening pressure and crowded damage text (2026-10-30)

Night one now uses an eight-basic-enemy roster, groups of two every two seconds,
with six active enemies at the start (up to eight after late-night rounding).
Previously this was ten basics, groups of three every 1.6 seconds, and eight to
ten active enemies. Basic health scales by 0.65 instead of 0.70 and damage by
0.60 instead of 0.68. The shipped JSON and fallback roster match. Later-night
stats, rosters, movement growth and endless pressure are unchanged.

Damage labels no longer create higher rows when nearby labels fill the lanes.
Seven horizontal lanes stay near the impact anchor; saturated local clusters
replace their oldest label. The overall 32-label budget and world-depth
occlusion remain in place. Immediate hits on the same entity within 120 ms
sum into one label without refreshing its lifetime. Critical and delayed hits
remain separate, and entity generations prevent cross-target aggregation.

Validation covers first-night cadence, reinforcements, forecast/default-data
consistency, later-night progression, burst aggregation, lane saturation,
critical separation, delayed impacts and label expiration.
