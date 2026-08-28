#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D ssaoTexture;
uniform mat4 inverseProjection;
uniform vec2 ssaoTexelSize;
uniform float ssaoEnabled;
uniform float ssaoStrength;
uniform float postExposure;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float hueDegrees;
uniform float temperature;
uniform float tint;
uniform float gammaValue;
uniform float blackPoint;
uniform float curveShadows;
uniform float curveMidtones;
uniform float curveHighlights;
uniform float sharpness;
uniform float vignette;
uniform float lowHealthAmount;
uniform float lowHealthTime;
uniform float lowHealthPulse;
uniform float paletteEnabled;
uniform float paletteLevels;
uniform float ditherEnabled;
uniform float ditherStrength;
uniform float bloomEnabled;
uniform float bloomStrength;
uniform float inkOutlinesEnabled;
uniform float outlineStrength;
uniform float outlineWidth;
uniform float paperGrainEnabled;
uniform float paperGrainStrength;
uniform float menuDofEnabled;
uniform float menuDofFocusDistance;
uniform float menuDofFocusRange;
uniform float menuDofMaximumBlurPixels;

out vec4 finalColor;

const vec3 LuminanceWeights =
    vec3(0.2126, 0.7152, 0.0722);

float luminanceOf(vec3 color)
{
    return dot(color, LuminanceWeights);
}

vec3 reconstructViewPosition(vec2 uv, float depth)
{
    vec4 clip = vec4(uv*2.0 - 1.0, depth*2.0 - 1.0, 1.0);
    vec4 view = inverseProjection*clip;
    return view.xyz/max(view.w, 0.000001);
}

vec3 decodeOctahedralNormal(vec2 encoded)
{
    vec2 octahedron = encoded*2.0 - 1.0;
    vec3 normal = vec3(
        octahedron.x, octahedron.y,
        1.0 - abs(octahedron.x) - abs(octahedron.y));
    if (normal.z < 0.0)
    {
        normal.xy =
            (1.0 - abs(normal.yx))*sign(normal.xy);
    }
    return normalize(normal);
}

float materialClassOf(vec4 packedNormal)
{
    return floor(packedNormal.b*4.0 + 0.01);
}

float screenAoMaskOf(vec3 packedNormal)
{
    float materialClass = floor(packedNormal.b*4.0 + 0.01);
    return clamp(
        (packedNormal.b - materialClass*0.25)*8.0,
        0.0, 1.0);
}

float bilateralSsao(vec2 uv)
{
    if (ssaoEnabled < 0.5)
    {
        return 0.0;
    }
    float centerDepth = texture(sceneDepth, uv).r;
    vec3 centerPackedNormal = texture(sceneNormal, uv).rgb;
    float centerMaterialClass = materialClassOf(
        vec4(centerPackedNormal, 1.0));
    float centerAoMask = screenAoMaskOf(centerPackedNormal);
    if (centerDepth >= 0.999999 ||
        centerMaterialClass < 0.5 || centerAoMask <= 0.001)
    {
        return 0.0;
    }
    vec3 centerPosition = reconstructViewPosition(uv, centerDepth);
    vec3 centerNormal = decodeOctahedralNormal(centerPackedNormal.rg);
    float total = 0.0;
    float totalWeight = 0.0;
    for (int sampleIndex = 0; sampleIndex < 5; ++sampleIndex)
    {
        vec2 offset = sampleIndex == 0
            ? vec2(0.0)
            : sampleIndex == 1
                ? vec2(1.0, 0.0)
                : sampleIndex == 2
                    ? vec2(-1.0, 0.0)
                    : sampleIndex == 3
                        ? vec2(0.0, 1.0)
                        : vec2(0.0, -1.0);
        vec2 sampleUv = clamp(
            uv + offset*ssaoTexelSize,
            ssaoTexelSize*0.5,
            vec2(1.0) - ssaoTexelSize*0.5);
        float sampleDepth = texture(sceneDepth, sampleUv).r;
        vec3 samplePackedNormal = texture(sceneNormal, sampleUv).rgb;
        float sampleMaterialClass = materialClassOf(
            vec4(samplePackedNormal, 1.0));
        float sampleAoMask = screenAoMaskOf(samplePackedNormal);
        if (sampleDepth >= 0.999999 ||
            sampleMaterialClass < 0.5 || sampleAoMask <= 0.001)
        {
            continue;
        }
        vec3 samplePosition = reconstructViewPosition(
            sampleUv, sampleDepth);
        vec3 sampleNormal = decodeOctahedralNormal(
            samplePackedNormal.rg);
        float depthWeight = exp(
            -abs(samplePosition.z - centerPosition.z)*3.6);
        float normalWeight = pow(
            max(dot(centerNormal, sampleNormal), 0.0), 5.0);
        float spatialWeight = sampleIndex == 0 ? 1.0 : 0.62;
        float weight = depthWeight*normalWeight*spatialWeight*
            sampleAoMask;
        total += texture(ssaoTexture, sampleUv).r*weight;
        totalWeight += weight;
    }
    return total/max(totalWeight, 0.0001);
}

