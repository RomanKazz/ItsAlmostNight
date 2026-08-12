# Graphics Phase 7 — AO and Ground Contact

Date: 2026-07-26

## Implemented

- Adjustable AO strength in the shared world shader.
- Temporary per-material AO coefficient for graybox geometry.
- Optional vertex-alpha AO source for future GLB models.
- Half-resolution screen-space AO for opaque large objects.
- Depth/normal material mask and bilateral AO upscale.
- Soft blob-shadow batch for resources, buildings, and enemies.
- Object-specific blob size and opacity.
- Camera-distance fade.
- Graphics-quality-dependent segment count and opacity.
- Existing `F7` blob-shadow toggle is now functional.

## AO path

`WorldMaterialState` supplies:

- `bakedAo`: temporary material AO coefficient;
- `vertexAoAmount`: blend weight for vertex-alpha AO.

The shader combines enabled AO sources, then applies them to lighting through
global `aoStrength`. Default strength is `0.30`. Runtime levels are:

- off;
- 20%;
- 30%;
- 35%.

This keeps AO restrained and adjustable. Future opaque GLB assets may store AO
in vertex alpha without changing the world shader. When vertex alpha is used
for AO, it is removed from output opacity.

Screen-space AO uses a sampleable scene depth texture plus octahedrally packed
world normals. It runs at half scene resolution (quarter resolution on Low),
uses 4/8/12 samples by quality, then receives a five-tap bilateral upscale in
the post-process pass. High quality fades between 30 and 50 metres; Medium
between 26 and 42 metres. Low preset disables SSAO.

`screenAoAmount` classifies materials. Terrain and large opaque props
participate. Grass, water, distant boundary trees, pond plants, small flowers,
and decorative pebbles do not. AO tint is a restrained cool green-grey rather
than black.

Current graybox coefficients:

- terrain: `0.90`;
- map obstacles: `0.74`;
- resources: `0.78`;
- buildings: `0.72`;
- enemies: `0.82`.

## Blob shadows

Blob shadows render after opaque world geometry in one rlgl triangle batch.
Each shadow is a low-segment ellipse with:

- dark center;
- zero-alpha perimeter;
- radius derived from object footprint;
- per-object opacity;
- smooth camera-distance fade;
- disabled depth writes.

Resources, loot chests, buildings, bushes, large flowers, and all active enemy
types contribute blobs. Distant
objects therefore retain cheap ground contact when outside useful shadow-map
coverage.

Quality affects cost:

- Low: 12 segments, reduced opacity;
- Medium: 18 segments;
- High: 24 segments.

The batch may flush internally if raylib's vertex capacity is reached, but it
does not issue one high-level draw call per object.

## Runtime controls

- `F7`: enable or disable blob shadows.
- `Shift+F7`: cycle AO strength through 0/20/30/35%.
- `F2`: inspect blob state and current AO strength.
- `F9`: enable or disable screen-space AO.

## Automated verification

- Release build: passed.
- Unit tests: passed.
- Graphics-resource smoke test: passed with a real OpenGL context.
- Driver-side compilation passed for world, grass, sky, water, cloud,
  post-process, and SSAO shaders.
- Source whitespace check: passed.

The application was launched successfully. Contact shadows were visually
confirmed in-game. SSAO intensity and distance tuning remain visual integration
work if the art direction changes.

## Manual integration checklist

1. Toggle `F7`; confirm soft contact shadows appear under resources, buildings,
   and enemies.
2. Inspect blob edges; confirm a smooth fade without hard circles.
3. Walk away; confirm blobs fade before becoming distracting.
4. Cycle `F10`; confirm lower quality uses cheaper, still acceptable blobs.
5. Cycle `Shift+F7`; confirm AO changes remain subtle and objects stay readable.
6. Inspect Night; confirm AO and blobs do not crush enemy silhouettes.
7. Disable directional shadows with `F3`; confirm blobs still provide contact.
8. Spawn many enemies; confirm blob rendering remains stable.
9. Toggle `F9`; confirm AO appears around large object/ground intersections,
   fades by 50 metres, and does not add speckled shading to grass.

## Known limitations

- Graybox primitives have no authored vertex AO; they use material coefficients.
- Blob ellipses follow world axes rather than individual model orientation.
- Terrain intersection height assumes the current flat ground plane.
- AO textures and packed material channels can supplement current vertex and
  scalar AO when more production GLB materials arrive.
