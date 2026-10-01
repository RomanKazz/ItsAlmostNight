# Economic building cards

Lumber mills, quarries and crystal mines require a run blueprint acquired from
level-up choices. Each blueprint is a unique Common card with fixed power. They
are independent of the four combat building slots; all three may be unlocked.
Manual gathering and perimeter/foundation construction remain available.
Sandbox and unlimited-resource construction retain all building types.

While economic blueprints remain, each level-up offers one in the second card
position. A combat blueprint remains guaranteed in the first position while a
combat slot is free. Remaining cards use the existing weighted pool.

Each producer has a production-speed card: SAW POWER, DEEP CUT, CRYSTAL FLOW.
Common gives +20%, Uncommon +30%, Rare +40%, Epic +50%, Legendary +60%.
Bonuses accumulate for that type and multiply the existing global production
speed, level speed and health efficiency. The producer system retains its
quarter-second cycle rounding and minimum interval. These cards enter the pool
only after owning their producer blueprint. WORKSHOP requires any producer;
RICH NODES still improves manual gathering and stays available from the start.
Cards reuse existing building icons and show a type-specific hover preview.

## Interfaces and persistence

- New upgrade IDs are appended; existing IDs keep their values.
- `RunEconomyBuildingTypes`, `runCombatBuilding`, and
  `runUpgradeProductionBuilding` distinguish economic cards from combat slots.
- `CrystalMineSystem::setTypeProductionSpeedMultipliers` updates the three type
  modifiers. Simulation derives them from saved upgrade power on refresh.
- Suspended-run `buildingArsenalVersion` is 2. Older checkpoints preserve already
  built producers by acquiring their blueprint during migration. New checkpoints
  preserve unbuilt blueprints as well. Starting a new run clears them.
