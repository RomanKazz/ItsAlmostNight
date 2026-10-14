# Shop icons

Ten separately generated RGBA icons, using the built-in `image_gen` tool.
Style reference: `assets/ui/building_turret.png` — chunky cel-shaded objects,
dark contour, warm orange/gold hardware, blue-gray metal, transparent padding.

Nine permanent upgrades use their stable `MetaShopDefinition::id` as filename.
`purchase.png` is used by Buy buttons in both shops; `rerolls.png` is shared
by the permanent Rerolls upgrade and the run shop's refresh button.
Run shop offer cards use the existing generated tome/building icons.

All icons are 256×256 with artwork fitted into a centered 216×216 area.
Original alpha is preserved; preparation only crops transparent padding and
resizes. `tools/prepare_shop_icons.py` reproduces runtime assets and the
256px/48px contact sheet. Pillow is the same authoring dependency used for tomes.

Exact prompts, source PNG paths and runtime mapping: `art/ui/shop/prompts.json`.
Visual comparison: `art/ui/shop/contact-sheet.png`.
`GameUi` loads once, uses bilinear filtering and unloads at shutdown.
`ShopMenuSmokeTests` validates every icon plus purchase/refresh actions and renders
the actual shop in English/Russian at eight viewport sizes.
