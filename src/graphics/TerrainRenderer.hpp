#pragma once

#include "world/TerrainHeightfield.hpp"

#include <raylib.h>

#include <vector>

namespace ian {

class TerrainRenderer {
  public:
    TerrainRenderer() = default;
    ~TerrainRenderer();

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;
    TerrainRenderer(TerrainRenderer&&) = delete;
    TerrainRenderer& operator=(TerrainRenderer&&) = delete;

    void rebuild(const TerrainHeightfield& terrain);
    void draw(
        Shader shader, Color tint,
        Vector3 focusPosition);
    void drawWater(Shader shader) const;
    void drawWireframe(Color tint) const;
    void shutdown();

    [[nodiscard]] bool ready() const;

  private:
    struct TerrainChunk {
        int x{};
        int z{};
        Model model{};
    };

    [[nodiscard]] Model buildChunk(
        int chunkX, int chunkZ) const;
    [[nodiscard]] Model buildWaterModel() const;
    void updateVisibleChunks(Vector3 focusPosition);

    const TerrainHeightfield* terrain_{};
    std::vector<TerrainChunk> chunks_;
    Model waterModel_{};
    bool ready_{};
};

} // namespace ian
