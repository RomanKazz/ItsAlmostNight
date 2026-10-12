# Weapon and tool cards

Musket (Rifle), Club, IceWand and FireWand use their existing stable acquisition
IDs. First card acquires level one, then each card grants one level. Axe and
Pickaxe begin at level one and have new appended IDs 57/58. Bomb acquisition
remains unique and provides its initial bundle; bomb purchasing is unchanged.
These cards occupy no tome slots. Common fixed-power cards cannot skip levels
at higher rarity. Eligible owned/new weapon groups split 50/50, independently
of group size, in both level choices and the existing four-offer shop.

Maximum level is the current Core level, capped at eight; before installing a
Core it is one. Capped cards disappear from the eligible pool and cannot be
selected/bought through a stale normal offer. Already-earned levels are retained
if the Core is lost. Debug bulk grants deliberately retain their existing cap
bypass for experimentation, but cannot exceed the eight-level weapon limit.

| Weapon | Level effect |
| --- | --- |
| Musket | Existing per-level damage (+1.5 internal), fire speed (+20% base), magazine (+2 rounds), reload (-0.25 s) progression extends to eight. Reload has a 0.45 s floor. |
| Club | +25% base hit damage, +6% base attack speed per extra level. |
| IceWand | +25% base direct damage, +6% cast speed per extra level. |
| FireWand | +25% base direct/burn damage, +6% cast speed per extra level. |
| Axe/Pickaxe | +35% base strike power per extra level, including harvesting. Node yield is unchanged. |

Level multipliers combine with the existing class/meta/tome bonuses. Refreshing
runtime effects is idempotent and does not repeatedly refill the musket.
Hotbar/inventory and card previews show owned levels. V is no longer a paid
weapon-upgrade action and emits no upgrade hint. Its modular-part action is
restricted to Buildings mode; its bomb-restock action remains available.

The former Resource Yield tome is now **Capacity Tome**, +20% world-node
stock and durability per common unit of power (30/40/50/60% for higher rarities).
It affects trees, stones and crystal deposits, including their respawns.
Yield density per point of harvesting damage stays approximately unchanged;
the node takes more work and contains more total resources. Existing extraction
is retained when upgrading a partially mined node. Dead nodes are not revived;
loot containers, carrying capacity and Core storage are unaffected. Meta/class
yield bonuses stay intact. The existing stat ID 27 is retained, with
ResourceCapacity as an alias of RichResources.

Checkpoint resourceNodeCapacityVersion is 1 (missing/zero means no node-capacity
bonus yet). Loading preserves an already-applied bonus and partial extraction;
older yield/storage tome saves migrate once to world stock and durability.
ResourceSystem::setNodeCapacityMultiplier and the optional fourth argument to
restoreNodes expose this modifier without changing existing callers. Existing
checkpoint bounds saturate individual nodes at one million stock/durability.

Persistence uses the existing upgrade count/power arrays, expanded by two
appended IDs. playerWeaponArsenalVersion is 2. Old ownership-only checkpoints
migrate baseline tools/starter weapons and retain the old musket level. Loading
rebuilds runtime stats exactly once. No PlayerWeapon IDs or old upgrade IDs were
reordered. Public additions: weaponLevels on SimulationSnapshot,
upgradeStats.resourceCapacity, PlayerWeaponSystem::setRifleLevel, and shared
level multiplier/card classification helpers.

Checks cover per-weapon power growth, single-level rarity semantics, Core caps,
shop filtering, ownership/upgrade weights, world stock and durability,
save/load/reset, old checkpoint migration, positive eight-level reload and
repeat-refresh ammo stability. High-level musket DPS and Core siege damage are
balance starting points to assess together in a full run.

High Stakes is retired: absent from level/shop/debug pools, with no wave
strength or additional-selection effect. Stable RiskyInvestment ID and the old
riskyInvestmentPending checkpoint key remain reserved for compatibility. Resume
clears old copies, pending activation and offers; existing earned generic rewards
are preserved. Public gameplay APIs are unchanged by this retirement.
