#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

namespace ian {

struct PerformanceFrameRecord {
    std::uint64_t frame{};
    double sessionSeconds{};
    int runState{};
    bool buildMode{};
    bool modularBuildMode{};
    std::size_t fixedTicks{};
    std::size_t activeEnemies{};
    std::size_t visibleEnemies{};
    std::size_t buildings{};
    std::size_t modularPieces{};
    double frameMs{};
    double inputMs{};
    double updateMs{};
    double simulationTickMs{};
    double renderMs{};
    double presentMs{};
    double renderPreparationMs{};
    double shadowMs{};
    double selectionMs{};
    double terrainMs{};
    double worldObjectsMs{};
    double environmentMs{};
    double overlaysMs{};
    double postProcessMs{};
    double uiMs{};
    double enemyAiMs{};
    double enemyCollisionMs{};
    double enemyDrawMs{};
    double blobShadowsMs{};
};

class PerformanceRecorder {
  public:
    bool start(std::string_view directory);
    void record(const PerformanceFrameRecord& frame);
    void stop();

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] const std::string& path() const noexcept;

  private:
    std::ofstream stream_;
    std::string path_;
    std::uint32_t framesSinceFlush_{};
};

} // namespace ian
