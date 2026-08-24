#include "graphics/InstanceBufferPool.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>

namespace ian {
namespace {

using PackedMatrix = std::array<float, 16>;
constexpr int MaterialMapCount = MATERIAL_MAP_BRDF + 1;

PackedMatrix packMatrix(Matrix transform) {
    PackedMatrix packed{};
    const float16 values = MatrixToFloatV(transform);
    std::copy_n(values.v, packed.size(), packed.begin());
    return packed;
}

void uploadMaterialColor(const Material& material) {
    if (material.maps == nullptr) {
        return;
    }
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] >= 0) {
        const Color color =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        const float values[4]{
            static_cast<float>(color.r) / 255.0F,
            static_cast<float>(color.g) / 255.0F,
            static_cast<float>(color.b) / 255.0F,
            static_cast<float>(color.a) / 255.0F,
        };
        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE],
                     values, SHADER_UNIFORM_VEC4, 1);
    }
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] >= 0) {
        const Color color =
            material.maps[MATERIAL_MAP_SPECULAR].color;
        const float values[4]{
            static_cast<float>(color.r) / 255.0F,
            static_cast<float>(color.g) / 255.0F,
            static_cast<float>(color.b) / 255.0F,
            static_cast<float>(color.a) / 255.0F,
        };
        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR],
                     values, SHADER_UNIFORM_VEC4, 1);
    }
}

void bindMaterialTextures(const Material& material) {
    if (material.maps == nullptr) {
        return;
    }
    for (int mapIndex = 0; mapIndex < MaterialMapCount; ++mapIndex) {
        const Texture2D texture = material.maps[mapIndex].texture;
        if (texture.id == 0U) {
            continue;
        }
        rlActiveTextureSlot(mapIndex);
        if (mapIndex == MATERIAL_MAP_IRRADIANCE ||
            mapIndex == MATERIAL_MAP_PREFILTER ||
            mapIndex == MATERIAL_MAP_CUBEMAP) {
            rlEnableTextureCubemap(texture.id);
        } else {
            rlEnableTexture(texture.id);
        }
        const int location =
            material.shader.locs[SHADER_LOC_MAP_DIFFUSE + mapIndex];
        if (location >= 0) {
            rlSetUniform(location, &mapIndex, SHADER_UNIFORM_INT, 1);
        }
    }
}

void unbindMaterialTextures(const Material& material) {
    if (material.maps == nullptr) {
        return;
    }
    for (int mapIndex = 0; mapIndex < MaterialMapCount; ++mapIndex) {
        if (material.maps[mapIndex].texture.id == 0U) {
            continue;
        }
        rlActiveTextureSlot(mapIndex);
        if (mapIndex == MATERIAL_MAP_IRRADIANCE ||
            mapIndex == MATERIAL_MAP_PREFILTER ||
            mapIndex == MATERIAL_MAP_CUBEMAP) {
            rlDisableTextureCubemap();
        } else {
            rlDisableTexture();
        }
    }
}

} // namespace

void InstanceBufferPool::beginFrame() {
    frameIndex_ = (frameIndex_ + 1U) % BufferedFrameCount;
    bufferCursor_ = 0U;
}

void InstanceBufferPool::shutdown() {
    for (auto& frame : frameBuffers_) {
        for (BufferSlot& slot : frame) {
            if (slot.id != 0U) {
                rlUnloadVertexBuffer(slot.id);
            }
        }
        frame.clear();
    }
    stagingTransforms_.clear();
    frameIndex_ = BufferedFrameCount - 1U;
    bufferCursor_ = 0U;
}

