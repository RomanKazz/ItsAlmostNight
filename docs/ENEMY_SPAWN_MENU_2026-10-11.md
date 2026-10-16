# Enemy spawn menu — 2026-10-11

B opens the existing debug spawner with the shared `menuStyle` panels, display
font, outlined labels, dark forest palette and gold selection state.

- Nine directly selectable enemy cards with the same icons as the night forecast.
- Normal, Berserker, Warden and Volatile buttons with distinct accent colors.
- Count presets: 1, 10, 50, 100, 500, 1000; slider retains the existing 2048 limit.
- Minus/plus adjust by one; Shift adjusts by ten; wheel works over the count.
- Enter summons; B/Esc and the close button close. Repeated summons keep the menu open.
- Existing spawn command and cursor/input suspension flow are preserved.
- English/Russian labels, descriptions and hints; adaptive landscape/portrait layout.
- Full enemy capacity disables the summon action. No model targets or new textures.

Public UI API: `EnemySpawnMenuInput`, `EnemySpawnMenuResult`,
`drawEnemySpawnCatalog(const GameUi&, ...)`, and layout/portrait helpers in `enemy_spawn_menu`.
The catalog and selected-enemy preview reuse cached `GameUi::enemyTexture` portraits
from the night forecast, preserving their aspect ratio. Splitlings share the splitter icon.
App integration retains the existing debug spawn fields and pending command.

Validation: Debug/Release builds; full Debug CTest 18/18 pass. Temporary UI harness
checked all nine selections, four variants, preset/step/slider controls, Enter,
close, capacity blocking and repeated summons. Actual renders inspected in English
and Russian at 1920x1200, 1280x720, 800x600 and 900x1200. Captures stay outside Git.
Only the new menu and its localization entries are included in the commit;
unrelated pre-existing working changes remain unstaged.

2026-10-16 icon update: Debug and Release builds; full Debug CTest 32/32 pass.
Catalog renders inspected at
1920×1080 and 900×1200. Icons are cached, centered, and drawn without stretching.
