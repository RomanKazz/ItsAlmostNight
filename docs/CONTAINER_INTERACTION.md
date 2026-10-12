# Breakable world containers

Barrels, crates and item crates open immediately with the rebound Interact key
(E by default), regardless of equipped weapon. They also break on one positive
tool hit. Trees, rocks and resource crystals retain progressive harvesting.
Containers show a localized “Break open” prompt instead of a health bar.
Paid loot chests keep their existing opening interaction and cost.

Simulation checks that the aimed node is active, is a container and is within
the normal resource interaction range. A precise mesh hit must also be within
that range. An arbitrary remote node ID cannot open it. Collection deactivates
the node and invalidates its collision geometry before another interaction can
grant loot. Existing respawn rules remain in place.

Both interaction and tool attacks share `Simulation::dropPropLoot`: existing
coin values, item rarity chances and heart chances are unchanged. Containers
produce the existing ResourceCollected event; they grant no harvest resources.

Presentation replaces the whole-model shrink with a 0.72-second cream cloud,
tumbling orange wood fragments, silver barrel hoops and brief gold sparkles.
The procedural triangle batch uses the existing unlit world shader, alpha
blending and scene depth testing. It needs no texture, shader, first-use asset
load or separate particle container. Low particle detail reduces cloud/chip
counts but retains the opening feedback. The existing wood-break sound now
also plays for wooden containers.

Public addition: `ResourceSystem::breakProp(EntityId)` returns the same optional
ResourceHit as damage. Container damage is now binary. ResourceNode health
fields remain for existing save compatibility; no save schema or stable ID
changes were made. PropBreak is appended to the presentation-only effect enum.

CPU checks cover all three container types, tool damage independence, repeated
interaction, stale IDs, collision invalidation, rifle-equipped interaction,
resource rejection, remote override rejection, reset and save/load. GPU checks
cover visible animation, expiry, Low detail, night readability and full scene
occlusion.
