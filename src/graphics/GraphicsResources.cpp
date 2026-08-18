#include "graphics/GraphicsResources.hpp"

#include <nlohmann/json.hpp>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace ian {
namespace {

constexpr int MaterialMapCount = 12;

using Json = nlohmann::json;

std::optional<Json> loadGltfDocument(
    std::string_view modelPath) {
    if (modelPath.empty()) return std::nullopt;
    std::ifstream file(
        std::string(modelPath),
        std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;
    const std::streampos end = file.tellg();
    if (end <= 0) return std::nullopt;
    std::vector<char> bytes(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    file.read(bytes.data(), static_cast<std::streamsize>(end));
    if (!file) return std::nullopt;

    try {
        const std::filesystem::path path{
            std::string(modelPath)};
        std::string extension = path.extension().string();
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        if (extension != ".glb") {
            return Json::parse(bytes.begin(), bytes.end());
        }
        if (bytes.size() < 20U) return std::nullopt;
        const auto readU32 = [&bytes](std::size_t offset) {
            return static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[offset])) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(bytes[offset + 1U]))
                 << 8U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(bytes[offset + 2U]))
                 << 16U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(bytes[offset + 3U]))
                 << 24U);
        };
        if (readU32(0U) != 0x46546c67U || readU32(4U) != 2U) {
            return std::nullopt;
        }
        const std::size_t declaredLength = readU32(8U);
        if (declaredLength > bytes.size()) return std::nullopt;
        constexpr std::uint32_t JsonChunk = 0x4e4f534aU;
        std::size_t offset = 12U;
        while (offset + 8U <= bytes.size()) {
            const std::size_t chunkLength =
                static_cast<std::size_t>(readU32(offset));
            const std::uint32_t chunkType = readU32(offset + 4U);
            offset += 8U;
            if (chunkLength > bytes.size() - offset) {
                return std::nullopt;
            }
            if (chunkType == JsonChunk) {
                return Json::parse(
                    bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                    bytes.begin() + static_cast<std::ptrdiff_t>(
                        offset + chunkLength));
            }
            offset += chunkLength;
        }
    } catch (const std::exception&) {
        // Model loading itself remains authoritative. This helper only
        // repairs external texture paths when the asset can be inspected.
    }
    return std::nullopt;
}

std::vector<std::string> gltfDiffuseTextureUris(
    std::string_view modelPath) {
    std::vector<std::string> result;
    const auto document = loadGltfDocument(modelPath);
    if (!document || !document->contains("materials") ||
        !(*document)["materials"].is_array()) {
        return result;
    }
    const Json empty{};
    const Json& textures = document->contains("textures") &&
            (*document)["textures"].is_array()
        ? (*document)["textures"]
        : empty;
    const Json& images = document->contains("images") &&
            (*document)["images"].is_array()
        ? (*document)["images"]
        : empty;
    for (const Json& material : (*document)["materials"]) {
        std::string uri;
        const auto pbr = material.find("pbrMetallicRoughness");
        if (pbr != material.end() && pbr->is_object()) {
            const auto baseColor =
                pbr->find("baseColorTexture");
            if (baseColor != pbr->end() && baseColor->is_object()) {
                const auto textureIndex = baseColor->find("index");
                if (textureIndex != baseColor->end() &&
                    textureIndex->is_number_integer()) {
                    const int texture = textureIndex->get<int>();
                    if (texture >= 0 &&
                        static_cast<std::size_t>(texture) <
                            textures.size() &&
                        textures[static_cast<std::size_t>(texture)]
                            .is_object()) {
                        const auto source = textures[
                            static_cast<std::size_t>(texture)].find(
                                "source");
                        if (source != textures[
                                static_cast<std::size_t>(texture)].end() &&
                            source->is_number_integer()) {
                            const int image = source->get<int>();
                            if (image >= 0 &&
                                static_cast<std::size_t>(image) <
                                    images.size() &&
                                images[static_cast<std::size_t>(image)]
                                    .is_object()) {
                                const auto imageUri = images[
                                    static_cast<std::size_t>(image)].find(
                                        "uri");
                                if (imageUri != images[
                                        static_cast<std::size_t>(image)]
                                        .end() &&
                                    imageUri->is_string()) {
                                    uri = imageUri->get<std::string>();
                                }
                            }
                        }
                    }
                }
            }
        }
        result.push_back(std::move(uri));
    }
    return result;
}

std::vector<std::filesystem::path> textureCandidates(
    std::string_view modelPath, std::string_view uri) {
    std::vector<std::filesystem::path> result;
    if (uri.empty() || uri.starts_with("data:")) return result;
    std::string normalizedUri(uri);
    std::replace(normalizedUri.begin(), normalizedUri.end(), '\\', '/');
    const std::filesystem::path uriPath(normalizedUri);
    if (uriPath.is_absolute()) {
        result.push_back(uriPath);
        return result;
    }
    std::filesystem::path directory{
        std::string(modelPath)};
    directory = directory.parent_path();
    for (int level = 0; level < 8; ++level) {
        result.push_back(directory / uriPath);
        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    return result;
}

std::optional<std::filesystem::path> existingTexturePath(
    const std::vector<std::filesystem::path>& candidates) {
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
        error.clear();
    }
    return std::nullopt;
}

void repairMissingGltfTextures(
    Model& model, std::string_view modelPath) {
    if (model.materials == nullptr || model.materialCount <= 0 ||
        model.meshMaterial == nullptr) {
        return;
    }
    const std::vector<std::string> uris =
        gltfDiffuseTextureUris(modelPath);
    // raylib keeps material slot 0 as a default material and puts glTF
    // materials at slots 1..N. Keep the offset conditional so this helper
    // also remains correct for loaders that do not add that default slot.
    const int materialOffset = model.materialCount ==
            static_cast<int>(uris.size()) + 1
        ? 1
        : 0;
    for (std::size_t gltfIndex = 0;
         gltfIndex < uris.size(); ++gltfIndex) {
        if (uris[gltfIndex].empty()) {
            continue;
        }
        const int materialIndex =
            static_cast<int>(gltfIndex) + materialOffset;
        if (materialIndex < 0 ||
            materialIndex >= model.materialCount) {
            continue;
        }
        Material& material = model.materials[materialIndex];
        if (material.maps == nullptr) continue;
        Texture2D& diffuse =
            material.maps[MATERIAL_MAP_DIFFUSE].texture;
        if (diffuse.id != 0U && IsTextureValid(diffuse)) {
            continue;
        }
        const auto path = existingTexturePath(textureCandidates(
            modelPath, uris[gltfIndex]));
        if (!path) {
            TraceLog(LOG_WARNING,
                     "MODEL: external texture '%s' for '%s' was not found",
                     uris[gltfIndex].c_str(),
                     std::string(modelPath).c_str());
            diffuse = {};
            continue;
        }
        const Texture2D repaired = LoadTexture(path->string().c_str());
        if (IsTextureValid(repaired)) {
            diffuse = repaired;
            TraceLog(LOG_INFO,
                     "MODEL: resolved texture '%s' for '%s'",
                     path->string().c_str(),
                     std::string(modelPath).c_str());
        } else {
            TraceLog(LOG_WARNING,
                     "MODEL: failed to load resolved texture '%s' for '%s'",
                     path->string().c_str(),
                     std::string(modelPath).c_str());
            diffuse = {};
        }
    }
}

} // namespace

