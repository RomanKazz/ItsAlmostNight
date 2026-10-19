# Compact world interaction hints

Building upgrade interactions again show the original green upgrade-arrow asset,
resource icons and chunky price digits in one world-anchored row. A rounded
black pill behind it uses alpha 102/255 (40% opacity), multiplied only by the
existing appearance fade. A separate rounded-square keycap to the left shows the
upgrade binding (U by default), with a warm cream face and
deep chocolate frame and raised base. Its strongly rounded square shape and
larger letter follow the user's close-up reference. The key is slightly taller
than the price pill, separated by a small gap (0.12 of the arrow's height).
There is no extra inner rim. The label is centred using its visible glyph bounds
instead of the font's line box, including rebound keys.
The complete row is lifted by 0.08 world-scaled metres. When held, the entire
upper cap (outline, face and label) moves down over a fixed lower base.
The dark letter has no outline or shadow. There is no extra Upgrade heading.
Available resource amounts are light; missing amounts remain red. Producer
stock/collection remains available when present. General interaction cards keep
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
Pixel checks cover the keycap colours, complete-group centring and rebinding;
the fast-turn and offscreen-clipping regressions remain in place. Public
interfaces, gameplay and save data are unchanged.

Crates, barrels and item crates now share the raised keycap and 40% black pill.
The pill contains only `Open` (localized), with the actual interact key to its
left. Its low world anchor is at 65% of the model's transformed bounds height, including
terrain alignment, yaw and scale. It follows perspective and leaves the screen
with its object instead of attaching below the crosshair or clamping to edges.
The cached prop prompt is culled against the current camera during fade-out.
Paid loot chests use the same world pill with `Open`, a coin icon and just the
opening cost. The price is light at or above the cost, red below it; the player's
current balance is never rendered here. The coin is optically lifted by 0.05
arrow heights (about two pixels at typical prompt scale). Free-key and godmode pricing still use
the actual opening cost. The chest hint sits 0.22 m above the transformed lid
bounds; crates and barrels retain their lower anchor.
Render checks compare exact funds with surplus funds (identical images), check
the red insufficient-funds price and coin icon. Render checks cover world-anchor
movement, fast turns, rebinding, fade geometry and both languages.

Container interaction reach is 1.5 times resource gathering reach (3.9 m with
the default 2.6 m reach). Client aim and authoritative interaction validation
agree; ID overrides remain range-checked. Container selection and physical
colliders grow by 15%, without changing model scale or harvesting distances.
Resource raycast, collision-edge and actual E interaction tests cover this.

Final validation: Debug and Release builds passed; full Debug CTest 42/42;
latest Debug/Release interaction UI tests passed after the coin alignment tweak.
Captures for affordable/unaffordable chests and moving prop anchors were
inspected. An earlier parallel graphics run hit its 90-second timeout; the
isolated graphics rerun passed, followed by the complete successful suite.
No public interface or save format changes.

Challenge columns use the same world-scaled keycap and 40% black pill, with only
`Challenge` in the pill. They retain their column anchor and interact binding;
there is no Begin Trial heading. World movement and fast-turn render checks
also cover challenges.
Challenge validation: Debug CTest 42/42; Release interaction UI smoke passed;
English and Russian captures inspected. Debug/Release builds passed.
