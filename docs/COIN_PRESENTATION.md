# Cartoon coin presentation

World coin pickups use opaque procedural minted geometry instead of the
previous red-textured imported coin. Each has a smooth cylindrical barrel,
bevelled bright rim, coloured flat face, raised four-point mark and small
attached highlight. Both sides carry the relief. All geometry uses the existing
triangle batch with constant stack data; no particles or new assets are loaded.

Existing denominations and collection logic stay unchanged:

- Bronze / 1 coin: warm honey-gold, scale 1.00.
- Silver / 5 coins: cool silver-blue, scale 1.10.
- HighValue / 10 coins: bright gold, scale 1.20.

Base radius is 0.215 world units. The coin material is 90% unlit, with no world
ink outline or screen AO, and still writes opaque depth and occludes ink behind
it. Hearts use their own red material and idle animation; see
`HEART_PRESENTATION.md`. Ground clearance remains inside the existing pickup
clearance.

Coin idle spin is 3.2 radians/second. Spawn bounce and collection anticipation
remain; magnetized coins shrink smoothly as they approach the camera so they
do not fill the view. XP crystal value tiers are documented separately in
`EXPERIENCE_CRYSTALS.md`.

`Renderer::drawCoin` retains its API. Graphics smoke checks warm/silver palettes,
both faces, value size differences, visible thickness, night readability and
foreground depth occlusion. Preview export:

```sh
IAN_CAPTURE_COINS=/tmp/ian-coins/previews ctest --test-dir build/debug \
  -R '^ian_graphics_smoke$' --output-on-failure
```