vec4 normalPixel(ivec2 coordinate)
{
    ivec2 size = textureSize(sceneNormal, 0);
    return texelFetch(
        sceneNormal,
        clamp(coordinate, ivec2(0), size - ivec2(1)),
        0);
}

float inkDifference(
    float centerDepth, float centerMaterialClass,
    float depthTolerance,
    ivec2 coordinate, float directionWeight)
{
    float neighborDepth = texelFetch(
        sceneDepth,
        clamp(coordinate, ivec2(0), textureSize(sceneDepth, 0) - ivec2(1)),
        0).r;
    vec4 neighborPackedNormal = normalPixel(coordinate);
    float neighborMaterialClass =
        materialClassOf(neighborPackedNormal);
    float centerIsObject = step(1.5, centerMaterialClass);
    float neighborIsObject = step(1.5, neighborMaterialClass);
    float centerIsTerrain =
        step(0.5, centerMaterialClass)*
        (1.0 - step(1.5, centerMaterialClass));
    float neighborIsBackground =
        1.0 - step(0.5, neighborMaterialClass);

    // Draw on the object's own pixels whenever its neighbour is terrain,
    // water, grass or sky.  Unlike a raw depth edge this closes the contour
    // where a trunk or rock meets the ground.
    float materialBoundary = centerIsObject*
        (1.0 - neighborIsObject);
    // Preserve separation between overlapping props of the same class.
    float objectDepthBoundary = centerIsObject*neighborIsObject*smoothstep(
        depthTolerance * 8.0,
        depthTolerance * 32.0,
        neighborDepth - centerDepth);
    // Terrain receives ink only against actual sky.  This keeps a fine
    // mountain skyline without outlining every blade of grass on the ground.
    float terrainSkyBoundary = centerIsTerrain*
        neighborIsBackground*step(0.9999, neighborDepth);
    float silhouette = max(
        materialBoundary,
        max(objectDepthBoundary, terrainSkyBoundary));
    // Deliberately ignore color and normal changes inside one surface.
    // Low-poly facets belong to the fill; ink is reserved for silhouettes.
    return silhouette * directionWeight;
}

float bayer4(vec2 pixel)
{
    ivec2 position = ivec2(mod(floor(pixel), 4.0));
    const float matrix[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    return matrix[position.y*4 + position.x]/16.0 - 0.5;
}

float paperNoise(vec2 pixel)
{
    vec2 cell = floor(pixel);
    return fract(sin(dot(cell, vec2(12.9898, 78.233)))*43758.5453);
}

vec3 bloomSample(vec2 uv, vec2 texel)
{
    vec3 bloom = vec3(0.0);
    float weight = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            if (abs(x) + abs(y) > 3)
            {
                continue;
            }
            float sampleWeight =
                1.0/(1.0 + float(x*x + y*y));
            vec3 sampleColor = texture(
                texture0, uv + vec2(float(x), float(y))*texel).rgb;
            float brightnessMask = smoothstep(
                0.62, 1.0, luminanceOf(sampleColor));
            bloom += sampleColor*brightnessMask*sampleWeight;
            weight += sampleWeight;
        }
    }
    return bloom/max(weight, 0.001);
}

