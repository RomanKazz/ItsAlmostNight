#pragma once

#include "core/Types.hpp"
#include "game/GameBalance.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ian {

inline constexpr std::uint8_t MaxBuildingLevel = 8;

enum class BuildingType {
    Core,
    Wall,
    Turret,
    GoldMine,
    Cannon,
    SlowTrap,
    Gate,
    LumberMill,
    Quarry,
    SpikeTrap,
};

enum WallConnection : std::uint8_t {
    WallConnectionNorth = 1U << 0U,
    WallConnectionEast = 1U << 1U,
    WallConnectionSouth = 1U << 2U,
    WallConnectionWest = 1U << 3U,
};

struct GridPosition {
    int x{};
    int z{};

    friend bool operator==(const GridPosition&, const GridPosition&) = default;
};

struct ResourceCost {
    int wood{};
    int stone{};
    int gold{};

    friend bool operator==(
        const ResourceCost&, const ResourceCost&) = default;
};

struct BuildingInstance {
    EntityId id;
    BuildingType type;
    GridPosition gridPosition;
    std::uint8_t rotation{};
    std::uint8_t level{1};
    double health{};
    double maxHealth{};
    bool open{};
    double baseHeight{};
    int platformStorey{-1};
    double foundationBottomHeight{};
    bool anvilEnhanced{};
    std::uint8_t anvilStacks{};
};

enum class PlacementError {
    None,
    CoreAlreadyPlaced,
    CoreRequired,
    InsufficientResources,
    Occupied,
    OutsideCoreArea,
    PlayerOverlap,
    WorldCollision,
    LimitReached,
    OutOfRange,
    CoreLevelRequired,
    ResourceBlocked,
};

struct PlacementResult {
    PlacementError error{PlacementError::None};
    ResourceCost cost;

    [[nodiscard]] bool valid() const { return error == PlacementError::None; }
};

struct PlacedBuilding {
    BuildingInstance building;
    ResourceCost cost;
};

struct BuildingDamageResult {
    EntityId id;
    BuildingType type;
    GridPosition gridPosition;
    double remainingHealth;
    bool destroyed;
    double baseHeight{};
};

enum class UpgradeError {
    None,
    NotFound,
    MaxLevel,
    Unsupported,
    CoreLevelRequired,
    InsufficientResources,
};

struct UpgradeBuildingCommand {
    EntityId buildingId;
};

struct RepairBuildingCommand {
    EntityId buildingId;
};

struct SellBuildingCommand {
    EntityId buildingId;
};

struct UpgradeResult {
    UpgradeError error{UpgradeError::None};
    std::optional<BuildingInstance> building;
    ResourceCost cost;

    [[nodiscard]] bool valid() const { return error == UpgradeError::None; }
};

enum class BuildingActionError {
    None,
    NotFound,
    FullHealth,
    Unsupported,
    InsufficientResources,
};

struct RepairResult {
    BuildingActionError error{BuildingActionError::None};
    std::optional<BuildingInstance> building;
    ResourceCost cost;
    double repairedHealth{};

    [[nodiscard]] bool valid() const { return error == BuildingActionError::None; }
};

struct SellResult {
    BuildingActionError error{BuildingActionError::None};
    std::optional<BuildingInstance> building;
    ResourceCost refund;

    [[nodiscard]] bool valid() const { return error == BuildingActionError::None; }
};

struct BuildingPreview {
    BuildingType type;
    GridPosition gridPosition;
    std::uint8_t rotation;
    PlacementResult placement;
    double baseHeight{};
    int platformStorey{-1};
    double foundationBottomHeight{};
};

struct PlaceBuildingCommand {
    BuildingType type;
    GridPosition gridPosition;
    std::uint8_t rotation;
    double baseHeight{};
    int platformStorey{-1};
    bool lockHeight{};
};

class BuildingSystem {
  public:
    explicit BuildingSystem(
        std::array<BuildingBalanceDefinition, GameBalance::BuildingTypeCount> definitions =
            GameBalance::defaults().buildings,
        EconomyBalanceDefinition economy = GameBalance::defaults().economy,
        int coreBuildRadius = 12);

    void reset();
    void setMaxHealthMultiplier(double multiplier);
    double restoreHealthFraction(double fraction);
    void setNewTowerBonusEnabled(bool enabled);
    void setNewTowerBonusStacks(int stacks);

