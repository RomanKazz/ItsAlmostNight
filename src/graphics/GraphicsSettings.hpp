#pragma once

namespace ian {

enum class GraphicsQuality {
    Low,
    Medium,
    High,
};

struct GraphicsSettings {
    bool sky{true};
    bool worldShader{true};
    bool shadows{true};
    bool fog{true};
    bool postProcessing{false};
    bool particles{true};
    bool blobShadows{true};
    bool bloom{false};
    bool ssao{false};

    int shadowMapSize{2048};

    float renderScale{1.0F};
    float shadowDistance{80.0F};
    float constantBias{0.0012F};
    float slopeBias{0.002F};
    float shadowStrength{0.75F};
    float aoStrength{0.30F};
    GraphicsQuality quality{GraphicsQuality::High};
};

} // namespace ian
