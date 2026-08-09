#version 330

in vec3 fragWorldPosition;
in float fragWaterDepth;
in float fragShoreDistance;
in float fragWaveHeight;

uniform vec3 cameraPosition;
uniform vec3 shallowColor;
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

out vec4 finalColor;

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
        -0.035, 0.145, fragShoreDistance);

    float time = timeSeconds*waveSpeed;
    // Reach the deep lake colour reasonably close to shore. Keeping most of
    // the surface translucent exposed the saturated green terrain beneath it
    // and made the lake read as a swamp.
    float depth = smoothstep(0.025, 0.72, fragWaterDepth);
    float variation =
        sin(fragWorldPosition.x*0.071 + fragWorldPosition.z*0.053)*0.018 +
        sin(fragWorldPosition.x*0.031 - fragWorldPosition.z*0.089)*0.012;
    float colorDepth = clamp(depth + variation, 0.0, 1.0);
    vec3 water = mix(shallowColor, deepColor, colorDepth);

    float waveX =
        cos(fragWorldPosition.x*0.092 + fragWorldPosition.z*0.027 + time*1.35)*0.060 +
        cos(fragWorldPosition.x*0.19 + fragWorldPosition.z*0.14 + time*1.9)*0.020;
    float waveZ =
        cos(fragWorldPosition.z*0.118 - fragWorldPosition.x*0.021 - time*1.05)*0.068 +
        cos(fragWorldPosition.x*0.19 + fragWorldPosition.z*0.14 + time*1.9)*0.015;
    vec3 normal = normalize(vec3(
        -waveX, 1.0, -waveZ));
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.15);
    vec3 clearSkyReflection = mix(skyColor, vec3(0.12, 0.48, 0.72), 0.24);
    water = mix(water, clearSkyReflection, 0.12 + fresnel*0.48);

    // Broad moving blue highlights break up the flat surface without making
    // it noisy or realistic enough to clash with the toon world.
    float longRipple = sin(
        fragWorldPosition.x*0.23 + fragWorldPosition.z*0.11 + time*1.7);
    float crossRipple = sin(
        fragWorldPosition.z*0.31 - fragWorldPosition.x*0.08 - time*1.25);
    float rippleLine = smoothstep(0.72, 0.96,
        longRipple*0.58 + crossRipple*0.42);
    water += vec3(0.055, 0.13, 0.18)*rippleLine*(0.35 + fresnel*0.65);

    vec3 halfDirection = normalize(viewDirection - normalize(sunDirection));
    float sparkle = pow(max(dot(normal, halfDirection), 0.0), 88.0);
    float sparkleNoise = smoothstep(
        0.72, 0.96,
        sin(fragWorldPosition.x*1.37 + time*3.1)*0.5 +
        sin(fragWorldPosition.z*1.71 - time*2.4)*0.5);
    water += sunColor*sparkle*(0.28 + sparkleNoise*0.42);

    float inward = clamp(-fragShoreDistance, 0.0, 2.0);
    float shoreFade = 1.0 - smoothstep(0.0, 0.58, inward);
    // Keep neighbouring foam phases continuous. A raw per-cell hash creates
    // visible rectangular seams where shoreline wave bands cross cell edges.
    float pattern = valueNoise(fragWorldPosition.xz*0.18)*0.22;
    float travel = fract(inward*0.82 + time*0.27 + pattern);
    float bandA = 1.0 - smoothstep(0.025, 0.075, abs(travel - 0.18));
    float bandB = 1.0 - smoothstep(
        0.022, 0.065, abs(fract(travel + 0.51) - 0.18));
    float foam = max(bandA, bandB*0.58)*shoreFade*
                 (1.0 - depth*0.72)*edgeCoverage;
    water = mix(water, vec3(0.68, 0.90, 0.96), foam*0.36);

    float alpha = mix(0.82, 0.96, depth);
    alpha = max(alpha, foam*0.88)*edgeCoverage;
    water *= dayNightTint*exposure;
    float cameraDistance = distance(cameraPosition, fragWorldPosition);
    float fog = smoothstep(fogStart, max(fogEnd, fogStart + 0.01),
                           cameraDistance);
    water = mix(water, fogColor, fog);
    alpha = mix(alpha, 1.0, fog*0.65);
    finalColor = vec4(water, alpha);
}
