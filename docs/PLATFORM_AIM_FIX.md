# Platform aiming and preview continuity

The combined platform tool now picks the nearest continuous deck plane along
its aim ray. Decorative mesh gaps and stale broad-phase candidates no longer
switch it to terrain, a lower storey, or another grid cell. Terrain in front of
a deck still occludes it; ramps retain their existing attachment handling.

Only exposed perimeter edges extend the current storey. Overlap with an adjacent
frame suppresses that edge socket, so an interior joint selects an upper floor
instead of proposing a piece inside the existing neighbour. One-cell movement
across a fully supported surface is retained. At a partial unsupported overhang,
upper-floor placement falls back to the complete supporting frame footprint.

If a platform target is blocked, a bounded search of twenty nearby anchors
(within 2.5 cells) finds the closest valid footprint at the same height/storey.
A small retention bias prevents toggling at equal-distance boundaries. The
selected anchor also drives placement/drag input, not only the rendered ghost.
Resource costs, support and range validation remain mandatory. If no nearby
candidate passes validation, the invalid hint remains. Dragged rows keep their
explicit endpoints and do not magnetize individual pieces.

Occupied platform footprints now honour the interpolated visual origin, as
normal previews do, instead of teleporting to the discrete target cell.
Placement validation, support requirements, construction cost and drag height
locking are unchanged. No public interfaces or save formats change.

Validation: Debug/Release builds and Debug CTest suite; Release gameplay and
presentation integration tests. Regression coverage uses real adjacent decks
and eleven aim samples around cell/frame boundaries, including a deliberately
incorrect lower-storey candidate. It also checks that exposed edges still extend
the current storey and that occupied footprint pixels move continuously with
the supplied visual origin. Additional regressions cover the half-overlapping
ground edge and an occupied upper floor magnetizing to a neighbouring valid
footprint; actual placement is committed and checked against that ghost.
Existing contextual platform/drag coverage remains.

## Post-placement preview

Successful platform/floor placement now suppresses automatic stacking at the
unchanged aim. The guard is anchored to the newly placed deck plane after the
player has been raised onto it, and lasts until the aim moves more than 0.65
cells, leaves that plane, or the platform tool is explicitly reselected.
Horizontal edge extension remains available immediately. This also applies to
a dragged row and automatic foundation+floor placement; failed placements and
ramps do not arm it. Run/tool reset clears this private presentation state.

Regression coverage commits an actual floor and checks unchanged aim, repeated
small mouse movements, retargeting, explicit reselection and unsuccessful
commit handling. No gameplay/public interfaces or save formats change.

Validation for this follow-up: Debug/Release builds pass; the App construction
presentation regression and Release gameplay suite pass. Full Debug is 41/42:
`ian_render_performance_smoke` reports prepared-viewmodel pixel mismatches and
also fails an isolated rerun. That renderer-only target does not use the
modified App placement code. The failure is recorded rather than weakening
its pixel comparison or changing unrelated viewmodel rendering.
