# Compact upgrade price

Building upgrade interactions again show the original green upgrade-arrow asset,
resource icons and chunky price digits in one world-anchored row. A rounded
black pill behind it uses alpha 102/255 (40% opacity), multiplied only by the
existing appearance fade. A separate rounded-square keycap to the left shows the
upgrade binding (U by default), with the hotbar's peach face and brown frame and
lower edge. Its rounded square shape follows the supplied keycap reference.
The dark letter has no outline or shadow. There is no extra Upgrade heading.
Available resource amounts are light; missing amounts remain red. Producer
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

World anchoring follow-up: the price pill is no longer clamped into the screen.
Its cached anchor is rechecked against the current camera before perspective
sizing, so a fast turn cannot project a fading target through the camera plane.
The row exits the viewport with its building. Perspective render regressions
cover immediate 70–180 degree turns, both sides of the 90-degree plane and
partial clipping at a screen edge.

Follow-up validation: Debug CTest 42/42; Release upgrade UI smoke passed;
Debug and Release builds passed. The clipped-edge capture was inspected.

The upgrade keycap shares the price row's world scale, fade and camera culling.
It respects rebound upgrade keys and depresses the face while held. The complete
keycap, gap and price pill are centred together on the world anchor. Other
interaction cards are unchanged.

Keycap validation: Debug/Release builds and upgrade UI smoke passed. The full
Debug suite passed the other 41 checks; the upgrade UI regression was rerun
successfully after fixing single-letter key widths. Captures were inspected.
Pixel checks cover the hotbar colours, complete-group centring and rebinding;
the fast-turn and offscreen-clipping regressions remain in place. Public
interfaces, gameplay and save data are unchanged.
