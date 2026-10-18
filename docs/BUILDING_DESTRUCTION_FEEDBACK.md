# Building destruction feedback

Buildings now squash and wobble briefly, shrink into a cream dust burst and
throw short-lived wood/metal shards. Core shards also use gold and purple.
The same debris renderer handles dismantling. Structural collapse retains its
storey delays, falling trajectory and later dust at the terrain impact site.

Combat, volatile explosions and trap wear now retain a lethal target's complete
BuildingInstance before BuildingSystem::damage removes it. This uses the existing
GameEvent::building payload: the visible model keeps its actual level, base,
rotation and platform. Debug core destruction retains its existing core copy.
No existing public interfaces, damage rules, costs or save formats change.

Presentation budgets:

- Up to 12 visible debris bursts, culled behind the camera and beyond 40 metres.
- Full quality: 8 dust puffs + 8 outlined shards, 768 batched vertices per burst;
  reduced quality: half the particles/vertices. No particle physics or lights.
- Up to 24 fading building models inside 40 metres.
- Nearby camera feedback shares the existing explosion cooldown and honours
  reduce-flashes. A collapsing group cannot restart the structure-break sound
  while that clip is already playing.

Validation: Debug, Release and sanitizer builds; Debug and ASan/UBSan CTest;
Release gameplay/presentation/audio tests. Regressions check lethal model capture,
real destruction event routing, retained upgrade/orientation, changing rendered
frames, delayed structural-collapse dust and particle expiry. Actual model/effect
captures were inspected at six points in the animation. To reproduce captures:
`IAN_DESTRUCTION_CAPTURE=/tmp/destruction its_almost_night --test-presentation-events`.

Results: ASan/UBSan 42/42; Release gameplay/presentation/audio 3/3. Debug
completed 41/42 initially (the general graphics resource test reached its
90-second timeout under parallel load); that test then passed unchanged in an
isolated 84.17-second rerun. No sanitizer diagnostics or new compiler warnings.
