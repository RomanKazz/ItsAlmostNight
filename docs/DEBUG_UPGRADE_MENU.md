# Debug upgrade catalog

Shift+F5 opens the run upgrade catalog. F5 retains its existing loot-item behavior
outside sandbox mode. Closing uses Escape, the close button, or Shift+F5 again.

- Search names and descriptions in English or the current language.
- Filter by player, defense, economy, unlocks, or utility.
- Click cards to select several different upgrades. Filtering preserves selections.
- Choose rarity and enter copies per upgrade; 1/10/100/1000 are shortcuts, not caps.
- Grant Selected (or Enter outside search) issues the selection and keeps the menu open.
- Repeatable upgrades bypass normal offer, level, and power caps.
- Blueprint grants bypass the four-defense arsenal limit.
- Unlocks remain unique. Selecting another cannon specialization replaces the old one.
- Retired upgrades remain excluded. Numeric overflow is rejected.

## Integration

`Simulation::grantDebugRunUpgradeBatch(effect, count, rarity)` aggregates copies
without a per-copy loop and refreshes runtime effects once per selected effect.
The existing `grantDebugRunUpgrade` and normal card selection retain their caps.
Save validation accepts the enlarged debug stack/power values without changing IDs
or the save layout.

`DebugUpgradeMenu.hpp` owns layout, filtering, input state, and rendering using the
shared chamfered menu plates, fonts, palette, and existing upgrade icons. App input,
render, and update connect it to the simulation and play one confirmation per batch.

Touched integration files: `App.hpp`, `AppInput.cpp`, `AppItemGrantMenu.cpp`,
`AppUpdate.cpp`, `Simulation.hpp`, `SimulationProgression.cpp`,
`SimulationSuspendedRun.cpp`, and `assets/data/localization.json`.

Tests in `RunUpgradeTests.cpp` cover bulk rarity scaling, repeated grants, all six
defense blueprints, unique unlocks, cannon style replacement, invalid inputs,
save/load, reset, and unchanged ordinary limits. `RunMenusSmokeTests.cpp` covers
selection, count entry, rarity, English/Russian search, UTF-8 boundaries, scrolling,
clipped hit testing, repeated grants, and landscape/portrait layout. It can export
previews when given an output directory.

The catalog disables SDF texture caching only while drawing clipped card text.
`ScopedSdfUiTextCache` in `UiText.hpp/.cpp` restores the previous cache policy after
that scope; other text keeps its existing caching. The native-window smoke test
checks actual name pixels after four frames, since render-texture previews skip
cache creation and would miss this regression.
