# Skill icon parser

`ian_skill_icon_parser` converts an AI-generated `1254x1254` JPEG or PNG
sheet into nine game-ready skill icons.

The input is interpreted as an exact `3x3` grid of `418x418` cells. For each
cell the tool removes the black background, preserves a soft alpha edge,
finds the visible silhouette, normalizes its perceived size, optically
centers it, and exports a transparent `256x256` PNG.

Build and import the first gathering sheet:

```sh
cmake --build --preset debug --target ian_skill_icon_parser
./build/debug/ian_skill_icon_parser \
  "/path/to/generated-sheet.jpg"
```

The default output directory is `assets/ui/skill_icons`, and the default
names, from left to right and top to bottom, are:

```text
bare_hands axe pickaxe
efficient_strikes power_swing lumber_mill
quarry crystal_mine night_shift
```

For later sheets, pass an output directory and exactly nine names:

```sh
./build/debug/ian_skill_icon_parser \
  "/path/to/second-sheet.jpg" \
  assets/ui/skill_icons \
  icon_1 icon_2 icon_3 icon_4 icon_5 icon_6 icon_7 icon_8 icon_9
```

The program deliberately rejects images that are not exactly `1254x1254` so
an unexpected generator resize cannot silently misalign the grid.