ShaderResource::~ShaderResource() {
    unload();
}

bool ShaderResource::load(const char* vertexPath, const char* fragmentPath) {
    unload();
    if ((vertexPath != nullptr && !FileExists(vertexPath)) ||
        (fragmentPath != nullptr && !FileExists(fragmentPath))) {
        return false;
    }
    shader_ = LoadShader(vertexPath, fragmentPath);
    loaded_ = IsShaderValid(shader_);
    if (!loaded_) {
        UnloadShader(shader_);
        shader_ = {};
    }
    return loaded_;
}

void ShaderResource::unload() {
    if (loaded_) {
        UnloadShader(shader_);
        shader_ = {};
        loaded_ = false;
    }
}

bool ShaderResource::valid() const {
    return loaded_;
}

Shader& ShaderResource::get() {
    return shader_;
}

const Shader& ShaderResource::get() const {
    return shader_;
}

TextureResource::~TextureResource() {
    unload();
}

bool TextureResource::load(const char* path) {
    unload();
    texture_ = LoadTexture(path);
    loaded_ = IsTextureValid(texture_);
    if (!loaded_ && texture_.id != 0U) {
        UnloadTexture(texture_);
        texture_ = {};
    }
    return loaded_;
}

void TextureResource::unload() {
    if (loaded_) {
        UnloadTexture(texture_);
        texture_ = {};
        loaded_ = false;
    }
}

bool TextureResource::valid() const {
    return loaded_;
}

Texture2D& TextureResource::get() {
    return texture_;
}

const Texture2D& TextureResource::get() const {
    return texture_;
}

ModelResource::~ModelResource() {
    unload();
}

