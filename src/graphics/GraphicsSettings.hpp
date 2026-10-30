#pragma once

namespace ian {

enum class GraphicsQuality {
    Low,
    Medium,
    High,
};

struct GraphicsSettings {
    bool fullscreen{false};
    bool sky{true};
    bool worldShader{true};
    bool shadows{true};
    bool fog{true};
    bool postProcessing{true};
    bool particles{true};
    bool grass{true};
    bool enemyLod{false};
    bool blobShadows{true};
    bool bloom{true};
    bool ssao{true};
    bool paletteQuantization{false};
    bool dithering{false};
    bool posterizedLighting{false};
    bool inkOutlines{true};
    bool fogBands{false};
    bool paperGrain{false};
    bool performanceLogging{false};

    // Zero selects the fitted desktop/window size. Explicit values are output pixels.
    int displayWidth{};
    int displayHeight{};

    // Internal world-render scale, independent from the output resolution.
    int renderWidth{1920};
    int renderHeight{1080};
    int shadowMapSize{2048};
    int frameRateLimit{60};

    int pixelSize{1};
    float shadowDistance{55.0F};
    float constantBias{0.00003F};
    float slopeBias{0.0002F};
    float shadowStrength{0.58F};
    float aoStrength{0.35F};

    float postExposure{};
    float brightness{};
    float contrast{1.033F};
    float colorSaturation{1.042F};
    float hueDegrees{0.285F};
    float temperature{0.040F};
    float tint{0.036F};
    float gamma{1.0F};
    float blackPoint{0.100F};
    float curveShadows{0.282F};
    float curveMidtones{0.021F};
    float curveHighlights{0.179F};
    float sharpness{};
    float vignette{0.014F};
    float paletteLevels{8.0F};
    float ditherStrength{0.35F};
    float lightingSteps{2.051F};
    float bloomStrength{0.28F};
    float outlineStrength{0.972F};
    float outlineWidth{1.0F};
    float outlineFogFadeStrength{0.75F};
    float fogBandCount{5.0F};
    float paperGrainStrength{0.035F};

    GraphicsQuality quality{GraphicsQuality::High};

    bool operator==(const GraphicsSettings&) const = default;
};

} // namespace ian
