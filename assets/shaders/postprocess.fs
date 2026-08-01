#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
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
uniform float posterizedLightingEnabled;
uniform float lightingSteps;
uniform float bloomEnabled;
uniform float bloomStrength;
uniform float inkOutlinesEnabled;
uniform float outlineStrength;
uniform float paperGrainEnabled;
uniform float paperGrainStrength;

out vec4 finalColor;

const vec3 LuminanceWeights =
    vec3(0.2126, 0.7152, 0.0722);

float luminanceOf(vec3 color)
{
    return dot(color, LuminanceWeights);
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

vec3 filmicToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        (color*(a*color + b))/(color*(c*color + d) + e),
        0.0, 1.0);
}

void main()
{
    vec4 source = texture(texture0, fragTexCoord)*fragColor;
    vec2 texel = 1.0/vec2(textureSize(texture0, 0));
    vec2 stylePixel = floor(
        fragTexCoord*vec2(textureSize(texture0, 0)));
    vec3 neighborhood =
        (texture(texture0, fragTexCoord + vec2(texel.x, 0.0)).rgb +
         texture(texture0, fragTexCoord - vec2(texel.x, 0.0)).rgb +
         texture(texture0, fragTexCoord + vec2(0.0, texel.y)).rgb +
         texture(texture0, fragTexCoord - vec2(0.0, texel.y)).rgb)*0.25;

    vec3 color =
        source.rgb + (source.rgb - neighborhood)*sharpness*0.72;
    if (bloomEnabled > 0.5)
    {
        color += bloomSample(fragTexCoord, texel)*bloomStrength;
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
    color = filmicToneMap(max(color, vec3(0.0)));
    color = pow(
        max(color, vec3(0.0)),
        vec3(1.0/max(gammaValue, 0.01)));

    float dither = bayer4(stylePixel)*
        ditherStrength*(ditherEnabled > 0.5 ? 1.0 : 0.0);
    if (posterizedLightingEnabled > 0.5)
    {
        float steps = max(round(lightingSteps), 2.0);
        float currentLuminance = max(luminanceOf(color), 0.0001);
        float steppedLuminance =
            floor(clamp(currentLuminance, 0.0, 1.0)*(steps - 1.0) +
                  0.5 + dither)/(steps - 1.0);
        color *= steppedLuminance/currentLuminance;
    }
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

    if (inkOutlinesEnabled > 0.5)
    {
        float centerLuminance = luminanceOf(source.rgb);
        float edge = 0.0;
        edge = max(edge, abs(centerLuminance - luminanceOf(
            texture(texture0, fragTexCoord + vec2(texel.x, 0.0)).rgb)));
        edge = max(edge, abs(centerLuminance - luminanceOf(
            texture(texture0, fragTexCoord - vec2(texel.x, 0.0)).rgb)));
        edge = max(edge, abs(centerLuminance - luminanceOf(
            texture(texture0, fragTexCoord + vec2(0.0, texel.y)).rgb)));
        edge = max(edge, abs(centerLuminance - luminanceOf(
            texture(texture0, fragTexCoord - vec2(0.0, texel.y)).rgb)));
        float ink = smoothstep(0.035, 0.22, edge)*outlineStrength;
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

    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), source.a);
}
