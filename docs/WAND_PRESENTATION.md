# Held ice and fire wands

Both wands now load distinct solid heads from
`assets/models/weapons/elemental_wands/`. Ice uses a faceted blue crystal
in a three-prong steel crown; fire uses a tapered ember in four gold claws.
The grip and original KayKit weapons palette are retained.

Casting follows the simulation's charge and fired events. It no longer
reuses the generic melee swing. Ice has a short precise thrust; fire has
a stronger winding motion, forward thrust and elastic kick. Release starts
at the charged pose and returns smoothly to rest. The idle ice wand no
longer reports a full charge when charge remaining is zero.

Head accents use one small triangle batch: a faint glow, a highlight,
five converging charge motes and a brief release flare. Disabling particles
removes the charge motes. No persistent emitter, allocation per frame,
extra light or new shader is required. Projectile visuals, damage,
charge duration and cooldown are unchanged.

The existing renderer call signature is unchanged. The only added public
resource accessor is `GraphicsResources::fireWandModel()`; its lifetime
matches `iceWandModel()` and the other held models.

`WandGraphicsChecks.hpp` checks idle charge state, release continuity,
settling and actual first-person mesh-pass visibility and palette through
ten poses for each wand. `AppPresentationEventsSmoke.cpp` checks the real
charge/fired event path. Set `IAN_CAPTURE_WANDS` to a directory when running
`ian_graphics_smoke` to export those frames. Add `IAN_WANDS_ONLY=1` for
a focused run of these checks. Blender sources, references,
reproduction commands and export validation are in
`art/models/wands/handoff.md`.

These are rigid procedural first-person animations, not skeletal clips.
They retain the user's saved viewmodel placement; heavily customized
placement can still move a held weapon off-screen.
