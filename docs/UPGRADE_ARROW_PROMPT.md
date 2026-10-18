# Compact upgrade price

Building upgrade interactions again show the original green upgrade-arrow asset,
resource icons and chunky price digits in one world-anchored row. A rounded
black pill behind it uses alpha 102/255 (40% opacity), multiplied only by the
existing appearance fade. There is no extra Upgrade heading or keycap in this
row. Available resource amounts are light; missing amounts remain red. Producer
stock/collection remains available when present. Other interaction cards keep
their existing style.

The restored row follows the previous perspective/world-size calculation and
uses the existing UI coordinate projection. No public interfaces, upgrade logic,
prices or save data change. No new assets are needed.

Validation: Debug/Release builds, full Debug CTest and Release upgrade UI test;
actual captured prompt inspected. Pixel regressions verify the green arrow,
missing/available resource colours, 40% black compositing and compact row height.
Optional captures: `ian_run_upgrade_ui_smoke /tmp/upgrade-prompts`.

Results: full Debug CTest 42/42; Release upgrade UI smoke passed. Both builds
complete without new compiler warnings.
