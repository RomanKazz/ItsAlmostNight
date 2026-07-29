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

The public floor module is `PlatformFrame`. It always occupies a 2x2 block
whose anchor is aligned to the even frame grid. The one-cell unit remains an
internal `BuildGrid` detail for walls, stairs and occupancy calculations.

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