bool ModelResource::load(const char* path) {
    unload();
    path_ = path != nullptr ? path : "";
    collisionAsset_ = loadGlbCollisionAsset(path);
    model_ = LoadModel(path);
    loaded_ = IsModelValid(model_);
    if (!loaded_) {
        // LoadModel creates a default material even when no mesh could be
        // loaded, so an invalid model can still own heap allocations.
        UnloadModel(model_);
        model_ = {};
        TraceLog(LOG_WARNING, "MODEL: failed to load '%s'",
                 path_.c_str());
    } else {
        for (auto iterator =
                 collisionAsset_.renderMeshIndices.rbegin();
             iterator !=
                 collisionAsset_.renderMeshIndices.rend();
             ++iterator) {
            const std::size_t meshIndex = *iterator;
            if (meshIndex >=
                static_cast<std::size_t>(model_.meshCount)) {
                continue;
            }
            UnloadMesh(model_.meshes[meshIndex]);
            for (int index =
                     static_cast<int>(meshIndex);
                 index + 1 < model_.meshCount; ++index) {
                model_.meshes[index] =
                    model_.meshes[index + 1];
                model_.meshMaterial[index] =
                    model_.meshMaterial[index + 1];
            }
            --model_.meshCount;
        }
        repairMissingGltfTextures(model_, path_);
        const auto finiteVector = [](Vector3 value) {
            return std::isfinite(value.x) &&
                   std::isfinite(value.y) &&
                   std::isfinite(value.z);
        };
        const auto finiteBounds = [&finiteVector](BoundingBox bounds) {
            return finiteVector(bounds.min) &&
                   finiteVector(bounds.max) &&
                   bounds.min.x <= bounds.max.x &&
                   bounds.min.y <= bounds.max.y &&
                   bounds.min.z <= bounds.max.z;
        };
        const float* modelTransformValues = &model_.transform.m0;
        for (int value = 0; value < 16; ++value) {
            if (!std::isfinite(modelTransformValues[value])) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' has a non-finite root transform; "
                         "using identity",
                         path_.c_str());
                model_.transform = MatrixIdentity();
                break;
            }
        }

        meshBounds_.assign(
            static_cast<std::size_t>(std::max(model_.meshCount, 0)), {});
        meshValid_.assign(meshBounds_.size(), false);
        meshHasSkinning_.assign(meshBounds_.size(), false);
        meshSkinningValid_.assign(meshBounds_.size(), true);
        bool boundsInitialized = false;
        bool hasSkinnedMesh = false;
        bool allSkinningValid = true;
        if (model_.skeleton.boneCount > MaximumGpuSkinningBones) {
            TraceLog(LOG_WARNING,
                     "MODEL: '%s' has %d bones; world shader supports "
                     "%d, GPU skinning disabled",
                     path_.c_str(), model_.skeleton.boneCount,
                     MaximumGpuSkinningBones);
            allSkinningValid = false;
        }
        const auto finiteTransform =
            [](const Transform& transform) {
                const auto finiteVector = [](Vector3 value) {
                    return std::isfinite(value.x) &&
                           std::isfinite(value.y) &&
                           std::isfinite(value.z);
                };
                return finiteVector(transform.translation) &&
                       finiteVector(transform.scale) &&
                       std::isfinite(transform.rotation.x) &&
                       std::isfinite(transform.rotation.y) &&
                       std::isfinite(transform.rotation.z) &&
                       std::isfinite(transform.rotation.w);
            };
        if (model_.skeleton.boneCount > 0) {
            if (model_.skeleton.bones == nullptr ||
                model_.skeleton.bindPose == nullptr) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' has an incomplete skeleton; "
                         "GPU skinning disabled",
                         path_.c_str());
                allSkinningValid = false;
            } else {
                for (int bone = 0;
                     bone < model_.skeleton.boneCount; ++bone) {
                    if (!finiteTransform(
                            model_.skeleton.bindPose[bone])) {
                        TraceLog(LOG_WARNING,
                                 "MODEL: '%s' skeleton bind pose has "
                                 "non-finite data at bone %d",
                                 path_.c_str(), bone);
                        allSkinningValid = false;
                        break;
                    }
                }
            }
        }
        for (int index = 0; index < model_.meshCount; ++index) {
            Mesh& mesh = model_.meshes[index];
            bool valid = true;
            if (mesh.vertexCount <= 0 || mesh.triangleCount <= 0 ||
                mesh.vertices == nullptr) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' mesh %d has no renderable vertex "
                         "or triangle data",
                         path_.c_str(), index);
                valid = false;
            }
            const std::size_t vertexCount = mesh.vertexCount > 0
                ? static_cast<std::size_t>(mesh.vertexCount) : 0U;
            const std::size_t triangleCount = mesh.triangleCount > 0
                ? static_cast<std::size_t>(mesh.triangleCount) : 0U;
            if (triangleCount > std::numeric_limits<std::size_t>::max() / 3U) {
                valid = false;
            }
            const std::size_t indexCount = triangleCount <=
                    std::numeric_limits<std::size_t>::max() / 3U
                ? triangleCount * 3U
                : 0U;
            if (mesh.indices != nullptr) {
                for (std::size_t value = 0; value < indexCount; ++value) {
                    if (static_cast<std::size_t>(mesh.indices[value]) >=
                        vertexCount) {
                        TraceLog(LOG_WARNING,
                                 "MODEL: '%s' mesh %d has index %zu >= "
                                 "vertex count %zu",
                                 path_.c_str(), index, value, vertexCount);
                        valid = false;
                        break;
                    }
                }
            } else if (indexCount > vertexCount) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' mesh %d needs %zu non-indexed "
                         "vertices but has %zu",
                         path_.c_str(), index, indexCount, vertexCount);
                valid = false;
            }

            for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
                const float* position = mesh.vertices + vertex * 3U;
                if (!std::isfinite(position[0]) ||
                    !std::isfinite(position[1]) ||
                    !std::isfinite(position[2])) {
                    TraceLog(LOG_WARNING,
                             "MODEL: '%s' mesh %d contains a non-finite "
                             "position at vertex %zu",
                             path_.c_str(), index, vertex);
                    valid = false;
                    break;
                }
            }
            if (mesh.normals != nullptr) {
                for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
                    const float* normal = mesh.normals + vertex * 3U;
                    if (!std::isfinite(normal[0]) ||
                        !std::isfinite(normal[1]) ||
                        !std::isfinite(normal[2])) {
                        TraceLog(LOG_WARNING,
                                 "MODEL: '%s' mesh %d contains a non-finite "
                                 "normal at vertex %zu",
                                 path_.c_str(), index, vertex);
                        valid = false;
                        break;
                    }
                }
            }

            const bool hasSkinData =
                mesh.boneIndices != nullptr || mesh.boneWeights != nullptr ||
                mesh.boneCount > 0;
            meshHasSkinning_[static_cast<std::size_t>(index)] = hasSkinData;
            bool skinningValid = true;
            if (hasSkinData) {
                hasSkinnedMesh = true;
                if (model_.skeleton.boneCount <= 0 ||
                    model_.skeleton.boneCount > MaximumGpuSkinningBones ||
                    model_.skeleton.bones == nullptr ||
                    model_.skeleton.bindPose == nullptr ||
                    mesh.boneIndices == nullptr ||
                    mesh.boneWeights == nullptr) {
                    skinningValid = false;
                } else {
                    for (std::size_t vertex = 0; vertex < vertexCount &&
                         skinningValid; ++vertex) {
                        float weightSum = 0.0F;
                        for (int influence = 0; influence < 4; ++influence) {
                            const std::size_t offset = vertex * 4U +
                                static_cast<std::size_t>(influence);
                            const unsigned int bone =
                                mesh.boneIndices[offset];
                            const float weight = mesh.boneWeights[offset];
                            if (bone >= static_cast<unsigned int>(
                                           model_.skeleton.boneCount) ||
                                !std::isfinite(weight) || weight < 0.0F) {
                                skinningValid = false;
                                break;
                            }
                            weightSum += weight;
                        }
                        if (!std::isfinite(weightSum) ||
                            weightSum <= 0.00001F) {
                            skinningValid = false;
                        }
                    }
                }
                if (!skinningValid) {
                    TraceLog(LOG_WARNING,
                             "MODEL: '%s' mesh %d has invalid skinning; "
                             "GPU skinning disabled for this model",
                             path_.c_str(), index);
                    allSkinningValid = false;
                }
            }
            meshSkinningValid_[static_cast<std::size_t>(index)] =
                skinningValid;

            if (model_.meshMaterial == nullptr ||
                model_.materials == nullptr ||
                model_.meshMaterial[index] < 0 ||
                model_.meshMaterial[index] >= model_.materialCount) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' mesh %d has an invalid material "
                         "index",
                         path_.c_str(), index);
                valid = false;
            } else {
                Material& material = model_.materials[
                    model_.meshMaterial[index]];
                if (material.maps == nullptr) {
                    TraceLog(LOG_WARNING,
                             "MODEL: '%s' material %d has no material maps",
                             path_.c_str(), model_.meshMaterial[index]);
                    valid = false;
                } else {
                    for (int map = 0; map < MaterialMapCount; ++map) {
                        Texture2D& texture = material.maps[map].texture;
                        if (texture.id != 0U && !IsTextureValid(texture)) {
                            TraceLog(LOG_WARNING,
                                     "MODEL: '%s' material %d map %d has "
                                     "an invalid texture handle",
                                     path_.c_str(),
                                     model_.meshMaterial[index], map);
                            texture = {};
                        }
                    }
                }
            }

            const BoundingBox bounds = mesh.vertices != nullptr &&
                    mesh.vertexCount > 0
                ? GetMeshBoundingBox(mesh)
                : BoundingBox{};
            if (!finiteBounds(bounds)) {
                valid = false;
            }
            if (!valid) {
                TraceLog(LOG_WARNING,
                         "MODEL: '%s' mesh %d is not safe to render",
                         path_.c_str(), index);
            } else {
                meshBounds_[static_cast<std::size_t>(index)] = bounds;
            }
            meshValid_[static_cast<std::size_t>(index)] = valid;
            if (valid) {
                if (!boundsInitialized) {
                    visualBounds_ = bounds;
                    boundsInitialized = true;
                } else {
                    visualBounds_.min.x = std::min(
                        visualBounds_.min.x, bounds.min.x);
                    visualBounds_.min.y = std::min(
                        visualBounds_.min.y, bounds.min.y);
                    visualBounds_.min.z = std::min(
                        visualBounds_.min.z, bounds.min.z);
                    visualBounds_.max.x = std::max(
                        visualBounds_.max.x, bounds.max.x);
                    visualBounds_.max.y = std::max(
                        visualBounds_.max.y, bounds.max.y);
                    visualBounds_.max.z = std::max(
                        visualBounds_.max.z, bounds.max.z);
                }
            }
        }
        gpuSkinningCompatible_ = hasSkinnedMesh && allSkinningValid;
    }
    return loaded_;
}

