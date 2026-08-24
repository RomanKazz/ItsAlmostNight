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
               "render_ms,present_ms,render_prep_ms,shadow_ms,selection_ms,"
               "terrain_ms,world_objects_ms,decorations_ms,grass_ms,"
               "environment_ms,pond_decor_ms,water_ms,cloud_ms,atmosphere_ms,"
               "overlays_ms,postprocess_ms,ui_ms,enemy_ai_ms,"
               "enemy_collision_ms,enemy_draw_ms,blob_shadows_ms\n";
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
            << value.presentMs << ',' << value.renderPreparationMs << ','
            << value.shadowMs << ',' << value.selectionMs << ','
            << value.terrainMs << ',' << value.worldObjectsMs << ','
            << value.decorationsMs << ',' << value.grassMs << ','
            << value.environmentMs << ',' << value.pondDecorMs << ','
            << value.waterMs << ',' << value.cloudMs << ','
            << value.atmosphereMs << ',' << value.overlaysMs << ','
            << value.postProcessMs << ',' << value.uiMs << ','
            << value.enemyAiMs << ',' << value.enemyCollisionMs << ','
            << value.enemyDrawMs << ',' << value.blobShadowsMs << '\n';
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
