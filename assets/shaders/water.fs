#version 330

in vec3 fragWorldPosition;
in float fragWaterDepth;
in float fragShoreDistance;

uniform vec3 cameraPosition;
uniform vec3 shoreColor;
uniform vec3 shallowColor;
uniform vec3 midColor;
uniform vec3 deepColor;
uniform vec3 skyColor;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 fogColor;
uniform vec3 dayNightTint;
uniform float fogStart;
uniform float fogEnd;
uniform float exposure;
uniform float timeSeconds;
uniform float waveSpeed;
uniform float shoreWidth;
uniform float depthFalloff;
uniform float patternScale;
uniform float patternStrength;
uniform float patternSpeed;
uniform float fresnelStrength;
uniform int qualityLevel;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

float hash21(vec2 value)
{
    value = fract(value*vec2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return fract(value.x*value.y);
}

float valueNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 blend = fract(position);
    blend = blend*blend*(3.0 - 2.0*blend);

    float northWest = hash21(cell);
    float northEast = hash21(cell + vec2(1.0, 0.0));
    float southWest = hash21(cell + vec2(0.0, 1.0));
    float southEast = hash21(cell + vec2(1.0, 1.0));
    return mix(
        mix(northWest, northEast, blend.x),
        mix(southWest, southEast, blend.x),
        blend.y);
}

void main()
{
    if (fragShoreDistance > 0.16)
    {
        discard;
    }
    float edgeCoverage = 1.0 - smoothstep(
        0.015, 0.15, fragShoreDistance);

    vec2 world = fragWorldPosition.xz;

    // Depth supplies only the broad shallow-to-deep trend. Two large static
    // world-space fields bend it enough that no colour boundary follows the
    // shoreline or reads as a concentric depth contour.
    float depth = clamp(fragWaterDepth*depthFalloff, 0.0, 1.0);
    float depthNoise1 = valueNoise(world*(patternScale*1.285714));
    float depthNoise2 = valueNoise(
        world*(patternScale*2.571429) + vec2(13.7, 4.2));
    float depthDistortion =
        depthNoise1*0.70 + depthNoise2*0.30 - 0.5;
    float distortedDepth = clamp(
        depth + depthDistortion*0.20, 0.0, 1.0);
    float shallow = smoothstep(0.08, 0.32, distortedDepth);
    float deep = smoothstep(0.42, 0.85, distortedDepth);
    vec3 water = mix(
        shallowColor, midColor, shallow);
    water = mix(water, deepColor, deep);

    // signedDistance is normalized by the authored shoreline width. This is
    // a shallow-water colour band, deliberately not a white foam contour.
    float inward = max(-fragShoreDistance, 0.0);
    float shoreAmount = 1.0 - smoothstep(
        shoreWidth*0.08, shoreWidth, inward);
    water = mix(water, shoreColor, shoreAmount*0.86);

    // An independent, even broader moving field creates hand-painted surface
    // patches unrelated to both depth and distance from shore.
    float driftTime = timeSeconds*patternSpeed;
    float surfaceNoise = valueNoise(
        world*patternScale +
        driftTime*vec2(0.004, 0.002));
    float patch = smoothstep(0.40, 0.65, surfaceNoise);
    float patchShift = (patch - 0.5)*2.0*patternStrength*
        (1.0 - shoreAmount*0.35);
    water *= vec3(
        1.0 + patchShift*0.68,
        1.0 + patchShift*0.94,
        1.0 + patchShift);

    // Almost-flat geometric motion only perturbs the Fresnel very slightly.
    float waveTime = timeSeconds*waveSpeed;
    float waveX = cos(
        world.x*0.092 + world.y*0.027 + waveTime*0.12)*0.00075;
    float waveZ = cos(
        world.y*0.118 - world.x*0.021 - waveTime*0.09)*0.00085;
    vec3 normal = normalize(vec3(-waveX, 1.0, -waveZ));
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float fresnel = pow(
        1.0 - clamp(dot(normal, viewDirection), 0.0, 1.0), 3.0);
    vec3 fresnelColor = mix(vec3(0.30, 0.72, 0.76), skyColor, 0.08);
    water = mix(water, fresnelColor, fresnel*fresnelStrength);

    // Rare broad highlights share the same field so they remain soft and do
    // not turn into a layer of small realistic sparkles.
    float softHighlight = smoothstep(0.76, 0.92, surfaceNoise)*
        (0.45 + (1.0 - depth)*0.55);
    water = mix(
        water, vec3(0.42, 0.82, 0.80),
        softHighlight*0.075);
    vec3 halfDirection = normalize(
        viewDirection - normalize(sunDirection));
    float sunHighlight = pow(
        max(dot(normal, halfDirection), 0.0), 72.0);
    water += sunColor*sunHighlight*softHighlight*0.055;

    float alpha = mix(0.94, 0.985, depth)*edgeCoverage;
    water *= dayNightTint*exposure;
    float nightAmount = smoothstep(
        0.02, 0.26, 1.0 - dayNightTint.r);
    // Gameplay water remains legible at night, but no channel stays at its
    // daytime brightness and blue/teal identity is preserved.
    water *= mix(
        vec3(1.0), vec3(0.62, 0.68, 0.72), nightAmount);
    float cameraDistance = distance(cameraPosition, fragWorldPosition);
    float fog = smoothstep(fogStart, max(fogEnd, fogStart + 0.01),
                           cameraDistance);
    water = mix(water, fogColor, fog);
    alpha = mix(alpha, 1.0, fog*0.65);
    finalColor = vec4(water, alpha);
    normalAo = vec4(0.0, 0.0, 0.0, 1.0);
}
