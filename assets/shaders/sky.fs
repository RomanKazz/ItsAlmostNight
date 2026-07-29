#version 330

uniform vec2 viewportSize;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform vec3 zenithColor;
uniform vec3 horizonColor;
uniform vec3 lowerSkyColor;
uniform vec3 celestialDirection;
uniform vec3 celestialColor;
uniform float celestialIntensity;
uniform float nightAmount;
uniform float timeSeconds;
uniform float exposure;
uniform float saturation;

out vec4 finalColor;

float hash21(vec2 position)
{
    return fract(sin(dot(position, vec2(127.1, 311.7)))*43758.5453);
}

float hash31(vec3 position)
{
    return fract(
        sin(dot(position, vec3(127.1, 311.7, 74.7)))*
        43758.5453);
}

float valueNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 local = fract(position);
    vec2 blend = local*local*(3.0 - 2.0*local);
    return mix(
        mix(hash21(cell), hash21(cell + vec2(1.0, 0.0)), blend.x),
        mix(hash21(cell + vec2(0.0, 1.0)),
            hash21(cell + vec2(1.0, 1.0)), blend.x),
        blend.y);
}

void main()
{
    vec2 uv = gl_FragCoord.xy/max(viewportSize, vec2(1.0));
    vec2 screen = uv*2.0 - 1.0;
    vec3 viewDirection = normalize(
        cameraForward +
        cameraRight*screen.x*tanHalfFov*aspectRatio +
        cameraUp*screen.y*tanHalfFov);

    float upperElevation = clamp(viewDirection.y, 0.0, 1.0);
    float zenithBlend = pow(smoothstep(0.0, 0.82, upperElevation), 0.72);
    vec3 sky = mix(horizonColor, zenithColor, zenithBlend);

    float lowerBlend = smoothstep(0.0, 0.42, -viewDirection.y);
    sky = mix(sky, lowerSkyColor, lowerBlend);

    float celestialAlignment =
        dot(viewDirection, normalize(celestialDirection));
    float halo = smoothstep(0.985, 0.9992, celestialAlignment);
    float disc = smoothstep(0.99945, 0.99982, celestialAlignment);
    sky += celestialColor*(halo*0.18 + disc*0.82)*celestialIntensity;

    vec3 starCell = floor(viewDirection*260.0);
    float starSeed = hash31(starCell);
    float stars = smoothstep(0.9965, 0.9998, starSeed);
    float twinkle =
        0.72 + 0.28*
        sin(timeSeconds*(1.2 + hash31(starCell + 17.0)*1.8) +
            starSeed*83.0);
    float starVisibility =
        smoothstep(0.12, 0.72, nightAmount)*
        smoothstep(-0.08, 0.16, viewDirection.y);
    vec3 starColor = mix(
        vec3(0.72, 0.82, 1.0),
        vec3(1.0, 0.88, 0.68),
        hash31(starCell + 41.0));
    sky += starColor*stars*twinkle*starVisibility*1.35;

    float cloudAltitude =
        smoothstep(0.015, 0.075, viewDirection.y)*
        (1.0 - smoothstep(0.58, 0.82, viewDirection.y));
    vec2 cloudPosition =
        viewDirection.xz/max(viewDirection.y, 0.08);
    cloudPosition +=
        vec2(timeSeconds*0.012, timeSeconds*0.004);
    float broadClouds = valueNoise(cloudPosition*0.72);
    float cloudDetail =
        valueNoise(cloudPosition*1.45 + vec2(timeSeconds*0.003, 7.4));
    float clouds =
        smoothstep(0.58, 0.76, broadClouds*0.72 + cloudDetail*0.28)*
        cloudAltitude;
    vec3 dayCloudColor =
        mix(horizonColor, vec3(0.94, 0.96, 0.95), 0.72);
    vec3 nightCloudColor =
        mix(horizonColor, zenithColor, 0.42)*1.15;
    vec3 cloudColor =
        mix(dayCloudColor, nightCloudColor, nightAmount);
    sky = mix(sky, cloudColor, clouds*0.30);

    sky *= exposure;
    float luminance = dot(sky, vec3(0.2126, 0.7152, 0.0722));
    sky = mix(vec3(luminance), sky, saturation);

    finalColor = vec4(sky, 1.0);
}
