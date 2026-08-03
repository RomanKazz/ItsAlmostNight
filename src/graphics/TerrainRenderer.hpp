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
    [[nodiscard]] bool chunkVisible(
        int chunkX, int chunkZ,
        Vector3 focusPosition) const;
    void updateVisibleChunks(Vector3 focusPosition);

    const TerrainHeightfield* terrain_{};
    std::vector<TerrainChunk> chunks_;
    bool ready_{};
};

} // namespace ian
