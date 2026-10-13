# Shrine assets

Two original altars built in Blender using the game's existing defense colormap. Player: purple reactor and heart. Buildings: blue reactor, gold gear and blue gem. Packed editable source: `shrines.blend`.

`manifest.json` records pivots, scale, palette and collision contract. `validation.json` records closed topology, outward normals, unit GLB normals, floor zero, palette identity and emblem clearance. `family-comparison.png` shows runtime proportions beside the original core and flamethrower; orthographic previews support future edits. Do not package `.blend1` authoring backups.

The game loads both GLBs directly, normalizes height to 2.8 m, applies its world shader, uses visible meshes for hover and a centered 2×2×2.8 m blocking box for movement. No attack sockets/animations are needed for these static interaction props. Used shrines keep their geometry and receive a dim tint.

Creation and validation ran through Blender MCP. The render script also supports background Blender. Automated game-loader/render/raycast checks passed; native manual gameplay inspection has not been claimed. Gameplay details: `docs/SHRINES.md`.
