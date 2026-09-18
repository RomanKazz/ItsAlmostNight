#include "graphics/GlbNodeAnimation.hpp"

#include <nlohmann/json.hpp>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace ian {
namespace {

using Json = nlohmann::json;

constexpr std::uint32_t GlbMagic = 0x46546c67U;
constexpr std::uint32_t JsonChunk = 0x4e4f534aU;
constexpr std::uint32_t BinaryChunk = 0x004e4942U;

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset) {
    if (offset + sizeof(std::uint32_t) > bytes.size()) return 0U;
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

template <std::size_t ComponentCount>
bool readFloatAccessor(const Json& root,
                       const std::vector<std::uint8_t>& binary,
                       int accessorIndex,
                       std::vector<std::array<float, ComponentCount>>& output) {
    if (accessorIndex < 0 ||
        !root.contains("accessors") ||
        static_cast<std::size_t>(accessorIndex) >=
            root.at("accessors").size()) {
        return false;
    }
    const Json& accessor = root.at("accessors").at(
        static_cast<std::size_t>(accessorIndex));
    if (accessor.value("componentType", 0) != 5126 ||
        !accessor.contains("bufferView") || accessor.contains("sparse")) {
        return false;
    }
    const int viewIndex = accessor.at("bufferView").get<int>();
    if (!root.contains("bufferViews") || viewIndex < 0 ||
        static_cast<std::size_t>(viewIndex) >=
            root.at("bufferViews").size()) {
        return false;
    }
    const Json& view = root.at("bufferViews").at(
        static_cast<std::size_t>(viewIndex));
    if (view.value("buffer", 0) != 0) return false;

    const std::size_t count = accessor.value("count", 0U);
    const std::size_t elementBytes = ComponentCount * sizeof(float);
    const std::size_t stride = view.value("byteStride", elementBytes);
    const std::size_t offset = view.value("byteOffset", 0U) +
        accessor.value("byteOffset", 0U);
    if (stride < elementBytes ||
        (count > 0U && offset + (count - 1U) * stride + elementBytes >
                           binary.size())) {
        return false;
    }

    output.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        std::memcpy(output[index].data(),
                    binary.data() + offset + index * stride,
                    elementBytes);
    }
    return true;
}

Vector3 jsonVector3(const Json& node, const char* key,
                    Vector3 fallback) {
    const auto iterator = node.find(key);
    if (iterator == node.end() || !iterator->is_array() ||
        iterator->size() != 3U) {
        return fallback;
    }
    return {iterator->at(0).get<float>(), iterator->at(1).get<float>(),
            iterator->at(2).get<float>()};
}

Quaternion jsonQuaternion(const Json& node, const char* key,
                          Quaternion fallback) {
    const auto iterator = node.find(key);
    if (iterator == node.end() || !iterator->is_array() ||
        iterator->size() != 4U) {
        return fallback;
    }
    return QuaternionNormalize({
        iterator->at(0).get<float>(), iterator->at(1).get<float>(),
        iterator->at(2).get<float>(), iterator->at(3).get<float>()});
}

template <typename Value, typename Interpolate>
Value sampleTrack(const std::vector<float>& times,
                  const std::vector<Value>& values, bool step,
                  float time, Value fallback, Interpolate interpolate) {
    if (times.empty() || times.size() != values.size()) return fallback;
    if (time <= times.front()) return values.front();
    if (time >= times.back()) return values.back();
    const auto upper = std::upper_bound(times.begin(), times.end(), time);
    const std::size_t upperIndex =
        static_cast<std::size_t>(std::distance(times.begin(), upper));
    const std::size_t lowerIndex = upperIndex - 1U;
    if (step) return values[lowerIndex];
    const float span = times[upperIndex] - times[lowerIndex];
    const float amount = span > 0.000001F
        ? (time - times[lowerIndex]) / span
        : 0.0F;
    return interpolate(values[lowerIndex], values[upperIndex], amount);
}

} // namespace

