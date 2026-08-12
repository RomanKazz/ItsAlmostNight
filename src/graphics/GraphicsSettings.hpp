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
    bool ssao{true};
    bool paletteQuantization{false};
    bool dithering{false};
    bool posterizedLighting{true};
    bool inkOutlines{false};
    bool fogBands{false};
    bool paperGrain{false};

    int shadowMapSize{2048};
    int frameRateLimit{60};

    int pixelSize{1};
    float shadowDistance{55.0F};
    float constantBias{0.00003F};
    float slopeBias{0.0002F};
    float shadowStrength{0.58F};
    float aoStrength{0.3F};

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
    float paletteLevels{8.0F};
    float ditherStrength{0.35F};
    float lightingSteps{5.0F};
    float bloomStrength{0.28F};
    float outlineStrength{0.35F};
    float outlineWidth{1.0F};
    float fogBandCount{5.0F};
    float paperGrainStrength{0.035F};

    GraphicsQuality quality{GraphicsQuality::High};

    bool operator==(const GraphicsSettings&) const = default;
};

} // namespace ian