void ModelResource::unload() {
    if (loaded_) {
        UnloadModel(model_);
        model_ = {};
        loaded_ = false;
    }
    collisionAsset_ = {};
    visualBounds_ = {};
    meshBounds_.clear();
    meshValid_.clear();
    meshHasSkinning_.clear();
    meshSkinningValid_.clear();
    gpuSkinningCompatible_ = false;
    runtimeBoneWarningLogged_ = false;
    path_.clear();
}

bool ModelResource::valid() const {
    return loaded_;
}

Model& ModelResource::get() {
    return model_;
}

const Model& ModelResource::get() const {
    return model_;
}

const GlbCollisionAsset&
ModelResource::collisionAsset() const {
    return collisionAsset_;
}

const BoundingBox& ModelResource::visualBounds() const {
    return visualBounds_;
}

std::span<const BoundingBox>
ModelResource::meshBounds() const {
    return meshBounds_;
}

bool ModelResource::meshValid(std::size_t index) const {
    return index < meshValid_.size() && meshValid_[index];
}

bool ModelResource::meshHasSkinning(std::size_t index) const {
    return index < meshHasSkinning_.size() &&
           meshHasSkinning_[index];
}

bool ModelResource::meshSkinningValid(std::size_t index) const {
    return index < meshSkinningValid_.size() &&
           meshSkinningValid_[index];
}

bool ModelResource::gpuSkinningCompatible() const {
    return gpuSkinningCompatible_;
}

bool ModelResource::runtimeBoneMatricesFinite() const {
    if (!gpuSkinningCompatible_ || model_.skeleton.boneCount <= 0) {
        return true;
    }
    const int boneCount = model_.skeleton.boneCount;
    if (model_.boneMatrices == nullptr ||
        boneCount > MaximumGpuSkinningBones) {
        if (!runtimeBoneWarningLogged_) {
            TraceLog(LOG_WARNING,
                     "MODEL: '%s' has no usable runtime bone matrices; "
                     "GPU skinning disabled for this draw",
                     path_.c_str());
            runtimeBoneWarningLogged_ = true;
        }
        return false;
    }
    for (int bone = 0; bone < boneCount; ++bone) {
        const float* values = &model_.boneMatrices[bone].m0;
        for (int value = 0; value < 16; ++value) {
            if (!std::isfinite(values[value])) {
                if (!runtimeBoneWarningLogged_) {
                    TraceLog(LOG_WARNING,
                             "MODEL: '%s' runtime bone matrix %d is "
                             "non-finite; GPU skinning disabled for this "
                             "draw",
                             path_.c_str(), bone);
                    runtimeBoneWarningLogged_ = true;
                }
                return false;
            }
        }
    }
    return true;
}

ModelAnimationsResource::~ModelAnimationsResource() {
    unload();
}

bool ModelAnimationsResource::load(const char* path) {
    unload();
    animations_ = LoadModelAnimations(path, &count_);
    return animations_ != nullptr && count_ > 0;
}

void ModelAnimationsResource::unload() {
    if (animations_ != nullptr) {
        UnloadModelAnimations(animations_, count_);
    }
    animations_ = nullptr;
    count_ = 0;
}

const ModelAnimation* ModelAnimationsResource::find(
    std::string_view name) const {
    for (int index = 0; index < count_; ++index) {
        if (name == animations_[index].name) {
            return &animations_[index];
        }
    }
    return nullptr;
}

RenderTextureResource::~RenderTextureResource() {
    unload();
}

bool RenderTextureResource::load(int width, int height) {
    unload();
    screenSpaceBuffers_ = false;
    if (width <= 0 || height <= 0) {
        return false;
    }

    target_ = LoadRenderTexture(width, height);
    loaded_ = IsRenderTextureValid(target_);
    if (!loaded_) {
        if (target_.id != 0U) {
            UnloadRenderTexture(target_);
        } else {
            if (target_.texture.id != 0U) {
                rlUnloadTexture(target_.texture.id);
            }
            if (target_.depth.id != 0U) {
                rlUnloadTexture(target_.depth.id);
            }
        }
        target_ = {};
    } else {
        SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
        SetTextureWrap(target_.texture, TEXTURE_WRAP_CLAMP);
    }
    return loaded_;
}

