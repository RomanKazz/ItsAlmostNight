# Cannon attack specializations

A level-up can offer one of three cannon attack styles, provided the run owns
Cannon in its arsenal (a placed instance is not required). Sandbox may also
choose a style. A style applies to every current and future Cannon in that run;
Catapult is unaffected. Selecting one removes all three style cards from later
offers. Styles reset on a new run and survive suspended-run saves.

| Card | Main damage per shell | Interval | Shells | Main blast radius | Secondary blasts |
| --- | --- | --- | --- | --- | --- |
| Heavy Shell | ×5 | ×3 | 1 | ×1 | None |
| Scattershot | ×0.4 | ×1 | 5 | ×0.55 | None |
| Cluster Shell | ×0.8 | ×1 | 1 | ×1 | 3 |

Scattershot fires the aimed shell plus four shells at different surrounding
impact points, 1.4 world units from the target. Each follows its own ballistic
trajectory. One volley emits one CannonFired event for sound, recoil and quests.

Cluster mini explosions use the existing cluster rules: 52% radius and 45% of
the main shell damage (36% of unmodified damage for Cluster Shell), distributed
around the impact. Existing Cluster Payload cards add up to four further mini
explosions; they still apply to both cannons and catapults. Every launched shell
captures its cluster count, damage, radius and visible size, so later changes
cannot alter shells already in flight. Projectile storage retains its 300-shell
cap; a volley may be shortened when this existing cap is reached.

Styles are single-pick cards with fixed Rare presentation and fixed power,
independent of the rarity multiplier used by ordinary numeric upgrades. The
choice's benefit and drawback are shown together on the card. The stats panel
shows the chosen cannon attack style separately from global defense modifiers.

All ordinary damage, rate, radius, high-ground, anvil and starting-character
bonuses continue to combine with the profile. Visual shell and trail sizes are
×1.7 for Heavy, ×0.65 for Scatter and ×1.15 for Cluster. No new particle system
or graphics dependency is added. Values are initial gameplay tuning.

## Interfaces and compatibility

- `CannonAttackStyle`, `CannonAttackProfile`, `cannonAttackProfile` define profiles.
- `CannonSystem::setAttackStyle` selects a runtime profile.
- `CannonProjectile` includes `visualScale` and `clusterExplosionCount`.
- `Renderer::drawCannonball` accepts an optional scale (defaults to 1).
- `RunUpgradeStats::cannonAttackStyle` exposes the choice to HUD.
- Three upgrade IDs are appended; existing IDs remain unchanged. Saved arrays
  expand with zero defaults for older files. Invalid saves containing several
  chosen styles or inconsistent style power/count are rejected. Stale pending
  specialization choices regenerate after restore.

Resource upgrades, Core progression and other weapon specializations are not
changed by this feature.
