#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float timeSeconds;
uniform vec4 tint;
uniform float intensity;

out vec4 finalColor;

float hash21(vec2 value)
{
    return fract(sin(dot(value, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(vec2 value)
{
    vec2 cell = floor(value);
    vec2 local = fract(value);
    vec2 blend = local * local * (3.0 - 2.0 * local);
    float a = hash21(cell);
    float b = hash21(cell + vec2(1.0, 0.0));
    float c = hash21(cell + vec2(0.0, 1.0));
    float d = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

void main()
{
    vec2 uv = fragTexCoord;
    float noise = valueNoise(uv * 7.0 + vec2(timeSeconds * 0.31, -timeSeconds * 0.23));
    noise = mix(noise, valueNoise(uv * 15.0 - timeSeconds * 0.48), 0.35);
    float pulse = 0.86 + 0.14 * sin(timeSeconds * 5.2 + noise * 5.0);
    float longitude = uv.x * 6.2831853;
    float latitude = uv.y * 3.1415926;
    float facets = 0.5 + 0.5 * sin(
        longitude * 7.0 + sin(latitude * 5.0) * 2.2 +
        timeSeconds * 0.9 + noise * 3.0);
    facets = pow(facets, 3.0);
    float bands = 0.5 + 0.5 * sin(
        latitude * 9.0 - longitude * 3.0 + timeSeconds * 2.1);
    bands = smoothstep(0.68, 0.96, bands);
    float facetMask = smoothstep(0.45, 0.92, facets);
    float alpha = clamp(0.68 + noise * 0.18 + bands * 0.10, 0.0, 1.0);
    vec3 core = vec3(0.875, 0.973, 1.0);
    vec3 primary = vec3(0.333, 0.812, 1.0);
    vec3 secondary = vec3(0.129, 0.549, 1.0);
    vec3 edge = vec3(0.749, 0.965, 1.0);
    vec3 color = mix(secondary, primary,
                     clamp(noise * 0.72 + facets * 0.24, 0.0, 1.0));
    color = mix(color, core, bands * 0.72 + facetMask * 0.22);
    color += edge * (bands * 0.34 + facetMask * 0.26);
    color += vec3(0.12, 0.38, 0.62) * noise * 0.24;
    finalColor = vec4(color * tint.rgb * pulse * intensity,
                      alpha * tint.a * fragColor.a * intensity);
}
