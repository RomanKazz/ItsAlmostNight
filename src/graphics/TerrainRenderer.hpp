#pragma once

#include "world/TerrainHeightfield.hpp"

#include <raylib.h>

namespace ian {

class TerrainRenderer {
  public:
    TerrainRenderer() = default;
    ~TerrainRenderer() = default;

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;
    TerrainRenderer(TerrainRenderer&&) = delete;
    TerrainRenderer& operator=(TerrainRenderer&&) = delete;

    void rebuild(const TerrainHeightfield& terrain);
    void draw(Shader shader, Color tint);
    void drawWireframe(Color tint) const;
    void shutdown();

    [[nodiscard]] bool ready() const;

  private:
    Model model_{};
    bool ready_{};
};

} // namespace ian
