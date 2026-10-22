# Item icons

Fifteen item icons generated separately with the built-in imagegen tool using
the shared Fortbonk upgrade-icon style prompt. The exact prompts and source
images are recorded in `prompts.json`.

Final assets are transparent 256x256 RGBA PNGs, with the original aspect ratio
preserved and silhouettes centered within a 216x216 content area. They use
chunky cel-shaded shapes, broad highlights, minimal details and a target black
outline weight of about 6 pixels at the exported size. No rarity backgrounds
or glows are baked into the images.

Apple, bread, iron bar, fuel jerrycan, compass, nail, treasure key, treasure map,
anvil, saw, battle potion, blueprint, hourglass, safety rope and field medkit
have their own artwork. Razor Charm, Wind Feather, Heartwood Seed and Pickup
Magnet reuse the existing matching upgrade assets.

`GameUi::lootTexture` provides the same artwork for the unlock catalog and HUD
item inventory, including the expanded inventory. `GameUi` owns and unloads
the fifteen additional textures; reused upgrade textures retain their original
ownership.
