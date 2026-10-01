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
separate foundation and upper-floor tools: foundations validate terrain at
storey zero, while floor platforms require an existing supported frame below.
The one-cell placement step is shared by buildings, platforms, walls and ramps.
Footprints retain their original size: buildings up to 2x2, platforms 2x2 and
ramps 2x4. Occupancy prevents overlaps.
Large buildings snap their centers to the nearest grid intersection, switching
at cell midpoints. Compact buildings retain cell-centered placement. Automatic
foundation previews derive their lower corner from the building footprint. Manual
foundation aim snaps to the nearest anchor at cell midpoints; valid foundation
previews bypass the valid-cell magnet and hysteresis. Dragging uses the same
thresholds. Exact midpoint ties retain the lower anchor.

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
