# Building hover UI

The health bar is lifted 0.48 m above the shared upgrade anchor instead of
0.28 m, leaving space above the keycap/price row. Producer collection rows
retain their extra spacing.

The building level is a camera-facing world billboard beside the health bar,
with fixed 0.19 m text height. It no longer uses a separate screen text cache
or clamps font size in pixels. The label follows distance, camera movement
and the building anchor. UTF-8 billboard glyph iteration keeps translated
level labels intact.

Only an explicitly selected building receives a level label. Automatic bars
shown after enemy damage receive no level. App passes its pinned building ID
through the new optional `buildingLevelTarget` argument of `TargetHealthBar::draw`;
the default leaves the label hidden. No save or gameplay data changes.

The stats card uses a single black surface with 40% opacity, rounded corners
matching the hint's visual scale, and no plate border or divider. Table labels
and numbers use the menu's bold body face at slightly larger sizes. Position,
stat values, upgrade comparisons and colour meanings are unchanged.

The interaction UI smoke test captures selected/damage-only/distant building
bars and the stats card. It verifies that damage-only bars contain no level,
world level text scales with distance and the card uses the same black alpha
as interaction hints.

Validation: Debug and Release builds passed. The focused UI smoke passed in
both configurations; the full Debug CTest suite passed (42/42).
