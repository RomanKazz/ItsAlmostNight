#pragma once

#include "core/WorldObjectProfiling.hpp"

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
    // Appended columns keep existing CSV readers compatible.
    int windowWidth{};
    int windowHeight{};
    int renderWidth{};
    int renderHeight{};
    int frameRateLimit{};
    int graphicsQuality{};
    // 0 none, 1 resource, 2 enemy, 3 building, 4 chest, 5 modular,
    // 6 challenge, 7 landmark, 8 loot, 9 placement preview.
    int selectionKind{};
    std::size_t resourceNodes{};
    std::size_t coinPickups{};
    std::size_t lootChests{};
    std::size_t presentationEffects{};
    std::size_t enemyProjectiles{};
    std::size_t instancedEnemies{};
    std::size_t enemyBatches{};
    std::size_t lowDetailEnemies{};
    std::size_t blobShadowCount{};
    std::size_t blobShadowTriangles{};
    std::size_t enemyFullAiUpdates{};
    std::size_t enemyThrottledAiMoves{};
    std::size_t enemySpatialRebuilds{};
    std::size_t enemyStructureGridRebuilds{};
    double enemySpatialRebuildMs{};
    double simulationTickTotalMs{};
    double selectionCompositeMs{};
    double resourceImpactOverlaysMs{};
    bool gpuTimersSupported{};
    std::uint64_t gpuWorldSamples{};
    std::uint64_t gpuPostProcessSamples{};
    std::uint64_t gpuUiSamples{};
    // Start-to-start cadence also includes work after record() is called.
    double frameIntervalMs{};
    double gpuShadowMs{};
    double gpuSelectionMs{};
    double gpuSceneMs{};
    std::uint64_t gpuShadowSamples{};
    std::uint64_t gpuSelectionSamples{};
    std::uint64_t gpuSceneSamples{};
    double gpuTerrainMs{};
    double gpuObjectsMs{};
    double gpuEnvironmentMs{};
    double gpuOverlaysMs{};
    std::uint64_t gpuTerrainSamples{};
    std::uint64_t gpuObjectsSamples{};
    std::uint64_t gpuEnvironmentSamples{};
    std::uint64_t gpuOverlaysSamples{};
    WorldObjectTimings worldObjectTimings{};
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
    double finalBatchSubmissionMs{};
    double presentThreadCpuMs{};
    double presentNotRunningMs{};
    bool presentThreadCpuValid{};
    std::size_t defenseShotEvents{};
    std::size_t activeDefenseProjectiles{};
    // Build/settings are required to compare recordings without guessing.
    bool optimizedBuild{};
    bool ssaoEnabled{};
    bool bloomEnabled{};
    bool inkOutlinesEnabled{};
    bool postProcessingEnabled{};
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