bool RenderTextureResource::loadScreenSpace(int width, int height) {
    unload();
    screenSpaceBuffers_ = true;
    if (width <= 0 || height <= 0) {
        screenSpaceBuffers_ = false;
        return false;
    }

    target_.id = rlLoadFramebuffer();
    target_.texture = {
        .id = rlLoadTexture(nullptr, width, height,
                            RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1),
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    normalTexture_ = {
        .id = rlLoadTexture(nullptr, width, height,
                            RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1),
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    target_.depth = {
        .id = rlLoadTextureDepth(width, height, false),
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R32,
    };
    if (target_.id == 0U || target_.texture.id == 0U ||
        normalTexture_.id == 0U || target_.depth.id == 0U) {
        if (target_.id != 0U) {
            rlUnloadFramebuffer(target_.id);
        }
        if (target_.texture.id != 0U) {
            rlUnloadTexture(target_.texture.id);
        }
        if (normalTexture_.id != 0U) {
            rlUnloadTexture(normalTexture_.id);
        }
        if (target_.depth.id != 0U) {
            rlUnloadTexture(target_.depth.id);
        }
        target_ = {};
        normalTexture_ = {};
        screenSpaceBuffers_ = false;
        return false;
    }

    rlFramebufferAttach(target_.id, target_.texture.id,
                        RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target_.id, normalTexture_.id,
                        RL_ATTACHMENT_COLOR_CHANNEL1,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target_.id, target_.depth.id,
                        RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlEnableFramebuffer(target_.id);
    rlActiveDrawBuffers(2);
    rlDisableFramebuffer();
    loaded_ = rlFramebufferComplete(target_.id);
    if (!loaded_) {
        unload();
        return false;
    }
    SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(target_.texture, TEXTURE_WRAP_CLAMP);
    SetTextureFilter(normalTexture_, TEXTURE_FILTER_POINT);
    SetTextureWrap(normalTexture_, TEXTURE_WRAP_CLAMP);
    SetTextureFilter(target_.depth, TEXTURE_FILTER_POINT);
    SetTextureWrap(target_.depth, TEXTURE_WRAP_CLAMP);
    return true;
}

void RenderTextureResource::unload() {
    if (screenSpaceBuffers_) {
        if (normalTexture_.id != 0U) {
            rlUnloadTexture(normalTexture_.id);
        }
        if (target_.texture.id != 0U) {
            rlUnloadTexture(target_.texture.id);
        }
        if (target_.depth.id != 0U) {
            rlUnloadTexture(target_.depth.id);
        }
        if (target_.id != 0U) {
            rlUnloadFramebuffer(target_.id);
        }
        normalTexture_ = {};
        target_ = {};
        screenSpaceBuffers_ = false;
        loaded_ = false;
    } else if (loaded_) {
        UnloadRenderTexture(target_);
        target_ = {};
        loaded_ = false;
    }
}

bool RenderTextureResource::valid() const {
    return loaded_;
}

RenderTexture2D& RenderTextureResource::get() {
    return target_;
}

const RenderTexture2D& RenderTextureResource::get() const {
    return target_;
}

const Texture2D& RenderTextureResource::normalTexture() const {
    return normalTexture_;
}

bool RenderTextureResource::screenSpaceBuffers() const {
    return screenSpaceBuffers_;
}

ShadowMapResource::~ShadowMapResource() {
    unload();
}

bool ShadowMapResource::load(int size) {
    unload();
    if (size <= 0) {
        return false;
    }

    target_.id = rlLoadFramebuffer();
    target_.texture = {
        .id = rlLoadTexture(nullptr, size, size,
                            RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1),
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    target_.depth = {
        .id = rlLoadTextureDepth(size, size, false),
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R32,
    };

    if (target_.id == 0U || target_.texture.id == 0U ||
        target_.depth.id == 0U) {
        if (target_.id != 0U) {
            rlUnloadFramebuffer(target_.id);
        }
        if (target_.texture.id != 0U) {
            rlUnloadTexture(target_.texture.id);
        }
        if (target_.depth.id != 0U) {
            rlUnloadTexture(target_.depth.id);
        }
        target_ = {};
        return false;
    }

    rlFramebufferAttach(target_.id, target_.texture.id,
                        RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target_.id, target_.depth.id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    loaded_ = rlFramebufferComplete(target_.id);
    if (!loaded_) {
        unload();
    }
    return loaded_;
}

void ShadowMapResource::unload() {
    if (target_.id != 0U) {
        rlUnloadFramebuffer(target_.id);
        target_.id = 0U;
        target_.depth.id = 0U;
    } else if (target_.depth.id != 0U) {
        rlUnloadTexture(target_.depth.id);
        target_.depth.id = 0U;
    }
    if (target_.texture.id != 0U) {
        rlUnloadTexture(target_.texture.id);
    }
    target_ = {};
    loaded_ = false;
}

bool ShadowMapResource::valid() const {
    return loaded_;
}

int ShadowMapResource::size() const {
    return target_.texture.width;
}

const RenderTexture2D& ShadowMapResource::target() const {
    return target_;
}

const Texture2D& ShadowMapResource::depthTexture() const {
    return target_.depth;
}

GraphicsResources::~GraphicsResources() {
    shutdown();
}

void GraphicsResources::initialize(const GraphicsSettings& settings) {
    initialized_ = true;
    worldShader_.load("assets/shaders/world.vs", "assets/shaders/world.fs");
    shadowShader_.load("assets/shaders/shadow.vs", "assets/shaders/shadow.fs");
    shadowDebugShader_.load(nullptr, "assets/shaders/shadow_debug.fs");
    skyShader_.load(nullptr, "assets/shaders/sky.fs");
    cloudShader_.load(
        "assets/shaders/cloud.vs", "assets/shaders/cloud.fs");
    waterShader_.load(
        "assets/shaders/water.vs", "assets/shaders/water.fs");
    selectionMaskShader_.load("assets/shaders/selection_mask.vs",
                              "assets/shaders/selection_mask.fs");
    selectionOutlineShader_.load(nullptr,
                                 "assets/shaders/selection_outline.fs");
    postProcessShader_.load(
        nullptr, "assets/shaders/postprocess.fs");
    fxaaShader_.load(nullptr, "assets/shaders/fxaa.fs");
    ssaoShader_.load(nullptr, "assets/shaders/ssao.fs");
    viewModelCompositeShader_.load(
        nullptr, "assets/shaders/viewmodel_composite.fs");
    grassShader_.load("assets/shaders/grass_instanced.vs",
                      "assets/shaders/grass_instanced.fs");
    upgradeEffectShader_.load("assets/shaders/upgrade_effect.vs",
                              "assets/shaders/upgrade_effect.fs");
    iceMagicShader_.load(nullptr, "assets/shaders/ice_magic.fs");
    coinOutlineShader_.load(
        "assets/shaders/coin_outline.vs",
        "assets/shaders/coin_outline.fs");
    heartOutlineShader_.load(
        "assets/shaders/heart_outline.vs",
        "assets/shaders/coin_outline.fs");
    terrainTexture_.load("assets/textures/grass_watercolor.png");
    if (terrainTexture_.valid()) {
        Texture2D& texture = terrainTexture_.get();
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    }
    constexpr std::array<const char*, 3> SkyboxPaths{{
        "assets/textures/skyboxes/day.png",
        "assets/textures/skyboxes/morning.png",
        "assets/textures/skyboxes/night.png",
    }};
    for (std::size_t index = 0; index < SkyboxPaths.size(); ++index) {
        skyboxTextures_[index].load(SkyboxPaths[index]);
        if (skyboxTextures_[index].valid()) {
            Texture2D& texture = skyboxTextures_[index].get();
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
        }
    }
    cannonModel_.load("assets/models/buildings/cannon.glb");
    cannonballModel_.load("assets/models/projectiles/cannonball.glb");
    catapultModel_.load("assets/models/buildings/catapult.glb");
    catapultBallModel_.load("assets/models/projectiles/catapult_ball.glb");
    arrowModel_.load("assets/models/projectiles/arrow.glb");
    sawBladeModel_.load("assets/models/projectiles/saw_blade.glb");
    crossbowModel_.load("assets/models/weapons/crossbow.glb");
    gunTurretModel_.load("assets/models/buildings/gun_turret.glb");
    turretBulletModel_.load("assets/models/projectiles/turret_bullet.glb");
    coreModel_.load("assets/models/buildings/core.glb");
    axeModel_.load("assets/models/tools/axe.glb");
    pickaxeModel_.load("assets/models/tools/pickaxe.glb");
    clubModel_.load("assets/models/tools/club.glb");
    hammerModel_.load("assets/models/tools/hammer.glb");
    iceWandModel_.load("assets/models/weapons/ice_wand/wand.glb");
    woodenChestModel_.load(
        "assets/models/buildings/chests/wooden_chest.glb");
    stoneChestModel_.load(
        "assets/models/buildings/chests/stone_chest.glb");
    challengeColumnModel_.load(
        "assets/models/challenges/skull_column.glb");
    challengeArenaPegModel_.load(
        "assets/models/challenges/arena_peg.glb");
    worldLandmarkModels_[0].load(
        "assets/models/world_landmarks/abandoned_mine.glb");
    worldLandmarkModels_[1].load(
        "assets/models/world_landmarks/abandoned_lumbermill.glb");
    coinModels_[0].load("assets/models/coins/coin-bronze.glb");
    coinModels_[1].load("assets/models/coins/coin-silver.glb");
    coinModels_[2].load("assets/models/coins/coin-high-value.glb");
    destructiblePropModels_[0].load(
        "assets/models/destructibles/barrel/barrel.glb");
    destructiblePropModels_[1].load(
        "assets/models/destructibles/crates/crate.glb");
    destructiblePropModels_[2].load(
        "assets/models/destructibles/crates/crate-item.glb");
    appleLootModel_.load("assets/models/items/apple.glb");
    breadLootModel_.load("assets/models/items/bread.glb");
    ironBarLootModel_.load(
        "assets/models/items/iron_bar.glb");
    fuelJerrycanLootModel_.load(
        "assets/models/items/fuel_jerrycan.glb");
    compassLootModel_.load(
        "assets/models/items/compass.glb");
    nailLootModel_.load(
        "assets/models/items/nail.glb");
    keyLootModel_.load(
        "assets/models/items/key.glb");
    mapLootModel_.load(
        "assets/models/items/map.glb");
    anvilLootModel_.load(
        "assets/models/items/anvil.glb");
    sawLootModel_.load(
        "assets/models/items/saw.glb");
    potionLootModel_.load(
        "assets/models/items/potion.glb");
    blueprintLootModel_.load(
        "assets/models/items/blueprint.glb");
    hourglassLootModel_.load(
        "assets/models/items/hourglass.glb");
    ropeLootModel_.load(
        "assets/models/items/rope.glb");
    heartLootModel_.load(
        "assets/models/items/heart.glb");
    platformModel_.load("assets/models/construction/platform.glb");
    rampModel_.load("assets/models/construction/ramp.glb");
    mineModel_.load("assets/models/buildings/mine.glb");
    lumberMillModel_.load(
        "assets/models/buildings/lumber_mill.glb");
    quarryModel_.load("assets/models/buildings/quarry.glb");
    spikeTrapModel_.load(
        "assets/models/traps/spike_trap.glb");
    rockModels_[0].load("assets/models/environment/stone_1.glb");
    rockModels_[1].load("assets/models/environment/stone_2.glb");
    rockModels_[2].load("assets/models/environment/stone_3.glb");
    crystalResourceModel_.load(
        "assets/models/environment/crystal.glb");
    treeModels_[0].load("assets/models/environment/tree_1_a.glb");
    treeModels_[1].load("assets/models/environment/tree_1_b.glb");
    treeModels_[2].load("assets/models/environment/tree_1_c.glb");
    treeModels_[3].load("assets/models/environment/tree_2_a.glb");
    treeModels_[4].load("assets/models/environment/tree_2_b.glb");
    treeModels_[5].load("assets/models/environment/tree_2_c.glb");
    treeModels_[6].load("assets/models/environment/tree_3_a.glb");
    treeModels_[7].load("assets/models/environment/tree_3_b.glb");
    treeModels_[8].load("assets/models/environment/tree_3_c.glb");
    boundaryTreeModels_[0].load(
        "assets/models/environment/boundary_forest/tree.glb");
    boundaryTreeModels_[1].load(
        "assets/models/environment/boundary_forest/tree_high.glb");
    decorativeRockModels_[0].load(
        "assets/models/environment/decor/rock_small_a.glb");
    decorativeRockModels_[1].load(
        "assets/models/environment/decor/rock_small_b.glb");
    decorativeRockModels_[2].load(
        "assets/models/environment/decor/rock_small_c.glb");
    decorativeRockModels_[3].load(
        "assets/models/environment/decor/rock_small_d.glb");
    decorativeBushModels_[0].load(
        "assets/models/environment/decor/bushes/a.glb");
    decorativeBushModels_[1].load(
        "assets/models/environment/decor/bushes/b.glb");
    decorativeBushModels_[2].load(
        "assets/models/environment/decor/bushes/c.glb");
    decorativeBushModels_[3].load(
        "assets/models/environment/decor/bushes/d.glb");
    decorativeBushModels_[4].load(
        "assets/models/environment/decor/bushes/e.glb");
    decorativeBushModels_[5].load(
        "assets/models/environment/decor/bushes/f.glb");
    decorativeBushModels_[6].load(
        "assets/models/environment/decor/plants/flowers.glb");
    decorativeBushModels_[7].load(
        "assets/models/environment/decor/plants/flowers-tall.glb");
    decorativeBushModels_[8].load(
        "assets/models/environment/decor/plants/plant.glb");
    pondDecorModels_[0].load(
        "assets/models/environment/decor/water/waterlily_A.gltf");
    pondDecorModels_[1].load(
        "assets/models/environment/decor/water/waterlily_B.gltf");
    pondDecorModels_[2].load(
        "assets/models/environment/decor/water/waterplant_A.gltf");
    pondDecorModels_[3].load(
        "assets/models/environment/decor/water/waterplant_B.gltf");
    pondDecorModels_[4].load(
        "assets/models/environment/decor/water/waterplant_C.gltf");
    cloudModels_[0].load("assets/models/environment/clouds/small.glb");
    cloudModels_[1].load("assets/models/environment/clouds/big.glb");
    wallIsolatedModel_.load(
        "assets/models/construction/walls/isolated.glb");
    wallEndModel_.load("assets/models/construction/walls/end.glb");
    wallCornerModel_.load("assets/models/construction/walls/corner.glb");
    wallTModel_.load("assets/models/construction/walls/t.glb");
    wallCrossModel_.load("assets/models/construction/walls/cross.glb");
    grassModelB_.load(
        "assets/models/environment/grass/Grass_2_B_Singlesided_Color1.gltf");
    grassModelC_.load(
        "assets/models/environment/grass/Grass_2_C_Singlesided_Color1.gltf");
    grassModelD_.load(
        "assets/models/environment/grass/Grass_2_D_Singlesided_Color1.gltf");
    enemyMinionModel_.load(
        "assets/models/enemies/pink_blob.gltf");
    enemyRogueModel_.load(
        "assets/models/enemies/ultimate/ninja.gltf");
    enemyWarriorModel_.load(
        "assets/models/enemies/ultimate/mushnub_evolved.gltf");
    enemyMageModel_.load(
        "assets/models/enemies/mage.glb");
    enemySapperModel_.load(
        "assets/models/enemies/ultimate/sapper.gltf");
    enemyFlyingModel_.load(
        "assets/models/enemies/ultimate/flying.gltf");
    enemyBossModel_.load(
        "assets/models/enemies/ultimate/boss.gltf");
    enemySplitterModel_.load(
        "assets/models/enemies/ultimate/green_spiky_blob.gltf");
    enemySplitlingModel_.load(
        "assets/models/enemies/ultimate/green_blob.gltf");
    enemyGeneralAnimations_.load(
        "assets/models/enemies/animations/general.glb");
    enemyPinkBlobAnimations_.load(
        "assets/models/enemies/pink_blob.gltf");
    enemyNinjaAnimations_.load(
        "assets/models/enemies/ultimate/ninja.gltf");
    enemyMushroomKingAnimations_.load(
        "assets/models/enemies/ultimate/mushnub_evolved.gltf");
    enemySplitterAnimations_.load(
        "assets/models/enemies/ultimate/green_spiky_blob.gltf");
    enemySplitlingAnimations_.load(
        "assets/models/enemies/ultimate/green_blob.gltf");
    enemyMovementAnimations_.load(
        "assets/models/enemies/animations/movement.glb");
    enemySapperAnimations_.load(
        "assets/models/enemies/ultimate/sapper.gltf");
    enemyFlyingAnimations_.load(
        "assets/models/enemies/ultimate/flying.gltf");
    enemyBossAnimations_.load(
        "assets/models/enemies/ultimate/boss.gltf");
    updateFramebuffer(settings);
    updateViewModelTarget();
    updateSelectionMask(settings);
    updateShadowMap(settings);
}

void GraphicsResources::updateViewModelTarget() {
    if (!initialized_) {
        return;
    }
    const int width = GetRenderWidth();
    const int height = GetRenderHeight();
    if (width <= 0 || height <= 0 ||
        (width == requestedViewModelWidth_ &&
         height == requestedViewModelHeight_)) {
        return;
    }
    requestedViewModelWidth_ = width;
    requestedViewModelHeight_ = height;
    viewModelTarget_.load(width, height);
    if (viewModelTarget_.valid()) {
        SetTextureFilter(viewModelTarget_.get().texture,
                         TEXTURE_FILTER_BILINEAR);
    }
}

void GraphicsResources::updateFramebuffer(const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }

    if (!settings.postProcessing) {
        sceneTarget_.unload();
        postProcessTarget_.unload();
        ssaoTarget_.unload();
        requestedSceneWidth_ = 0;
        requestedSceneHeight_ = 0;
        requestedSsaoWidth_ = 0;
        requestedSsaoHeight_ = 0;
        return;
    }

    const int framebufferWidth = GetRenderWidth();
    const int framebufferHeight = GetRenderHeight();
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    const int pixelSize = std::clamp(settings.pixelSize, 1, 8);
    const int desiredWidth =
        std::max(1, (framebufferWidth + pixelSize - 1) / pixelSize);
    const int desiredHeight =
        std::max(1, (framebufferHeight + pixelSize - 1) / pixelSize);
    const bool needsScreenSpaceBuffers = settings.ssao;
    if (desiredWidth != requestedSceneWidth_ ||
        desiredHeight != requestedSceneHeight_ ||
        !sceneTarget_.valid() ||
        sceneTarget_.screenSpaceBuffers() != needsScreenSpaceBuffers) {
        requestedSceneWidth_ = desiredWidth;
        requestedSceneHeight_ = desiredHeight;
        if (needsScreenSpaceBuffers) {
            sceneTarget_.loadScreenSpace(desiredWidth, desiredHeight);
        } else {
            sceneTarget_.load(desiredWidth, desiredHeight);
        }
    }
    if (!postProcessTarget_.valid() ||
        postProcessTarget_.get().texture.width != desiredWidth ||
        postProcessTarget_.get().texture.height != desiredHeight) {
        postProcessTarget_.load(desiredWidth, desiredHeight);
        if (postProcessTarget_.valid()) {
            SetTextureFilter(postProcessTarget_.get().texture,
                             TEXTURE_FILTER_BILINEAR);
        }
    }

    if (!settings.ssao || !sceneTarget_.valid()) {
        ssaoTarget_.unload();
        requestedSsaoWidth_ = 0;
        requestedSsaoHeight_ = 0;
        return;
    }
    const int divisor = settings.quality == GraphicsQuality::Low ? 4 : 2;
    const int ssaoWidth = std::max(1, (desiredWidth + divisor - 1) / divisor);
    const int ssaoHeight = std::max(1, (desiredHeight + divisor - 1) / divisor);
    if (ssaoWidth == requestedSsaoWidth_ &&
        ssaoHeight == requestedSsaoHeight_ &&
        ssaoTarget_.valid()) {
        return;
    }
    requestedSsaoWidth_ = ssaoWidth;
    requestedSsaoHeight_ = ssaoHeight;
    ssaoTarget_.load(ssaoWidth, ssaoHeight);
    if (ssaoTarget_.valid()) {
        SetTextureFilter(ssaoTarget_.get().texture,
                         TEXTURE_FILTER_BILINEAR);
    }
}

void GraphicsResources::updateSelectionMask(
    const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }

    const bool useSceneTarget =
        settings.postProcessing && sceneTarget_.valid();
    const int desiredWidth =
        useSceneTarget ? requestedSceneWidth_ : GetScreenWidth();
    const int desiredHeight =
        useSceneTarget ? requestedSceneHeight_ : GetScreenHeight();
    if (desiredWidth <= 0 || desiredHeight <= 0 ||
        (desiredWidth == requestedSelectionMaskWidth_ &&
         desiredHeight == requestedSelectionMaskHeight_)) {
        return;
    }

    requestedSelectionMaskWidth_ = desiredWidth;
    requestedSelectionMaskHeight_ = desiredHeight;
    selectionMaskTarget_.load(desiredWidth, desiredHeight);
}

void GraphicsResources::updateShadowMap(const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }
    if (!settings.shadows) {
        shadowMap_.unload();
        requestedShadowMapSize_ = 0;
        return;
    }

    const int desiredSize = std::clamp(settings.shadowMapSize, 512, 2048);
    if (desiredSize == requestedShadowMapSize_) {
        return;
    }
    requestedShadowMapSize_ = desiredSize;
    shadowMap_.load(desiredSize);
}

void GraphicsResources::shutdown() {
    viewModelTarget_.unload();
    ssaoTarget_.unload();
    shadowMap_.unload();
    enemyBossAnimations_.unload();
    enemySplitlingAnimations_.unload();
    enemySplitterAnimations_.unload();
    enemyFlyingAnimations_.unload();
    enemySapperAnimations_.unload();
    enemyMovementAnimations_.unload();
    enemyMushroomKingAnimations_.unload();
    enemyNinjaAnimations_.unload();
    enemyPinkBlobAnimations_.unload();
    enemyGeneralAnimations_.unload();
    enemyBossModel_.unload();
    enemySplitlingModel_.unload();
    enemySplitterModel_.unload();
    enemyFlyingModel_.unload();
    enemySapperModel_.unload();
    enemyMageModel_.unload();
    enemyWarriorModel_.unload();
    enemyRogueModel_.unload();
    enemyMinionModel_.unload();
    grassModelD_.unload();
    grassModelC_.unload();
    grassModelB_.unload();
    wallCrossModel_.unload();
    wallTModel_.unload();
    wallCornerModel_.unload();
    wallEndModel_.unload();
    wallIsolatedModel_.unload();
    for (auto& treeModel : treeModels_) {
        treeModel.unload();
    }
    for (auto& treeModel : boundaryTreeModels_) {
        treeModel.unload();
    }
    for (auto& rockModel : decorativeRockModels_) {
        rockModel.unload();
    }
    for (auto& bushModel : decorativeBushModels_) {
        bushModel.unload();
    }
    for (auto& pondDecorModel : pondDecorModels_) {
        pondDecorModel.unload();
    }
    for (auto& cloudModel : cloudModels_) {
        cloudModel.unload();
    }
    for (auto& rockModel : rockModels_) {
        rockModel.unload();
    }
    crystalResourceModel_.unload();
    quarryModel_.unload();
    spikeTrapModel_.unload();
    lumberMillModel_.unload();
    mineModel_.unload();
    rampModel_.unload();
    platformModel_.unload();
    hammerModel_.unload();
    iceWandModel_.unload();
    clubModel_.unload();
    stoneChestModel_.unload();
    challengeColumnModel_.unload();
    challengeArenaPegModel_.unload();
    for (auto& model : worldLandmarkModels_) model.unload();
    for (auto& model : destructiblePropModels_) model.unload();
    for (auto& model : coinModels_) model.unload();
    woodenChestModel_.unload();
    heartLootModel_.unload();
    ropeLootModel_.unload();
    hourglassLootModel_.unload();
    blueprintLootModel_.unload();
    potionLootModel_.unload();
    sawLootModel_.unload();
    anvilLootModel_.unload();
    mapLootModel_.unload();
    keyLootModel_.unload();
    nailLootModel_.unload();
    compassLootModel_.unload();
    fuelJerrycanLootModel_.unload();
    ironBarLootModel_.unload();
    breadLootModel_.unload();
    appleLootModel_.unload();
    pickaxeModel_.unload();
    axeModel_.unload();
    coreModel_.unload();
    crossbowModel_.unload();
    gunTurretModel_.unload();
    turretBulletModel_.unload();
    sawBladeModel_.unload();
    arrowModel_.unload();
    cannonballModel_.unload();
    cannonModel_.unload();
    catapultBallModel_.unload();
    catapultModel_.unload();
    placeholderModel_.unload();
    terrainTexture_.unload();
    for (auto& texture : skyboxTextures_) texture.unload();
    fallbackTexture_.unload();
    upgradeEffectShader_.unload();
    iceMagicShader_.unload();
    coinOutlineShader_.unload();
    heartOutlineShader_.unload();
    grassShader_.unload();
    ssaoShader_.unload();
    fxaaShader_.unload();
    postProcessShader_.unload();
    viewModelCompositeShader_.unload();
    selectionOutlineShader_.unload();
    selectionMaskShader_.unload();
    skyShader_.unload();
    cloudShader_.unload();
    waterShader_.unload();
    shadowDebugShader_.unload();
    shadowShader_.unload();
    worldShader_.unload();
    sceneTarget_.unload();
    postProcessTarget_.unload();
    selectionMaskTarget_.unload();
    requestedSceneWidth_ = 0;
    requestedSceneHeight_ = 0;
    requestedSsaoWidth_ = 0;
    requestedSsaoHeight_ = 0;
    requestedViewModelWidth_ = 0;
    requestedViewModelHeight_ = 0;
    requestedSelectionMaskWidth_ = 0;
    requestedSelectionMaskHeight_ = 0;
    requestedShadowMapSize_ = 0;
    initialized_ = false;
}

} // namespace ian
