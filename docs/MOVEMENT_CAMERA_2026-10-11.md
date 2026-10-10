# Movement and camera follow-up — 2026-10-11

Base movement speed is now 6.5 m/s, up 8.33% from 6.0 m/s (+30% from the
original 5.0 m/s). Defaults and runtime gameplay JSON use the same value.
Movement tests now verify 6.5 m/s, diagonal normalization, Shift equivalence,
movement tome scaling and preserved dash.

The previous sprint removal missed presentation behavior. Holding Shift no
longer widens camera FOV, increases camera bob amplitude or changes bob cadence.
Normal target FOV stays 75 degrees; the existing dash target remains 88 degrees
with smooth transitions. Camera bob follows actual movement speed.

Public interfaces and saved binding IDs are unchanged. Shift continues to
control sandbox flight descent. No memory or lifetime changes were made.

Changed files: `assets/data/gameplay.json`, `src/game/GameBalance.cpp`,
`src/app/AppUpdate.cpp`, `src/app/AppGameplayRender.cpp`,
`tests/GameBalanceTests.cpp`, `tests/SimulationTests.cpp`, this report.

Validation:

- Debug and Release configure/build completed without compiler warnings.
- Release unit, graphics and settings smoke tests: **3/3 passed**.
- Full Debug suite: **16/17 passed**. The pre-existing `ian_run_menus_smoke`
  text/scissor failure remains: `native window retains upgrade names after the
  SDF cache warms under scissor`. Movement, unit and graphics checks passed.
- Source audit found no remaining sprint references in camera update/render;
  the movement reference is solely sandbox flight descent.
- `git diff --check` passed.

Visual feel of the faster movement should still be assessed in a real run;
this task has no interactive camera playtest.
