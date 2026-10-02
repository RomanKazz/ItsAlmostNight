# Economic building cards

Lumber mills, quarries and crystal mines require a run blueprint acquired from
level-up choices. Each blueprint is a unique Common card with fixed power. They
are independent of the four combat building slots; all three may be unlocked.
Manual gathering and perimeter/foundation construction remain available.
Sandbox and unlimited-resource construction retain all building types.

While economic blueprints remain, each level-up offers one in the second card
position. A combat blueprint remains guaranteed in the first position while a
combat slot is free. Remaining cards use the existing weighted pool.

Economic production now uses the shared INDUSTRY HAMMER (+15% for a Common
card, scaled by rarity). It enters the pool after owning any producer blueprint.
HARVEST HAMMER improves manual gathering and stays available from the start.

The three type-specific speed cards are retired from new offers. Their stable
IDs and production modifiers remain for compatibility: old runs retain acquired
bonuses, which multiply global speed, level speed and health efficiency. The
producer system retains quarter-second cycle rounding and its minimum interval.

## Crystal mine balance and storage labels

The base crystal mine produces 2 crystals every 12 seconds (10 per minute),
down from 3 every 8 seconds (22.5 per minute). Its existing level, hammer and
health modifiers still apply. Lumber mill and quarry production is unchanged.
Local storage capacities and building costs remain compatible with saved runs.

Every full producer displays a world-space FULL label. Its render-target source
rectangle starts at the texture's height and uses negative height to flip the
image without sampling outside the texture's UV range.

## Interfaces and persistence

- New upgrade IDs are appended; existing IDs keep their values.
- `RunEconomyBuildingTypes`, `runCombatBuilding`, and
  `runUpgradeProductionBuilding` distinguish economic cards from combat slots.
- `CrystalMineSystem::setTypeProductionSpeedMultipliers` updates the three type
  modifiers. Simulation derives them from saved upgrade power on refresh.
- Suspended-run `buildingArsenalVersion` is 2. Older checkpoints preserve already
  built producers by acquiring their blueprint during migration. New checkpoints
  preserve unbuilt blueprints as well. Starting a new run clears them.

## Core build range and retired utility hammers

REACH HAMMER and SALVAGE HAMMER no longer appear in new card offers. Their IDs
and already acquired effects remain valid for suspended runs. Each successful
core upgrade now adds 3 meters to construction range: a normal run starts at
9 meters and reaches 30 meters at level 8. The core menu compares the current
and next range alongside capacity upgrades. Failed upgrades never add range.

Suspended runs record `coreBuildRadiusLevel` to avoid adding the level bonus
again after loading. Saves without that field start at accounted level 1 and
receive the missing core-level range once, in addition to their saved bonuses.

## Player weapon blueprints

Club, rifle, bombs, ice wand and fire wand are now one-time common acquisition
cards. A selected card adds the weapon to the run hotbar without occupying a
building slot; repeat offers disappear. Axe and pickaxe stay available, and
Alchemist retains its starting fire wand. Bomb acquisition supplies one bundle.
Core levels no longer unlock weapons. Sandbox/unlimited mode retains all tools.

Weapon card IDs are appended after existing effects. Suspended runs record
`playerWeaponArsenalVersion` (1); old saves without it retain weapons unlocked
by their historical core level. Ownership resets on a new run.
