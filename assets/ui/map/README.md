# Tactical map icons

Seven transparent 256×256 PNG icons generated with the built-in imagegen tool.
The full prompt set and original generation paths are recorded in `prompts.json`.
Each silhouette is centered in a 224-pixel area, preserving alpha and aspect ratio.

- chest: discovered treasure / available chest loot
- wood, stone, crystal: original generated studies; the game now reuses the
  exact `assets/ui/resource_*.png` HUD textures for resource clusters
- ruins: procedural scenery sites
- supplies: nearby barrels and crates (distant supplies require the map item)
- challenge: nearby challenge shrines; completed challenges are omitted

`MinimapMarkers` groups resources in stable 24-metre world cells and anchors each
marker on an actual node. Markers refresh with the minimap target; important
markers take priority over decorative icons. Chest discovery rules remain intact.
Icons are drawn before enemies and counter-rotated to stay upright.

`MinimapLandscape` caches a 1024-pixel atlas per terrain seed. Terrain shading,
contours, beaches, shallow water, depth colors and wave strokes are baked once.
Pond outlines, bays and islands use the same signed-distance shape as the world.

## Illustrated landscape

The eight additional sprites (`forest`, `desert`, `snow`, `corrupted`, `mountain`,
`base`, `boss`, `boulders`) come from `art/ui/minimap/illustrated_atlas.png`,
generated with the built-in imagegen tool. The final prompt is saved beside it
in `prompt.txt`. They retain transparent alpha, use 16-pixel padding, and fit a
256×256 canvas. `tools/split_minimap_atlas.swift` reproduces the extraction using
native macOS CoreGraphics, without third-party tooling dependencies.

The landscape uses the world's biome boundaries, actual resource-node clusters,
road curves, and pond shapes. Mountain symbols only indicate raised, steep
terrain; they do not claim additional impassable obstacles. Trees represent
forest cover and remain stable when individual resource nodes are depleted.
Sparse log markers, stone and crystal markers still represent live resources.
All terrain sprites, roads, shading and water are baked into the same 1024×1024
texture when terrain changes; dynamic markers retain the 20/30 Hz target cache.

The compact compass follows the player. Expanded mode smoothly centers on the
world origin and becomes north-up, with a short icon legend on the right.
Discovery and loot rules are unchanged. `SimulationSnapshot::terrainPaths` is a
read-only view of existing terrain paths, with the same lifetime as terrain
samples and ponds; no save or gameplay state was added.
