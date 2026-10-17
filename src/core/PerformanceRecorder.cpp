#include "core/PerformanceRecorder.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace ian {

bool PerformanceRecorder::start(std::string_view directory) {
    stop();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    std::ostringstream filename;
    filename << directory << "/session_" << milliseconds << ".csv";
    path_ = filename.str();
    // A large userspace buffer keeps disk writes out of normal frame work.
    static thread_local char buffer[256U * 1024U];
    stream_.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    stream_.open(path_, std::ios::out | std::ios::trunc);
    if (!stream_) {
        path_.clear();
        return false;
    }
    framesSinceFlush_ = 0U;
    stream_ << "frame,session_seconds,run_state,build_mode,modular_build_mode,"
               "fixed_ticks,active_enemies,visible_enemies,buildings,"
               "modular_pieces,frame_ms,input_ms,update_ms,sim_tick_ms,"
               "render_ms,present_ms,present_pacing_ms,present_swap_ms,"
               "render_prep_ms,shadow_ms,selection_ms,"
               "terrain_ms,world_objects_ms,decorations_ms,grass_ms,"
               "environment_ms,pond_decor_ms,water_ms,cloud_ms,atmosphere_ms,"
               "overlays_ms,postprocess_ms,ui_ms,viewmodel_ms,world_ui_ms,"
               "interaction_prompt_ms,hud_core_ms,hud_target_health_ms,"
               "hud_resources_ms,hud_vitality_ms,hud_information_ms,"
               "hud_objectives_ms,hud_loot_ms,hud_phase_ms,"
               "hud_hotbar_ms,hud_feedback_ms,hud_text_flush_ms,"
               "resource_gain_ms,minimap_ms,minimap_setup_ms,"
               "minimap_target_refresh_ms,minimap_frame_ms,"
               "minimap_labels_ms,ui_overlays_ms,ui_text_ms,"
               "ui_text_submissions,"
               "ui_text_characters,gpu_world_ms,"
               "gpu_postprocess_ms,gpu_viewmodel_ms,"
               "gpu_viewmodel_target_ms,gpu_viewmodel_composite_ms,"
               "gpu_ui_ms,enemy_ai_ms,"
               "enemy_collision_ms,enemy_draw_ms,blob_shadows_ms";
    stream_ << ",gpu_viewmodel_target_samples,gpu_viewmodel_composite_samples,"
               "gpu_queries_issued,output_width,output_height,run_state_changed,"
               "minimap_prewarm_ms,terrain_chunks_submitted,terrain_chunks_culled,"
               "window_focused,window_minimized,window_focus_changed,"
               "final_batch_submission_ms,present_thread_cpu_ms,"
               "present_not_running_ms,present_thread_cpu_valid,"
               "defense_shot_events,active_defense_projectiles,optimized_build,"
               "ssao_enabled,bloom_enabled,ink_outlines_enabled,postprocessing_enabled\n";
    return true;
}

void PerformanceRecorder::record(
    const PerformanceFrameRecord& value) {
    if (!stream_) {
        return;
    }
    stream_ << value.frame << ',' << std::fixed << std::setprecision(4)
            << value.sessionSeconds << ',' << value.runState << ','
            << static_cast<int>(value.buildMode) << ','
            << static_cast<int>(value.modularBuildMode) << ','
            << value.fixedTicks << ',' << value.activeEnemies << ','
            << value.visibleEnemies << ',' << value.buildings << ','
            << value.modularPieces << ',' << value.frameMs << ','
            << value.inputMs << ',' << value.updateMs << ','
            << value.simulationTickMs << ',' << value.renderMs << ','
            << value.presentMs << ',' << value.presentPacingMs << ','
            << value.presentSwapMs << ',' << value.renderPreparationMs << ','
            << value.shadowMs << ',' << value.selectionMs << ','
            << value.terrainMs << ',' << value.worldObjectsMs << ','
            << value.decorationsMs << ',' << value.grassMs << ','
            << value.environmentMs << ',' << value.pondDecorMs << ','
            << value.waterMs << ',' << value.cloudMs << ','
            << value.atmosphereMs << ',' << value.overlaysMs << ','
            << value.postProcessMs << ',' << value.uiMs << ','
            << value.viewModelMs << ',' << value.worldUiMs << ','
            << value.interactionPromptMs << ',' << value.hudCoreMs << ','
            << value.hudTargetHealthMs << ','
            << value.hudResourcesMs << ',' << value.hudVitalityMs << ','
            << value.hudInformationMs << ','
            << value.hudObjectivesMs << ',' << value.hudLootMs << ','
            << value.hudPhaseMs << ',' << value.hudHotbarMs << ','
            << value.hudFeedbackMs << ',' << value.hudTextFlushMs << ','
            << value.resourceGainMs << ',' << value.minimapMs << ','
            << value.minimapSetupMs << ','
            << value.minimapTargetRefreshMs << ','
            << value.minimapFrameMs << ',' << value.minimapLabelsMs << ','
            << value.uiOverlaysMs << ',' << value.uiTextMs << ','
            << value.uiTextSubmissions << ','
            << value.uiTextCharacters << ','
            << value.gpuWorldMs << ',' << value.gpuPostProcessMs << ','
            << value.gpuViewModelMs << ','
            << value.gpuViewModelTargetMs << ','
            << value.gpuViewModelCompositeMs << ','
            << value.gpuUiMs << ','
            << value.enemyAiMs << ',' << value.enemyCollisionMs << ','
            << value.enemyDrawMs << ',' << value.blobShadowsMs;
    stream_ << ',' << value.gpuViewModelTargetSamples
            << ',' << value.gpuViewModelCompositeSamples
            << ',' << value.gpuQueriesIssued
            << ',' << value.outputWidth << ',' << value.outputHeight
            << ',' << static_cast<int>(value.runStateChanged)
            << ',' << value.minimapPreparationMs
            << ',' << value.terrainChunksSubmitted << ',' << value.terrainChunksCulled
            << ',' << static_cast<int>(value.windowFocused)
            << ',' << static_cast<int>(value.windowMinimized)
            << ',' << static_cast<int>(value.windowFocusChanged)
            << ',' << value.finalBatchSubmissionMs
            << ',' << value.presentThreadCpuMs
            << ',' << value.presentNotRunningMs
            << ',' << static_cast<int>(value.presentThreadCpuValid)
            << ',' << value.defenseShotEvents
            << ',' << value.activeDefenseProjectiles
            << ',' << static_cast<int>(value.optimizedBuild)
            << ',' << static_cast<int>(value.ssaoEnabled)
            << ',' << static_cast<int>(value.bloomEnabled)
            << ',' << static_cast<int>(value.inkOutlinesEnabled)
            << ',' << static_cast<int>(value.postProcessingEnabled)
            << '\n';
    ++framesSinceFlush_;
    constexpr std::uint32_t FlushIntervalFrames = 300U;
    if (framesSinceFlush_ >= FlushIntervalFrames) {
        stream_.flush();
        framesSinceFlush_ = 0U;
    }
}

void PerformanceRecorder::stop() {
    if (stream_) {
        stream_.flush();
        stream_.close();
    }
    framesSinceFlush_ = 0U;
}

bool PerformanceRecorder::active() const noexcept {
    return stream_.is_open();
}

const std::string& PerformanceRecorder::path() const noexcept {
    return path_;
}

} // namespace ian
