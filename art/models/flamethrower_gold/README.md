# Gold flamethrower accents

The runtime base model and all seven cumulative upgrade overlays now use the
core's original gold palette swatch: Blender UV U=.65625, V=.285–.465. Bright
steel trims, straps, supports, caps and reinforcement rings are gold. Dark
metal working parts retain their contrast.

`recolor.json` records before/after hashes and affected UV vertices. Only float
TEXCOORD_0 bytes changed: vertex positions/normals, indices, materials, embedded
PNG, rig/collider/socket nodes, pivots, mesh order and file lengths are identical.
The original colormap SHA256 remains
`cbd2f95976ed3f18070769ce32a819527d9bc00d74253844e3882f747b5d76b6`.
Runtime dimensions and all movement/collision contracts match the existing
`art/models/flamethrower_upgrades/manifest.json`.

`flamethrower_gold.blend` is the current packed editable source, imported and
checked through Blender MCP in an isolated scene. Original base/yaw/pitch
rig and all level overlays are retained. `level_1.png`–`level_8.png` and
front/side/back/top views show the result. `comparison.png` compares the
previous silver base with gold level 1 and gold level 8 under identical lighting.

Apply the idempotent UV mapping:

```sh
python3 tools/recolor_flamethrower_accents.py
```

Render/open the editable source as an ordinary Blender project (default family
reference is the cannon; `-- --reference /path/to/old.glb` selects another):

```sh
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background --python-exit-code 1 --python tools/render_flamethrower_gold.py
```

The overlay generator also creates gold accents on subsequent rebuilds.
Acceptance: original PNG embedded byte-for-byte, no residual bright-silver UVs,
all non-UV geometry/rig data byte-identical; game loader/render/muzzle/raycast
and existing flamethrower attack tests passed. `validation.json` records this;
native manual combat playtest is not claimed.
