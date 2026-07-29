# Graphics Phase 7 — AO and Ground Contact

Date: 2026-07-26

## Implemented

- Adjustable AO strength in the shared world shader.
- Temporary per-material AO coefficient for graybox geometry.
- Optional vertex-alpha AO source for future GLB models.
- Soft blob-shadow batch for resources, buildings, and enemies.
- Object-specific blob size and opacity.
- Camera-distance fade.
- Graphics-quality-dependent segment count and opacity.
- Existing `F7` blob-shadow toggle is now functional.

Full-screen SSAO was intentionally not implemented.

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

Resources, buildings, and all active enemy types contribute blobs. Distant
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
- `F9`: reserved SSAO toggle; SSAO remains unimplemented.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.
- Source whitespace check: passed.

The application was not launched and no screenshots were taken. No standalone
GLSL validator is installed. Driver-side shader compilation, contact-shadow
tuning, and visual comparison remain owner-run integration work.

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

## Known limitations

- Graybox primitives have no authored vertex AO; they use material coefficients.
- Blob ellipses follow world axes rather than individual model orientation.
- Terrain intersection height assumes the current flat ground plane.
- AO textures and packed material channels can be added when production GLB
  materials arrive.