    [[nodiscard]] PlacementResult validate(BuildingType type, GridPosition position, int wood,
                                           int stone, int gold = 0,
                                           double baseHeight = 0.0) const;
    std::optional<PlacedBuilding> place(BuildingType type, GridPosition position,
                                        std::uint8_t rotation, int wood, int stone, int gold = 0,
                                        double baseHeight = 0.0,
                                        int platformStorey = -1,
                                        double foundationBottomHeight = 0.0);
    std::optional<BuildingInstance> remove(EntityId id);
    std::optional<BuildingDamageResult> damage(EntityId id, double amount);
    std::optional<BuildingInstance> toggleGate(EntityId id);
    [[nodiscard]] std::optional<EntityId> raycast(Vec3 origin, Vec3 direction,
                                                  double maxDistance) const;
    [[nodiscard]] RepairResult validateRepair(EntityId id, int wood, int stone, int gold) const;
    RepairResult repair(EntityId id, int wood, int stone, int gold);
    SellResult sell(EntityId id);
    [[nodiscard]] ResourceCost configuredCost(BuildingType type) const;
    [[nodiscard]] ResourceCost upgradeCost(const BuildingInstance& building) const;
    [[nodiscard]] UpgradeResult validateUpgrade(EntityId id, int wood, int stone, int gold) const;
    UpgradeResult upgrade(EntityId id, int wood, int stone, int gold);

    [[nodiscard]] bool hasCore() const;
    [[nodiscard]] std::optional<BuildingInstance> core() const;
    [[nodiscard]] const std::vector<BuildingInstance>& buildings() const;

  private:
    [[nodiscard]] bool overlaps(BuildingType type, GridPosition position,
                                double baseHeight) const;
    [[nodiscard]] const BuildingBalanceDefinition& definition(BuildingType type) const;
    [[nodiscard]] ResourceCost cost(BuildingType type) const;
    [[nodiscard]] ResourceCost repairCost(const BuildingInstance& building) const;
    [[nodiscard]] ResourceCost sellRefund(const BuildingInstance& building) const;

    std::vector<BuildingInstance> buildings_;
    std::uint32_t nextIndex_{1000};
    std::array<BuildingBalanceDefinition, GameBalance::BuildingTypeCount> definitions_;
    EconomyBalanceDefinition economy_;
    int coreBuildRadius_;
    double maxHealthMultiplier_{1.0};
    bool newTowerBonusEnabled_{};
    std::uint8_t newTowerBonusStacks_{};
};

[[nodiscard]] ResourceCost buildingCost(BuildingType type);
[[nodiscard]] ResourceCost buildingRepairCost(const BuildingInstance& building);
[[nodiscard]] ResourceCost buildingSellRefund(const BuildingInstance& building);
[[nodiscard]] ResourceCost buildingUpgradeCost(const BuildingInstance& building);
[[nodiscard]] bool buildingBlocksMovement(BuildingType type);
[[nodiscard]] bool buildingBlocksMovement(const BuildingInstance& building);
[[nodiscard]] double buildingFootprintHalfExtent(
    BuildingType type);
[[nodiscard]] Vec3 buildingWorldPosition(
    BuildingType type, GridPosition position);
[[nodiscard]] Vec3 buildingWorldPosition(
    const BuildingInstance& building);
[[nodiscard]] Vec3 buildingWorldPosition(
    const BuildingDamageResult& building);
[[nodiscard]] std::uint8_t wallConnectionMask(
    std::span<const BuildingInstance> buildings, GridPosition position,
    double baseHeight = 0.0);
[[nodiscard]] std::uint8_t wallFallbackRotation(
    std::span<const BuildingInstance> buildings,
    const BuildingInstance& wall);

inline constexpr double MinimumPlacementDistance = 1.0;
inline constexpr double MaximumPlacementDistance = 10.0;

[[nodiscard]] GridPosition aimedBuildingGridPosition(
    Vec3 playerPosition, double yaw, double pitch,
    double minimumDistance = MinimumPlacementDistance,
    double maximumDistance = MaximumPlacementDistance,
    BuildingType type = BuildingType::Core,
    double placementPlaneHeight = 0.0);

} // namespace ian
