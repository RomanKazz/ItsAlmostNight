# Cartoon healing hearts

World healing pickups now use a rounded, opaque pillow-shaped heart with two
lobes, a clear notch and softly rounded lower tip. Convex front/back faces have
smooth normals, a red-to-pink colour ramp and attached cream/pink highlights.
The model keeps its volume from the side without intersecting spheres, texture
planes, particle halos or per-pickup allocations. Contour and contour normals
are cached in fixed arrays.

The heart world material is 86% unlit with no screen AO or world ink outline.
It writes regular opaque depth and covers ink behind it. Idle hearts face the
camera with a gentle ±0.24-radian sway and a small heartbeat pulse (up to 6%).
Existing spawn bounce and attraction remain. Hearts also shrink when a magnet
pulls them close to the camera, using the same smooth shrink as coins.

Healing amount, collection rules, coin/XP denominations, HUD heart icons and
`Renderer::drawHeart` calling convention are unchanged.

Graphics smoke checks the two-lobe silhouette/notch, daytime/nighttime red
palette, both faces, thickness, highlights and foreground occlusion. Preview:

```sh
IAN_CAPTURE_HEARTS=/tmp/ian-hearts/previews ctest --test-dir build/debug \
  -R '^ian_graphics_smoke$' --output-on-failure
```
