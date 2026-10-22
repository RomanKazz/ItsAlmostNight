# KayKit world scenery

Ten models from **KayKit Medieval Hexagon Pack 1.0 FREE**, distributed under
CC0. The original license is retained in `LICENSE-KAYKIT.txt`; original glTF
paths and baked dimensions are listed in `source_manifest.json`.

The GLBs embed their textures. Positions were uniformly scaled to meters,
centered in X/Z, and grounded at Y=0. Their dimensions are shared by placement,
collision and rendering in `src/world/WorldScenery.hpp`. Keep that table in
sync when replacing a model. `tent.glb` is an open canopy, with movement
collisions on its four posts rather than across its entire footprint.

`src/world/WorldScenery.cpp` assembles four open compositions: ruined yards,
lumber camps, stone outposts and well courtyards. Placement uses a separate,
deterministic terrain-seed stream, checks dry flat ground, and reserves room
around resources, loot, landmarks and challenge arenas. Up to twelve sites
are attempted; unsuitable locations are skipped rather than forced onto a
slope or into another object.

These are static scenery, separate from buildable/repairable production
structures. Rendering batches repeated models through GPU instancing. Both
player collision and ground-enemy navigation account for the solid pieces.
`WorldSceneryLoot.cpp` populates accepted sites with a 45% chest roll and
three independent 55% destructible-prop slots. Failed placement is skipped.
It relocates existing exploration chests and interactive barrels/crates,
preserving the world loot budget, object IDs, rewards and respawn rules.
The templates contain no guaranteed decorative crates.
Continue derives the same layout from the terrain seed and removes pieces
that conflict with the saved player, structures or relocated resources;
the save format remains compatible with existing runs.
