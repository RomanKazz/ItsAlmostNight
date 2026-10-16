# Elemental wands

Editable source: `elemental_wands.blend`, collections `ICE_WAND` and
`FIRE_WAND`. Both models are at the runtime origin; fire is hidden initially
so the models do not overlap in the viewport. Unhide the meshes in the
`FIRE_WAND` collection and their render visibility to edit or render it.
The original KayKit grip is retained. Every new head part is a named mesh.

Runtime assets:

- `assets/models/weapons/elemental_wands/ice_wand.glb`: 550 triangles.
- `assets/models/weapons/elemental_wands/fire_wand.glb`: 566 triangles.

Each GLB has one primitive, one material and the original embedded
`weapons_bits_texture.png`, with no external dependencies. The unchanged
palette SHA-256 is recorded in `validation.json`.

Blender uses +Z up, game/glTF uses +Y up. Units are metres. Grip origin,
shaft length and first-person scale match the previous wand. Head effect
anchor is `(0, .35, 0)` in game coordinates. No collision objects or mesh
animations are needed for held weapons: the game drives their rigid casts.

The comparison image shows new ice, new fire, previous wand and club under
the same neutral light. Other views isolate the new pair.

Rebuild in a fresh background Blender:

```sh
blender --factory-startup --background --python-exit-code 1 --python tools/create_wand_models.py
blender --factory-startup --background --python-exit-code 1 --python tools/validate_wand_models.py
```

Validation checks GLB bounds, finite unit normals, UV ranges, original
embedded PNG bytes, single-primitive export, clean reimport, manifold new
parts, positive volume and packed editable images. The inherited grip is
preserved rather than remeshed. KayKit source assets are CC0; see the
existing asset license in `assets/models/tools/LICENSE-KAYKIT.txt`.
