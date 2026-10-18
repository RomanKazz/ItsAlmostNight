# Terrain and modular building integration

## Existing systems reused

- `MapDefinition` remains the source for map bounds, spawns, resources and
  static obstacles.
- `Simulation` remains the authoritative fixed-step gameplay state.
- `CollisionWorld` remains the XZ broad collision layer and will receive
  modular gameplay colliders in the collision phase.
- `BuildingSystem` remains active for legacy defensive buildings while the
  modular grid is introduced incrementally.
- `Renderer` keeps the shared world shader, terrain texture, lighting and
  shadow pipeline.

## Added architecture

- `WorldConfig`: one source for terrain and modular-grid dimensions.
- `TerrainHeightfield`: deterministic height data, interpolation, normals and
  bounds; authoritative for terrain walking and future placement.
- `TerrainRenderer`: GPU mesh generated from the heightfield without gameplay
  rules.
- `BuildGrid`: 3D discrete coordinates and layered occupancy with stable
  entity ownership.

The simulation floor entity is `PlatformFrame`. It always occupies a 2x2
block; its anchor advances on the one-cell placement grid, including odd and
negative coordinates. Placement exposes
one contextual platform tool: terrain aim places a foundation at storey zero;
aiming at a platform center proposes the next floor, while its edge extends the
current storey. Floors still require an existing supported frame below.
Defenses, producers and construction tools share one hotbar, with a gap before
platforms and ramps. C only cycles equipment, never construction categories.
The one-cell placement step is shared by buildings, platforms, walls and ramps.
Footprints retain their original size: buildings up to 2x2, platforms 2x2 and
ramps 2x4. Occupancy prevents overlaps.
Large buildings snap their centers to the nearest grid intersection, switching
at cell midpoints. Compact buildings retain cell-centered placement. Automatic
foundation previews derive their lower corner from the building footprint. Manual
foundation aim snaps to the nearest anchor at cell midpoints; valid foundation
previews bypass the valid-cell magnet and hysteresis. Dragging uses the same
thresholds. Exact midpoint ties retain the lower anchor. The standalone tool
converts camera hits at the frame center to lower-corner targets before preview
and drag snapping, matching the center used by large buildings.

Each frame owns its floor, four terrain- or floor-reaching supports and
perimeter beams. Frames above ground depend directly on the frame below.
The single storey height is `ModularStoreyHeightCells == 4`; its world-space
height is always derived from `4 * cellSize`.

## Integration order

1. Validate and place a ground `PlatformFrame` from terrain corner samples.
2. Stack another frame at exactly four cells above the previous frame.
3. Generate the floor, four supports and perimeter beams as one instance.
4. Reuse shared corner supports through reference counts.
5. Attach walls and four-cell stairs to existing frames.
6. Synchronize floor/ramp/wall colliders and structural dependencies.
   Ramps have a fixed 2x4-cell footprint and connect floors exactly
   one four-cell storey apart.
7. Recalculate or collapse dependants after safe removal.
8. Add serialization DTOs after the runtime representation stabilizes.

Legacy defensive buildings are not removed during these phases.

Fence tops use thin walkable segments matching their side colliders (0.5m above
the building base), including junctions and elevated fences. Player support and
swept landing queries use these segments; jumps can land on and move along them.

## Compact construction radius (2026-10-18)

New runs start with a 4 m horizontal construction radius around the Core.
Each successful Core upgrade adds 1 m: levels 1–8 provide 4/5/6/7/8/9/10/11 m.
The previous values were 9 m initially and +3 m per level (30 m at level 8).
The graybox asset and fallback map use the same initial radius. Structures and
foundation placement share the current radius; their existing preview boundary
updates from the same value. The boundary also appears when aiming at the Core
while holding equipment; aiming at another building does not reveal it.
Height is not included in this horizontal limit.
Godmode and sandbox retain their existing bypasses. Suspended runs preserve
already-saved radius values; subsequent Core levels use the new +1 m growth.
Public interfaces and save formats are unchanged.

Validation: Debug test suite checked, with its simulation test rerun after
isolating fixed-layout fixtures from radius balance. Release simulation tests
and Debug/Release Core-hover rendering tests passed. The four-meter hover
boundary was also inspected in an offscreen capture. Diff whitespace checks pass.
