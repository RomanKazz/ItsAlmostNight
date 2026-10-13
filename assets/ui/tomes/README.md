# Tome icons

Twenty-two active stat tomes, each generated separately with the built-in
`image_gen` tool. The defense icons (`building_turret.png` and
`building_flamethrower.png`) supplied the visual style: chunky volume, broad
cel shading, a dark outline, warm gold hardware and clean transparent padding.
The first Fortitude book served as the reference for the remaining variants;
only cover color and the central emblem change.

All runtime files are 256x256 RGBA PNGs, centered in a 216x216 content area.
Generated alpha is preserved. Rarity, slot frames and glow belong to the UI.
`GameUi::runUpgradeTexture` supplies these icons to HUD tome slots, level-up
cards, shop offers and debug/unlock catalogs. Textures load once at UI startup
and use the existing shutdown ownership. Retired tomes keep the existing
procedural fallback for old/debug data.

Exact prompts and original generated-image paths:
`art/ui/tomes/prompts.json`. The editable reference bitmap is
`art/ui/tomes/fortitude-source.png`; the size comparison is
`art/ui/tomes/contact-sheet.png` (256px and 48px).
`tools/prepare_tome_icons.py` reproduces the transparent-padding crop and
aspect-preserving resize from the saved source paths. It requires Pillow.

Gameplay effects, tome colors/IDs, slot limits and saved runs are unchanged.
