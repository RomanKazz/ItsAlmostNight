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

void main()
{
    if (fragShoreDistance > 0.16)
    {
        discard;
    }
    float edgeCoverage = 1.0 - smoothstep(
        -0.035, 0.145, fragShoreDistance);

    float time = timeSeconds*waveSpeed;
    float depth = smoothstep(0.02, 0.96, fragWaterDepth);
    float variation =
        sin(fragWorldPosition.x*0.071 + fragWorldPosition.z*0.053)*0.025 +
        sin(fragWorldPosition.x*0.031 - fragWorldPosition.z*0.089)*0.018;
    vec3 water = mix(shallowColor, deepColor, clamp(depth + variation, 0.0, 1.0));

    vec3 normal = normalize(vec3(
        -cos(fragWorldPosition.x*0.105 + time*1.7)*0.055,
        1.0,
        -cos(fragWorldPosition.z*0.137 - time*1.25)*0.055));
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.4);
    water = mix(water, skyColor, fresnel*0.36 + 0.08);

    vec3 halfDirection = normalize(viewDirection - normalize(sunDirection));
    float sparkle = pow(max(dot(normal, halfDirection), 0.0), 72.0);
    float sparkleNoise = smoothstep(
        0.72, 0.96,
        sin(fragWorldPosition.x*1.37 + time*3.1)*0.5 +
        sin(fragWorldPosition.z*1.71 - time*2.4)*0.5);
    water += sunColor*sparkle*(0.22 + sparkleNoise*0.34);

    float inward = clamp(-fragShoreDistance, 0.0, 2.0);
    float shoreFade = 1.0 - smoothstep(0.0, 1.65, inward);
    float pattern = hash21(floor(fragWorldPosition.xz*0.18))*0.22;
    float travel = fract(inward*0.58 + time*0.32 + pattern);
    float bandA = 1.0 - smoothstep(0.035, 0.115, abs(travel - 0.18));
    float bandB = 1.0 - smoothstep(
        0.03, 0.095, abs(fract(travel + 0.47) - 0.18));
    float foam = max(bandA, bandB*0.72)*shoreFade*
                 (1.0 - depth*0.55)*edgeCoverage;
    water = mix(water, vec3(0.82, 1.0, 0.94), foam*0.72);

    float alpha = mix(0.67, 0.88, depth);
    alpha = max(alpha, foam*0.92)*edgeCoverage;
    water *= dayNightTint*exposure;
    float cameraDistance = distance(cameraPosition, fragWorldPosition);
    float fog = smoothstep(fogStart, max(fogEnd, fogStart + 0.01),
                           cameraDistance);
    water = mix(water, fogColor, fog);
    alpha = mix(alpha, 1.0, fog*0.65);
    finalColor = vec4(water, alpha);
}