InstanceBatch InstanceBufferPool::upload(
    std::span<const Matrix> transforms) {
    if (transforms.empty() ||
        transforms.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    auto& buffers = frameBuffers_[frameIndex_];
    if (bufferCursor_ >= buffers.size()) {
        buffers.emplace_back();
    }
    BufferSlot& slot = buffers[bufferCursor_++];

    stagingTransforms_.resize(transforms.size());
    std::transform(
        transforms.begin(), transforms.end(),
        stagingTransforms_.begin(), packMatrix);
    const std::size_t dataBytes =
        stagingTransforms_.size() * sizeof(PackedMatrix);
    if (dataBytes >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    rlDrawRenderBatchActive();
    bool recreated = false;
    if (slot.id == 0U || slot.capacityBytes < dataBytes) {
        if (slot.id != 0U) {
            rlUnloadVertexBuffer(slot.id);
        }
        constexpr std::size_t MinimumBufferBytes = 256U;
        slot.capacityBytes = std::max(
            MinimumBufferBytes, std::bit_ceil(dataBytes));
        if (slot.capacityBytes >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            slot = {};
            return {};
        }
        slot.id = rlLoadVertexBuffer(
            nullptr, static_cast<int>(slot.capacityBytes), true);
        recreated = true;
    }

    const bool unchanged = !recreated &&
        slot.uploadedTransforms.size() == stagingTransforms_.size() &&
        std::memcmp(
            slot.uploadedTransforms.data(), stagingTransforms_.data(),
            dataBytes) == 0;
    if (!unchanged && slot.id != 0U) {
        rlUpdateVertexBuffer(
            slot.id, stagingTransforms_.data(),
            static_cast<int>(dataBytes), 0);
        slot.uploadedTransforms = stagingTransforms_;
    }
    rlDisableVertexBuffer();

    return {
        .bufferId = slot.id,
        .instanceCount = static_cast<int>(transforms.size()),
        .sourceTransforms = transforms.data(),
    };
}

void InstanceBufferPool::drawMesh(
    Mesh mesh, Material material,
    const InstanceBatch& batch) const {
    if (!batch || mesh.vertexCount <= 0) {
        return;
    }
    const int transformLocation =
        material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM];
    if (batch.bufferId == 0U || mesh.vaoId == 0U ||
        transformLocation < 0 || rlIsStereoRenderEnabled()) {
        DrawMeshInstanced(
            mesh, material, batch.sourceTransforms,
            batch.instanceCount);
        return;
    }

    rlDrawRenderBatchActive();
    rlEnableShader(material.shader.id);
    uploadMaterialColor(material);

    const Matrix view = rlGetMatrixModelview();
    const Matrix projection = rlGetMatrixProjection();
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] >= 0) {
        rlSetUniformMatrix(
            material.shader.locs[SHADER_LOC_MATRIX_VIEW], view);
    }
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] >= 0) {
        rlSetUniformMatrix(
            material.shader.locs[SHADER_LOC_MATRIX_PROJECTION],
            projection);
    }
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] >= 0) {
        rlSetUniformMatrix(
            material.shader.locs[SHADER_LOC_MATRIX_NORMAL],
            MatrixIdentity());
    }

    if (!rlEnableVertexArray(mesh.vaoId)) {
        rlDisableShader();
        DrawMeshInstanced(
            mesh, material, batch.sourceTransforms,
            batch.instanceCount);
        return;
    }
    rlEnableVertexBuffer(batch.bufferId);
    for (int column = 0; column < 4; ++column) {
        const unsigned int attribute = static_cast<unsigned int>(
            transformLocation + column);
        rlEnableVertexAttribute(attribute);
        rlSetVertexAttribute(
            attribute, 4, RL_FLOAT, false, sizeof(Matrix),
            column * static_cast<int>(sizeof(Vector4)));
        rlSetVertexAttributeDivisor(attribute, 1);
    }

    bindMaterialTextures(material);
    const Matrix modelView = MatrixMultiply(
        rlGetMatrixTransform(), view);
    const Matrix mvp = MatrixMultiply(modelView, projection);
    if (material.shader.locs[SHADER_LOC_MATRIX_MVP] >= 0) {
        rlSetUniformMatrix(
            material.shader.locs[SHADER_LOC_MATRIX_MVP], mvp);
    }
    if (mesh.indices != nullptr) {
        rlDrawVertexArrayElementsInstanced(
            0, mesh.triangleCount * 3, nullptr,
            batch.instanceCount);
    } else {
        rlDrawVertexArrayInstanced(
            0, mesh.vertexCount, batch.instanceCount);
    }

    unbindMaterialTextures(material);
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
    rlDisableShader();
}

bool InstanceBufferPool::drawModel(
    Model& model, Shader shader,
    std::span<const Matrix> transforms, Color tint) {
    if (transforms.empty() || model.meshCount <= 0 ||
        model.meshes == nullptr || model.meshMaterial == nullptr ||
        model.materials == nullptr) {
        return false;
    }
    const InstanceBatch batch = upload(transforms);
    if (!batch) {
        return false;
    }
    bool drewMesh = false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        Material material = model.materials[materialIndex];
        material.shader = shader;
        Color originalDiffuseColor{};
        const bool hasMaterialMaps = material.maps != nullptr;
        if (hasMaterialMaps) {
            originalDiffuseColor =
                material.maps[MATERIAL_MAP_DIFFUSE].color;
        }
        if (material.maps != nullptr) {
            material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
        }
        drawMesh(model.meshes[meshIndex], material, batch);
        if (hasMaterialMaps) {
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                originalDiffuseColor;
        }
        drewMesh = true;
    }
    return drewMesh;
}

} // namespace ian
