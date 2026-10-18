# Building placement feedback

Placement now uses a 0.7-second pop/settle animation, low cream dust at the
footprint and four small gold sparkles.
The existing stagger and placement audio for dragged construction are preserved.
Automatic support platforms follow their owner's spring without a duplicate burst.

Rendering uses the existing depth-tested colour-only triangle batch, no new
textures, lights, physics particles or per-frame allocations. Bursts are culled
at 40 metres/behind the camera and capped at 12 visible placements per frame.
Each full-detail burst uses at most 456 vertices (5,472 at the cap); reduced
particle quality uses 228. The accessibility reduce-flashes option removes the
sparkles. Collision, targeting, build costs and save data do not
change. No existing public interfaces change.

Validation: Debug and Release builds; full Debug CTest suite; Release gameplay
and presentation integration tests. The first full Debug run passed 41/42; an
unrelated prepared-viewmodel pixel parity check failed, then passed unchanged
on a focused rerun alongside the presentation test. The real App test checks placement event
routing, support deduplication, animated render frames, delayed starts and exact
rest scale/particle expiry. Optional model captures:
`IAN_BUILD_PLACEMENT_CAPTURE=/tmp/placement its_almost_night --test-presentation-events`.
