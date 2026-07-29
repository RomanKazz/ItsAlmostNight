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

out vec4 finalColor;

const vec3 LuminanceWeights =
    vec3(0.2126, 0.7152, 0.0722);

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
    vec4 source = texture(texture0, fragTexCoord)*fragColor;
    vec2 texel = 1.0/vec2(textureSize(texture0, 0));
    vec3 neighborhood =
        (texture(texture0, fragTexCoord + vec2(texel.x, 0.0)).rgb +
         texture(texture0, fragTexCoord - vec2(texel.x, 0.0)).rgb +
         texture(texture0, fragTexCoord + vec2(0.0, texel.y)).rgb +
         texture(texture0, fragTexCoord - vec2(0.0, texel.y)).rgb)*0.25;

    vec3 color =
        source.rgb + (source.rgb - neighborhood)*sharpness*0.72;
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

    vec2 centered = fragTexCoord*2.0 - 1.0;
    float vignetteMask =
        smoothstep(0.28, 1.25, dot(centered, centered));
    color *= 1.0 - vignetteMask*vignette;

    finalColor = vec4(max(color, vec3(0.0)), source.a);
}