float menuDepthOfFieldAmount(vec2 uv)
{
    if (menuDofEnabled < 0.5)
    {
        return 0.0;
    }
    float depth = texture(sceneDepth, uv).r;
    float viewDistance = depth < 0.999999
        ? length(reconstructViewPosition(uv, depth))
        : menuDofFocusDistance + menuDofFocusRange*4.0;
    float distanceFromFocus =
        abs(viewDistance - menuDofFocusDistance);
    return smoothstep(
        menuDofFocusRange*0.18,
        menuDofFocusRange,
        distanceFromFocus);
}

vec3 menuDepthOfFieldSample(vec2 uv, vec2 texel, float amount)
{
    if (amount <= 0.001)
    {
        return texture(texture0, uv).rgb;
    }
    vec2 radius = texel*menuDofMaximumBlurPixels*amount;
    vec3 result = texture(texture0, uv).rgb*0.262;
    result += texture(texture0, clamp(
        uv + vec2( 0.000,  1.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.707,  0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 1.000,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.707, -0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.000, -1.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-0.707, -0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-1.000,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-0.707,  0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.360,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.041;
    result += texture(texture0, clamp(
        uv + vec2(-0.360,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.041;
    return result;
}

vec3 rotateHue(vec3 color, float angle)
{
    float cosine = cos(angle);
    float sine = sin(angle);
    mat3 rotation = mat3(
        0.299 + 0.701*cosine + 0.168*sine,
        0.587 - 0.587*cosine + 0.330*sine,
        0.114 - 0.114*cosine - 0.497*sine,
        0.299 - 0.299*cosine - 0.328*sine,
        0.587 + 0.413*cosine + 0.035*sine,
        0.114 - 0.114*cosine + 0.292*sine,
        0.299 - 0.300*cosine + 1.250*sine,
        0.587 - 0.588*cosine - 1.050*sine,
        0.114 + 0.886*cosine - 0.203*sine);
    return rotation*color;
}

void main()
{
    ivec2 sourceSize = textureSize(texture0, 0);
    vec2 healthCentered = fragTexCoord*2.0 - 1.0;
    float healthRadius = length(healthCentered);
    float healthEdge = smoothstep(0.18, 1.18, healthRadius);
    vec2 healthDirection = vec2(0.0);
    vec2 healthUv = fragTexCoord;
    if (lowHealthAmount > 0.001)
    {
        healthDirection = healthRadius > 0.0001
            ? healthCentered/healthRadius
            : vec2(0.0);
        float healthWave = sin(
            healthRadius*17.0 - lowHealthTime*2.4);
        healthUv = clamp(
            fragTexCoord + healthDirection*healthWave*healthEdge*
                lowHealthAmount*0.0032,
            vec2(0.0), vec2(1.0));
    }
    ivec2 sourceCoordinate = clamp(
        ivec2(floor(healthUv*vec2(sourceSize))),
        ivec2(0), sourceSize - ivec2(1));
    vec2 pixelUv =
        (vec2(sourceCoordinate) + 0.5)/vec2(sourceSize);
    vec4 source =
        texelFetch(texture0, sourceCoordinate, 0)*fragColor;
    vec2 texel = 1.0/vec2(sourceSize);
    float menuDofAmount = menuDepthOfFieldAmount(pixelUv);
    source.rgb = menuDepthOfFieldSample(
        pixelUv, texel, menuDofAmount)*fragColor.rgb;
    if (lowHealthAmount > 0.001)
    {
        vec2 chromaticOffset = healthDirection*texel*2.2*
            healthEdge*lowHealthAmount;
        source.r = texture(
            texture0, clamp(healthUv + chromaticOffset,
                            vec2(0.0), vec2(1.0))).r;
        source.b = texture(
            texture0, clamp(healthUv - chromaticOffset,
                            vec2(0.0), vec2(1.0))).b;
    }
    vec2 stylePixel = vec2(sourceCoordinate);
    vec3 color = source.rgb;
    float contactAo = bilateralSsao(pixelUv)*
        clamp(ssaoStrength, 0.0, 0.6);
    // Cool green-grey contact, never black. Keeps bright low-poly palette.
    color *= mix(
        vec3(1.0), vec3(0.38, 0.52, 0.47), contactAo);
    if (abs(sharpness) > 0.001)
    {
        vec3 neighborhood =
            (texture(texture0, pixelUv + vec2(texel.x, 0.0)).rgb +
             texture(texture0, pixelUv - vec2(texel.x, 0.0)).rgb +
             texture(texture0, pixelUv + vec2(0.0, texel.y)).rgb +
             texture(texture0, pixelUv - vec2(0.0, texel.y)).rgb)*0.25;
        color += (source.rgb - neighborhood)*sharpness*0.72*
            (1.0 - menuDofAmount);
    }
    if (bloomEnabled > 0.5)
    {
        color += bloomSample(pixelUv, texel)*bloomStrength;
    }
    color *= exp2(postExposure);
    color += brightness;
    color =
        max(color - vec3(blackPoint), vec3(0.0))/
        max(1.0 - blackPoint, 0.001);
    color = (color - 0.5)*contrast + 0.5;

    color += vec3(
        temperature*0.10 - tint*0.025,
        tint*0.075,
        -temperature*0.10 - tint*0.025);
    color = rotateHue(color, radians(hueDegrees));
    float luminance = dot(color, LuminanceWeights);
    color = mix(vec3(luminance), color, saturation);

    float shadowWeight =
        1.0 - smoothstep(0.08, 0.52, luminance);
    float highlightWeight =
        smoothstep(0.48, 0.92, luminance);
    float midtoneWeight =
        clamp(1.0 - shadowWeight - highlightWeight, 0.0, 1.0);
    float curveExposure =
        curveShadows*shadowWeight +
        curveMidtones*midtoneWeight +
        curveHighlights*highlightWeight;
    color *= exp2(curveExposure*0.85);
    color = pow(
        max(color, vec3(0.0)),
        vec3(1.0/max(gammaValue, 0.01)));

    float dither = bayer4(stylePixel)*
        ditherStrength*(ditherEnabled > 0.5 ? 1.0 : 0.0);
    if (paletteEnabled > 0.5)
    {
        float levels = max(round(paletteLevels), 2.0);
        color = floor(clamp(color, 0.0, 1.0)*(levels - 1.0) +
                      0.5 + dither)/(levels - 1.0);
    }
    else if (ditherEnabled > 0.5)
    {
        color += dither/255.0*6.0;
    }

    float centerMaterialClass = materialClassOf(
        normalPixel(sourceCoordinate));
    if (inkOutlinesEnabled > 0.5 && centerMaterialClass > 0.5)
    {
        float centerDepth = texture(sceneDepth, pixelUv).r;
        float depthTolerance = max(fwidth(centerDepth) * 1.5, 0.000002);
        float viewDistance = centerDepth < 0.999999
            ? length(reconstructViewPosition(pixelUv, centerDepth))
            : 100000.0;
        // Distance controls both opacity and width. Far silhouettes keep a
        // fine line instead of disappearing, while nearby props receive the
        // chunky contour used by the HUD and concept art.
        float nearDistance =
            1.0 - smoothstep(24.0, 120.0, viewDistance);
        // Distant silhouettes stay solid enough to survive FXAA instead of
        // breaking into short dashes along thin tree crowns and branches.
        float outlineDistance = mix(0.56, 1.0, nearDistance);
        outlineDistance *= 1.0 - menuDofAmount;
        // The auxiliary normal/AO target marks trees and sizeable plants.
        // Their ink disappears earlier than prop/enemy ink, including the
        // terrain-side half of an edge, so distant foliage becomes a clean
        // mass instead of line noise without making its RGB transparent.
        float vegetationOutlineFade = mix(
            0.82, 1.0,
            1.0 - smoothstep(32.0, 125.0, viewDistance));
        float sourceVegetation = step(
            2.5, centerMaterialClass);
        outlineDistance *= mix(
            1.0, vegetationOutlineFade, sourceVegetation);
        float edge = 0.0;
        // Keep the close-up contour only slightly wider than the distant
        // one. The previous 1.75 multiplier turned the minimum UI setting
        // into a nearly two-pixel line.
        float distanceWidthScale = mix(
            1.00, 1.40,
            1.0 - smoothstep(10.0, 78.0, viewDistance));
        // Trees become thinner sooner than solid props, but retain one full
        // pixel even in the distant forest.
        distanceWidthScale *= mix(
            1.0,
            mix(1.0, 0.94, smoothstep(28.0, 105.0, viewDistance)),
            sourceVegetation);
        float effectiveWidth = clamp(
            outlineWidth * distanceWidthScale, 1.0, 3.0);
        float sourceTerrain = step(0.5, centerMaterialClass)*
            (1.0 - step(1.5, centerMaterialClass));
        // Mountain contours stay a single fine pixel at every distance.
        effectiveWidth = mix(effectiveWidth, 1.0, sourceTerrain);
        // Fractional outer rings give nearby silhouettes a soft extra edge
        // instead of jumping directly between hard one- and two-pixel lines.
        // Four cardinal samples still keep this kernel half the former size.
        int pixelWidth = int(ceil(effectiveWidth));
        for (int ring = 1; ring <= 3; ++ring)
        {
            if (ring > pixelWidth)
            {
                continue;
            }
            ivec2 horizontal = ivec2(ring, 0);
            ivec2 vertical = ivec2(0, ring);
            float ringEdge = 0.0;
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass, depthTolerance,
                sourceCoordinate + horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass, depthTolerance,
                sourceCoordinate - horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass, depthTolerance,
                sourceCoordinate + vertical, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass, depthTolerance,
                sourceCoordinate - vertical, 1.0));
            float ringCoverage = clamp(
                effectiveWidth - float(ring) + 1.0,
                0.0, 1.0);
            edge = max(edge, ringEdge * ringCoverage);
        }
        // A steeper response restores the chunky foreground silhouette.
        // Distance still removes it from the background independently.
        float nearBoost = mix(1.0, 1.24,
            1.0 - smoothstep(18.0, 32.0, viewDistance));
        float ink = smoothstep(0.10, 0.52, edge)*
            sqrt(clamp(outlineStrength, 0.0, 1.0))*1.3*
            outlineDistance*nearBoost;
        vec3 inkColor = vec3(0.006, 0.008, 0.010);
        color = mix(color, inkColor, clamp(ink, 0.0, 0.985));
    }

    if (paperGrainEnabled > 0.5)
    {
        float grain = paperNoise(stylePixel) - 0.5;
        color *= 1.0 + grain*paperGrainStrength*2.0;
    }

    vec2 centered = fragTexCoord*2.0 - 1.0;
    float vignetteMask =
        smoothstep(0.28, 1.25, dot(centered, centered));
    color *= 1.0 - vignetteMask*vignette;

    float heartbeat = 0.5 + 0.5*sin(lowHealthTime*4.1);
    heartbeat = heartbeat*heartbeat*lowHealthPulse;
    float dangerMask = smoothstep(
        0.12, 1.08, dot(centered, centered));
    float danger = clamp(
        dangerMask*lowHealthAmount*(0.72 + heartbeat*0.18),
        0.0, 0.88);
    vec3 dangerColor =
        color*vec3(0.58, 0.24, 0.29) +
        vec3(0.045 + heartbeat*0.018, 0.0, 0.008);
    color = mix(color, dangerColor, danger);
    float centerDrain = lowHealthAmount*
        (1.0 - smoothstep(
            0.05, 1.0, dot(centered, centered)))*0.08;
    float healthLuminance = luminanceOf(color);
    color = mix(color, vec3(healthLuminance), centerDrain);

    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
