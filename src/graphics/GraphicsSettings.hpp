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
    bool paletteQuantization{false};
    bool dithering{false};
    bool posterizedLighting{false};
    bool inkOutlines{false};
    bool fogBands{false};
    bool paperGrain{false};

    int shadowMapSize{2048};

    int pixelSize{3};
    float shadowDistance{80.0F};
    float constantBias{0.00003F};
    float slopeBias{0.0002F};
    float shadowStrength{0.58F};
    float aoStrength{0.3F};

    float postExposure{0.04F};
    float brightness{};
    float contrast{0.96F};
    float colorSaturation{0.9F};
    float hueDegrees{};
    float temperature{0.025F};
    float tint{};
    float gamma{1.02F};
    float blackPoint{};
    float curveShadows{0.1F};
    float curveMidtones{0.015F};
    float curveHighlights{-0.08F};
    float sharpness{0.04F};
    float vignette{0.07F};
    float paletteLevels{8.0F};
    float ditherStrength{0.35F};
    float lightingSteps{5.0F};
    float bloomStrength{0.28F};
    float outlineStrength{0.35F};
    float fogBandCount{5.0F};
    float paperGrainStrength{0.035F};

    GraphicsQuality quality{GraphicsQuality::High};
};

} // namespace ian
