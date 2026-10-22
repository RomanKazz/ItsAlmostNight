# Main menu source artwork

Source: Figma file Cf7YU7vDqFoeyjEQdUt3og, Frame 19, node 695:2.
Downloaded through the connected Figma plugin on 2026-10-04.

All source SVGs and icons remain unmodified. Polygon PNGs are transparent
2x rasterizations of those original SVGs using resvg-js 2.6.2, preserving
their fill filters. The original stroke path is composited above the
filtered group so the inner highlight cannot lighten the black outline.
The Figma PNG download endpoint included an opaque white
canvas; lunasvg does not support the SVG inner shadow filters. Focus-corner
SVGs are rasterized once at initialization. Layout includes stroke/shadow
overflow and scales uniformly from 1920x1080.

To regenerate polygon PNGs (optional; no dependency is needed to build/run):

```sh
npm install --prefix /tmp/ian-svg-rasterizer --no-audit --no-fund @resvg/resvg-js@2.6.2
NODE_PATH=/tmp/ian-svg-rasterizer/node_modules node tools/rasterize_main_menu.cjs
```

- imgPolygon2/3/4/5: Play / Unlocks / Quests / Shop.
- imgPolygon7/8/9: Settings / Credits / Exit.
- *_selected: yellow button variants using Play's original fill color;
  imgPolygon2_inactive: grey Play. They retain each button's source geometry.
- imgPolygon12: quest card background.
- imgPolygon10/11/13/14/15: records frame / header / active tab / inactive tab / body.
- imgUnion/1/2/3: Play focus corners (rotations 0 / 180 / -90 / 90).
- imgSilver2561, imgSpikes3, imgWrench2562, imgGear2562: source icons.
- imgImage7 is byte-identical to ../logo.png, so the existing logo is reused.
- imgVector is the sample leaderboard's striped table reference. Rows are
  drawn in code with alternating fills and a gold player highlight. GLOBAL
  and FRIENDS currently show eight rows from separate design fixtures defined in
  MainMenuHome.cpp; they are placeholders for future Steam leaderboard data.
  This reference asset is not rendered, and local profile records are unchanged.

The home layout follows the updated screenshot: one featured quest with a
live completion summary and View All, a large central Play button, a horizontal
Unlocks / Quests / Shop row, and transparent Settings / Exit footer actions
using the user-provided SVG icons in `../icons`. Credits is absent from home.
Quest and leaderboard panels stay anchored to opposite screen edges. Plates
are now drawn as chamfered geometry with a uniform 5-unit black stroke,
18-unit corner cuts and a 3-unit inner highlight; resizing changes their
straight spans without stretching borders or corners. Raster plates remain
as source references; the logo, quest icons and focus corners are still used.
Choose Background is available in Settings / Preset. When a suspended run
exists, the full-height Continue button appears above Play and shifts the
main button stack down by one row.

Text is dynamic, rendered with CIKO and Dico Sans Soft ExtraBold. Dico was
copied from the user's installed font; Russian body text uses the existing
Cyrillic fallback. The design's white canvas is transparent in the game:
the user's saved 3D background remains visible without depth-of-field blur.