bool GlbNodeAnimation::load(std::string_view path,
                            std::string_view nodeName) {
    unload();
    std::ifstream stream(std::string(path), std::ios::binary);
    if (!stream) return false;
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    if (bytes.size() < 20U || readU32(bytes, 0U) != GlbMagic) return false;

    std::string jsonText;
    std::vector<std::uint8_t> binary;
    std::size_t cursor = 12U;
    while (cursor + 8U <= bytes.size()) {
        const std::size_t length = readU32(bytes, cursor);
        const std::uint32_t type = readU32(bytes, cursor + 4U);
        cursor += 8U;
        if (cursor + length > bytes.size()) return false;
        if (type == JsonChunk) {
            jsonText.assign(reinterpret_cast<const char*>(bytes.data() + cursor),
                            length);
        } else if (type == BinaryChunk) {
            binary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                          bytes.begin() + static_cast<std::ptrdiff_t>(cursor + length));
        }
        cursor += length;
    }
    if (jsonText.empty() || binary.empty()) return false;

    try {
        const Json root = Json::parse(jsonText);
        if (!root.contains("nodes") || !root.contains("animations")) {
            return false;
        }
        int nodeIndex = -1;
        for (std::size_t index = 0; index < root.at("nodes").size(); ++index) {
            if (root.at("nodes").at(index).value("name", std::string{}) ==
                nodeName) {
                nodeIndex = static_cast<int>(index);
                break;
            }
        }
        if (nodeIndex < 0) return false;
        const Json& node = root.at("nodes").at(
            static_cast<std::size_t>(nodeIndex));
        base_.translation = jsonVector3(node, "translation", {});
        base_.rotation = jsonQuaternion(
            node, "rotation", {0.0F, 0.0F, 0.0F, 1.0F});
        base_.scale = jsonVector3(node, "scale", {1.0F, 1.0F, 1.0F});

        const Json& animation = root.at("animations").front();
        for (const Json& channel : animation.at("channels")) {
            const Json& target = channel.at("target");
            if (target.value("node", -1) != nodeIndex) continue;
            const int samplerIndex = channel.value("sampler", -1);
            if (samplerIndex < 0 ||
                static_cast<std::size_t>(samplerIndex) >=
                    animation.at("samplers").size()) {
                continue;
            }
            const Json& sampler = animation.at("samplers").at(
                static_cast<std::size_t>(samplerIndex));
            std::vector<std::array<float, 1>> timeValues;
            if (!readFloatAccessor<1>(root, binary,
                                      sampler.value("input", -1),
                                      timeValues)) {
                continue;
            }
            std::vector<float> times;
            times.reserve(timeValues.size());
            for (const auto& value : timeValues) times.push_back(value[0]);
            const bool step = sampler.value("interpolation", "LINEAR") ==
                "STEP";
            const std::string targetPath = target.value("path", "");
            if (targetPath == "rotation") {
                std::vector<std::array<float, 4>> values;
                if (!readFloatAccessor<4>(root, binary,
                                          sampler.value("output", -1),
                                          values)) {
                    continue;
                }
                rotation_.times = std::move(times);
                rotation_.step = step;
                rotation_.values.reserve(values.size());
                for (const auto& value : values) {
                    rotation_.values.push_back(QuaternionNormalize(
                        {value[0], value[1], value[2], value[3]}));
                }
            } else if (targetPath == "translation" ||
                       targetPath == "scale") {
                std::vector<std::array<float, 3>> values;
                if (!readFloatAccessor<3>(root, binary,
                                          sampler.value("output", -1),
                                          values)) {
                    continue;
                }
                VectorTrack& track = targetPath == "translation"
                    ? translation_ : scale_;
                track.times = std::move(times);
                track.step = step;
                track.values.reserve(values.size());
                for (const auto& value : values) {
                    track.values.push_back({value[0], value[1], value[2]});
                }
            }
        }

        startTime_ = std::numeric_limits<float>::infinity();
        endTime_ = -std::numeric_limits<float>::infinity();
        const auto includeTimes = [this](const std::vector<float>& times) {
            if (times.empty()) return;
            startTime_ = std::min(startTime_, times.front());
            endTime_ = std::max(endTime_, times.back());
        };
        includeTimes(translation_.times);
        includeTimes(rotation_.times);
        includeTimes(scale_.times);
        loaded_ = std::isfinite(startTime_) && std::isfinite(endTime_) &&
            endTime_ >= startTime_ &&
            (!translation_.times.empty() || !rotation_.times.empty() ||
             !scale_.times.empty());
    } catch (const Json::exception&) {
        unload();
    }
    return loaded_;
}

void GlbNodeAnimation::unload() {
    translation_ = {};
    rotation_ = {};
    scale_ = {};
    base_ = {{}, {0.0F, 0.0F, 0.0F, 1.0F},
             {1.0F, 1.0F, 1.0F}};
    startTime_ = 0.0F;
    endTime_ = 0.0F;
    loaded_ = false;
}

bool GlbNodeAnimation::valid() const {
    return loaded_;
}

Transform GlbNodeAnimation::sample(float normalizedTime) const {
    if (!loaded_) return base_;
    // glTF key times are absolute within the clip. Blender's sampled export
    // starts at frame 1 (1/60 s), so normalized 20/60 must sample frame 20,
    // rather than frame 20 plus the first-key offset.
    const float time =
        std::clamp(normalizedTime, 0.0F, 1.0F) * endTime_;
    Transform result = base_;
    result.translation = sampleTrack(
        translation_.times, translation_.values, translation_.step, time,
        base_.translation,
        [](Vector3 a, Vector3 b, float amount) {
            return Vector3Lerp(a, b, amount);
        });
    result.rotation = sampleTrack(
        rotation_.times, rotation_.values, rotation_.step, time,
        base_.rotation,
        [](Quaternion a, Quaternion b, float amount) {
            return QuaternionSlerp(a, b, amount);
        });
    result.scale = sampleTrack(
        scale_.times, scale_.values, scale_.step, time, base_.scale,
        [](Vector3 a, Vector3 b, float amount) {
            return Vector3Lerp(a, b, amount);
        });
    return result;
}

const Transform& GlbNodeAnimation::baseTransform() const {
    return base_;
}

} // namespace ian
