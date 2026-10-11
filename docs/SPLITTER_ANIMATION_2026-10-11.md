# Splitter division presentation — 2026-10-11

- Parent: 0.38 s foot-pivot squash, stretch, recoil and compact collapse.
- Children: 0.72 s asymmetric spring, 0.64 m hop, landing squash and settle.
- Pose uses render sub-tick time and stops while gameplay is paused.
- Removed the three expanding ground rings and additive central orb.
- Replaced the 18 long-lived fragments with seven short, shaded slime droplets (0.56 s).
- Skip the generic splitter corpse so it cannot obscure the division pose.
- Visible, focused, instanced, elite-detail and selection-mask draws share deformation.
- Gameplay child count, spawn timing, health, damage and launch knockback are unchanged.

Public presentation API additions: `SplitterPose`, `splitterPose`,
`enemySplitPose`, `enemySplitSubTickSeconds`; optional sub-tick argument on
`enemyRenderPosition`; optional `bodyStretch`/`bodyRoll` on `Renderer::drawEnemy`
and `enemyPresentationTransform`, with identity defaults; matching per-instance fields.

Validation: Debug and Release builds; graphics smoke tests in both configurations;
continuous positive deformation, child landing reset, rendered model visibility and
individual/instanced image agreement. Full Debug CTest: 17/18 pass, including enemy
split gameplay tests. Existing unrelated `ian_run_menus_smoke` failure remains:
"native window retains upgrade names after the SDF cache warms under scissor".
Visual keyframes inspected using actual Splitter and Splitling assets.

These checks use the integrated working tree, which contains unrelated pre-existing
changes. Only this task's changes are included in the commit.
