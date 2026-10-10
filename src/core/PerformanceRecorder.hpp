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
    double presentPacingMs{};
    double presentSwapMs{};
    double renderPreparationMs{};
    double shadowMs{};
    double selectionMs{};
    double terrainMs{};
    double worldObjectsMs{};
    double decorationsMs{};
    double grassMs{};
    double environmentMs{};
    double pondDecorMs{};
    double waterMs{};
    double cloudMs{};
    double atmosphereMs{};
    double overlaysMs{};
    double postProcessMs{};
    double uiMs{};
    double viewModelMs{};
    double worldUiMs{};
    double interactionPromptMs{};
    double hudCoreMs{};
    double hudTargetHealthMs{};
    double hudResourcesMs{};
    double hudVitalityMs{};
    double hudInformationMs{};
    double hudObjectivesMs{};
    double hudLootMs{};
    double hudPhaseMs{};
    double hudHotbarMs{};
    double hudFeedbackMs{};
    double hudTextFlushMs{};
    double resourceGainMs{};
    double minimapMs{};
    double minimapSetupMs{};
    double minimapTargetRefreshMs{};
    double minimapFrameMs{};
    double minimapLabelsMs{};
    double uiOverlaysMs{};
    double uiTextMs{};
    std::size_t uiTextSubmissions{};
    std::size_t uiTextCharacters{};
    double gpuWorldMs{};
    double gpuPostProcessMs{};
    double gpuViewModelMs{};
    double gpuViewModelTargetMs{};
    double gpuViewModelCompositeMs{};
    double gpuUiMs{};
    double enemyAiMs{};
    double enemyCollisionMs{};
    double enemyDrawMs{};
    double blobShadowsMs{};
    std::uint64_t gpuViewModelTargetSamples{};
    std::uint64_t gpuViewModelCompositeSamples{};
    std::size_t gpuQueriesIssued{};
    int outputWidth{};
    int outputHeight{};
    bool runStateChanged{};
    double minimapPreparationMs{};
    std::size_t terrainChunksSubmitted{};
    std::size_t terrainChunksCulled{};
    bool windowFocused{};
    bool windowMinimized{};
    bool windowFocusChanged{};
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
