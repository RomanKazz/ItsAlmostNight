# Stump

Original low-poly character for ItsAlmostNight. All editable parts and the palette
are packed in `stump.blend`; `palette.png` is a local copy of the chest relic family
palette used for reproducible authoring. The GLB is in
`assets/models/characters/stump.glb`; the 512px transparent portrait is rendered
from that clean GLB import, with no painted geometry or external model generation.

Runtime usage, gameplay and validation are described in
[STUMP_CHARACTER_RU.md](../../../docs/STUMP_CHARACTER_RU.md).

Front/side/back/top and reference images use the same neutral studio lighting.
The reference tree's collision proxies are hidden. The model has no external
third-party geometry, rig, mechanical sockets or gameplay collision: it currently
supplies the portrait in this first-person game.
