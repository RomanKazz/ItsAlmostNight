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
    bool postProcessing{true};
    bool particles{true};
    bool grass{true};
    bool blobShadows{true};
    bool bloom{false};
    bool ssao{false};

    int shadowMapSize{2048};

    int pixelSize{3};
    float shadowDistance{80.0F};
    float constantBias{0.00003F};
    float slopeBias{0.0002F};
    float shadowStrength{0.75F};
    float aoStrength{0.25F};

    float postExposure{};
    float brightness{};
    float contrast{1.0F};
    float colorSaturation{1.0F};
    float hueDegrees{};
    float temperature{};
    float tint{};
    float gamma{1.0F};
    float blackPoint{};
    float curveShadows{};
    float curveMidtones{};
    float curveHighlights{};
    float sharpness{};
    float vignette{};

    GraphicsQuality quality{GraphicsQuality::High};
};

} // namespace ian
