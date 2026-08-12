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

float bilateralSsao(vec2 uv)
{
    if (ssaoEnabled < 0.5)
    {
        return 0.0;
    }
    float centerDepth = texture(sceneDepth, uv).r;
    vec3 centerPackedNormal = texture(sceneNormal, uv).rgb;
    if (centerDepth >= 0.999999 || centerPackedNormal.b <= 0.001)
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
        if (sampleDepth >= 0.999999 ||
            samplePackedNormal.b <= 0.001)
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
        float weight = depthWeight*normalWeight*spatialWeight;
        total += texture(ssaoTexture, sampleUv).r*weight;
        totalWeight += weight;
    }
    return total/max(totalWeight, 0.0001);
}

vec3 sourcePixel(ivec2 coordinate)
{
    ivec2 size = textureSize(texture0, 0);
    return texelFetch(
        texture0,
        clamp(coordinate, ivec2(0), size - ivec2(1)),
        0).rgb;
}

float inkDifference(
    vec3 centerColor, float centerLuminance,
    ivec2 coordinate, float directionWeight)
{
    vec3 neighborColor = sourcePixel(coordinate);
    float neighborLuminance = luminanceOf(neighborColor);
    float luminanceContrast =
        abs(centerLuminance - neighborLuminance);
    float colorContrast =
        length(centerColor - neighborColor)*0.16;
    float darkerSide = smoothstep(
        -0.025, 0.065,
        neighborLuminance - centerLuminance);
    return (luminanceContrast + colorContrast)*
        mix(0.04, 1.0, darkerSide)*directionWeight;
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
    ivec2 sourceCoordinate = clamp(
        ivec2(floor(fragTexCoord*vec2(sourceSize))),
        ivec2(0), sourceSize - ivec2(1));
    vec2 pixelUv =
        (vec2(sourceCoordinate) + 0.5)/vec2(sourceSize);
    vec4 source =
        texelFetch(texture0, sourceCoordinate, 0)*fragColor;
    vec2 texel = 1.0/vec2(sourceSize);
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
        color += (source.rgb - neighborhood)*sharpness*0.72;
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

    if (inkOutlinesEnabled > 0.5 && source.a > 0.25)
    {
        float centerLuminance = luminanceOf(source.rgb);
        float edge = 0.0;
        int pixelWidth = int(round(clamp(
            outlineWidth, 1.0, 4.0)));
        for (int ring = 1; ring <= 4; ++ring)
        {
            if (ring > pixelWidth)
            {
                continue;
            }
            ivec2 horizontal = ivec2(ring, 0);
            ivec2 vertical = ivec2(0, ring);
            ivec2 diagonal = ivec2(ring, ring);
            float ringEdge = 0.0;
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate + horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate - horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate + vertical, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate - vertical, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate + diagonal, 0.72));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate - diagonal, 0.72));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate + ivec2(ring, -ring), 0.72));
            ringEdge = max(ringEdge, inkDifference(
                source.rgb, centerLuminance,
                sourceCoordinate + ivec2(-ring, ring), 0.72));
            edge = max(edge, ringEdge);
        }
        float ink = smoothstep(0.060, 0.26, edge)*outlineStrength;
        color *= 1.0 - clamp(ink, 0.0, 0.92);
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

    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
