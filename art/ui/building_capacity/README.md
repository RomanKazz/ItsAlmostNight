# Shared construction capacity

Runtime icon: `assets/ui/tomes/expansion.png` (256×256 RGBA).
Source: `art/ui/building_capacity/expansion_source.png`.
Generated with the built-in `imagegen` tool, referencing the existing Fortitude
and Industry tome PNGs. Exact final prompt: `art/ui/building_capacity/prompt.txt`.
Resized with native macOS `sips`, preserving alpha.

The placed-building budget is 3 + 2 × (Core level − 1). All producers, towers,
support structures and traps share it. The Core, walls, gates and modular
construction do not consume places. Selling/destruction releases a place;
loading preserves old bases even above budget. Godmode bypasses it.

Append-only upgrade IDs: BuildingCapacity = 68 (tome, accumulated fractional
rarity power, floor at use), ExtraBuilding = 69 (shop-only fixed +1, slot-free).
Building shrines offer BuildingCapacity as a separate, slot-free +1 blessing.
The existing four blueprint-type slots and their meta progression stay separate.
All three bonus sources derive capacity from persisted upgrade/shrine state;
no duplicated capacity counter is written to save files.

Interface changes: BuildingLimitCategory now has one Structure category;
BuildingSystem exposes placedBuildingCount(), totalBuildingLimit(),
buildingLimitBonus(), setBuildingLimitBonus(). SimulationSnapshot exposes the
three counters to the UI. BuildingStats replaces its two old limit fields with
buildingLimit. Runtime ownership, saves and UI were verified in the current
workspace, preserving pre-existing uncommitted work.

Checks: full Debug and ASan/UBSan 42/42; focused building/capacity/save/shop
checks additionally pass in Debug, ASan and Release; Release shop/shrine/card
preview/viewport tests 4/4. EN and RU shop previews were inspected.
