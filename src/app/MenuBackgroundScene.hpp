#pragma once

#include "buildings/BuildingSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <nlohmann/json_fwd.hpp>

namespace ian {

// Decoration for the menu only: never submitted to the gameplay simulation.
class MenuBackgroundScene {
  public:
    static constexpr std::size_t MaximumBuildings = 512U;

    [[nodiscard]] static bool supports(BuildingType type);
    [[nodiscard]] BuildingPreview preview(
        BuildingType type, Vec3 terrainHit, std::uint8_t rotation,
        const TerrainHeightfield& terrain, double worldLimit) const;
    bool place(const BuildingPreview& preview);
    bool remove(Vec3 origin, Vec3 direction, double maximumDistance);
    [[nodiscard]] const std::vector<BuildingInstance>& buildings() const;
    [[nodiscard]] nlohmann::json toJson() const;
    // Invalid saves leave the current scene intact.
    bool loadJson(const nlohmann::json& document, double worldLimit);
    bool copyBuildings(std::span<const BuildingInstance> buildings);

  private:
    BuildingSystem buildings_;
    std::uint32_t nextIndex_{1000U};
};

} // namespace ian
