#pragma once

#include <raylib.h>

#include <string_view>
#include <vector>

namespace ian {

class GlbNodeAnimation {
  public:
    [[nodiscard]] bool load(std::string_view path,
                            std::string_view nodeName);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] Transform sample(float normalizedTime) const;
    [[nodiscard]] const Transform& baseTransform() const;

  private:
    struct VectorTrack {
        std::vector<float> times;
        std::vector<Vector3> values;
        bool step{};
    };

    struct RotationTrack {
        std::vector<float> times;
        std::vector<Quaternion> values;
        bool step{};
    };

    Transform base_{{}, {0.0F, 0.0F, 0.0F, 1.0F},
                    {1.0F, 1.0F, 1.0F}};
    VectorTrack translation_;
    RotationTrack rotation_;
    VectorTrack scale_;
    float startTime_{};
    float endTime_{};
    bool loaded_{};
};

} // namespace ian
