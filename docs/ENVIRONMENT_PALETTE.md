# Environment palette pass

Terrain is quieter than foliage, leaving saturated gameplay colors readable. All four visual biomes retain their identities and seed-specific palette variations.

| Biome | Ground RGB | Foliage RGB |
| --- | --- | --- |
| Forest | 0.33, 0.43, 0.30 | 0.38, 0.58, 0.43 |
| Pink grove | 0.51, 0.37, 0.44 | 0.74, 0.46, 0.61 |
| Autumn | 0.49, 0.40, 0.29 | 0.76, 0.50, 0.32 |
| Cold | 0.29, 0.47, 0.59 | 0.50, 0.66, 0.74 |

Both the world shader and instanced grass now use the palette hue for the forest, as they already did for the other zones. Source luminance preserves texture detail and the existing light/shadow ramps. The authored green reference constants remain fixed for normalization. The CPU ground-color sampler uses the same mapping so the minimap matches terrain.

Palette generation lives in `src/world/VisualBiomes.hpp`; shader mapping lives in `assets/shaders/world.fs` and `assets/shaders/grass_instanced.fs`. No public API, geometry, biome geography, UI, global color grading, building material, loot material or gameplay changes.

Validation: Debug configure/build and full CTest (17/18; the existing native SDF menu-text failure remains). Existing checks cover seeded biome identity, seamless boundaries, tree color consistency across batched/unbatched paths, cold-grass highlights, unchanged ordinary prop colors and minimap sampling. Before/after captures of all four biomes were inspected. Release is rebuilt; the Release graphics suite passes (1/1). No new tests were needed for this palette adjustment. Builds retain the existing duplicate `libian_simulation.a` linker warning.

The world shader had unrelated pending changes before this task. Only the palette mapping hunk is included in the palette commit; those unrelated edits remain in the working tree.
