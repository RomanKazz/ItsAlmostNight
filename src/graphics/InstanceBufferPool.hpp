#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace ian {

// Lightweight handle to transforms uploaded for the current render frame.
// The source pointer is retained only for raylib's compatibility fallback.
struct InstanceBatch {
    unsigned int bufferId{};
    int instanceCount{};
    const Matrix* sourceTransforms{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return instanceCount > 0 && sourceTransforms != nullptr;
    }
};

// raylib's DrawMeshInstanced creates and destroys a VBO on every call. This
// pool retains triple-buffered dynamic VBOs, shares one transform upload
// across every mesh in a model, and skips unchanged uploads.
class InstanceBufferPool {
public:
    InstanceBufferPool() = default;
    ~InstanceBufferPool() = default;

    InstanceBufferPool(const InstanceBufferPool&) = delete;
    InstanceBufferPool& operator=(const InstanceBufferPool&) = delete;
    InstanceBufferPool(InstanceBufferPool&&) = delete;
    InstanceBufferPool& operator=(InstanceBufferPool&&) = delete;

    void beginFrame();
    void shutdown();

    [[nodiscard]] InstanceBatch upload(
        std::span<const Matrix> transforms);
    void drawMesh(Mesh mesh, Material material,
                  const InstanceBatch& batch) const;
    [[nodiscard]] bool drawModel(
        Model& model, Shader shader,
        std::span<const Matrix> transforms,
        Color tint = {255, 255, 255, 255});

private:
    struct BufferSlot {
        unsigned int id{};
        std::size_t capacityBytes{};
        std::vector<std::array<float, 16>> uploadedTransforms;
    };

    static constexpr std::size_t BufferedFrameCount = 3U;
    std::array<std::vector<BufferSlot>, BufferedFrameCount> frameBuffers_;
    std::vector<std::array<float, 16>> stagingTransforms_;
    std::size_t frameIndex_{BufferedFrameCount - 1U};
    std::size_t bufferCursor_{};
};

} // namespace ian
