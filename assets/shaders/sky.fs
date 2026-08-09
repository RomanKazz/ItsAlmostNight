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

vec3 cubeStarCoordinates(vec3 direction)
{
    vec3 absoluteDirection = abs(direction);
    if (absoluteDirection.x >= absoluteDirection.y &&
        absoluteDirection.x >= absoluteDirection.z) {
        return vec3(
            direction.zy/max(absoluteDirection.x, 0.0001),
            direction.x >= 0.0 ? 0.0 : 1.0);
    }
    if (absoluteDirection.y >= absoluteDirection.z) {
        return vec3(
            direction.xz/max(absoluteDirection.y, 0.0001),
            direction.y >= 0.0 ? 2.0 : 3.0);
    }
    return vec3(
        direction.xy/max(absoluteDirection.z, 0.0001),
        direction.z >= 0.0 ? 4.0 : 5.0);
}

float cloudFbm(vec2 position)
{
    float result = 0.0;
    float weight = 0.54;
    mat2 rotation = mat2(0.80, 0.60, -0.60, 0.80);
    for (int octave = 0; octave < 4; ++octave) {
        result += valueNoise(position)*weight;
        position = rotation*position*2.03 + vec2(17.1, 9.2);
        weight *= 0.48;
    }
    return result/1.011;
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
    float zenithBlend = pow(
        smoothstep(0.0, 0.88, upperElevation), 0.58);
    vec3 turquoiseHorizon = mix(
        horizonColor, vec3(0.72, 0.91, 0.92),
        (1.0 - nightAmount)*0.34);
    vec3 sky = mix(turquoiseHorizon, zenithColor, zenithBlend);

    float horizonHaze =
        exp(-abs(viewDirection.y)*15.0)*(1.0 - nightAmount);
    sky = mix(sky, turquoiseHorizon*1.045, horizonHaze*0.34);

    float lowerBlend = smoothstep(0.0, 0.42, -viewDirection.y);
    sky = mix(sky, lowerSkyColor, lowerBlend);

    float celestialAlignment =
        dot(viewDirection, normalize(celestialDirection));
    float dayCelestial = 1.0 - smoothstep(0.48, 0.92, nightAmount);
    float broadHalo = smoothstep(0.925, 0.9990, celestialAlignment);
    float innerHalo = smoothstep(0.982, 0.99945, celestialAlignment);
    float disc = smoothstep(0.99928, 0.99972, celestialAlignment);
    vec3 warmDiscColor = mix(
        celestialColor, vec3(1.0, 0.88, 0.62),
        dayCelestial*0.38);
    sky += warmDiscColor*
        (broadHalo*0.055 + innerHalo*0.17)*
        celestialIntensity;
    sky = mix(
        sky, warmDiscColor*(1.05 + celestialIntensity*0.12),
        disc*mix(0.72, 0.94, dayCelestial));

    vec3 starCoordinates = cubeStarCoordinates(viewDirection);
    vec2 starGrid = starCoordinates.xy*68.0;
    vec2 starCell = floor(starGrid);
    vec2 starLocal = fract(starGrid);
    vec2 faceOffset = vec2(
        starCoordinates.z*137.0,
        starCoordinates.z*59.0);
    float starSeed = hash21(starCell + faceOffset);
    float starExists = smoothstep(0.978, 0.998, starSeed);
    vec2 starPosition = vec2(
        hash21(starCell + faceOffset + vec2(17.3, 4.1)),
        hash21(starCell + faceOffset + vec2(7.7, 29.6)));
    starPosition = mix(vec2(0.16), vec2(0.84), starPosition);
    float starDistance = length(starLocal - starPosition);
    float starSize = mix(
        0.045, 0.090,
        hash21(starCell + faceOffset + vec2(41.2, 11.8)));
    float starAntialias =
        max(fwidth(starDistance)*0.85, 0.012);
    float starCore =
        (1.0 - smoothstep(
            starSize, starSize + starAntialias,
            starDistance))*starExists;
    float starGlow =
        (1.0 - smoothstep(
            starSize + 0.02, starSize + 0.28,
            starDistance))*starExists;
    float twinkle =
        0.72 + 0.28*
        sin(timeSeconds*(1.0 + starSeed*1.6) +
            starSeed*83.0);
    float starVisibility =
        smoothstep(0.12, 0.72, nightAmount)*
        smoothstep(-0.08, 0.16, viewDirection.y);
    vec3 starColor = mix(
        vec3(0.72, 0.82, 1.0),
        vec3(1.0, 0.88, 0.68),
        hash21(starCell + faceOffset + vec2(8.2, 73.1)));
    sky += starColor*
        (starCore*1.45 + starGlow*0.22)*
        twinkle*starVisibility;

    // A faint galactic band gives the night sky large-scale structure behind
    // the stars without making the ground brighter.
    vec3 galacticNormal = normalize(vec3(0.38, 0.72, 0.58));
    float galacticDistance = abs(dot(viewDirection, galacticNormal));
    float galacticBand = exp(-galacticDistance*galacticDistance*58.0);
    float galacticDust = cloudFbm(
        viewDirection.xz*5.4 +
        viewDirection.xy*2.1 + vec2(31.7, -12.4));
    float galaxy = galacticBand*
        smoothstep(0.18, 0.82, galacticDust)*
        smoothstep(-0.05, 0.28, viewDirection.y)*
        smoothstep(0.34, 0.92, nightAmount);
    sky += mix(
        vec3(0.16, 0.24, 0.48),
        vec3(0.42, 0.22, 0.58),
        galacticDust)*galaxy*0.16;

    // A cold lunar halo keeps the moon readable through the richer sky.
    float lunarNight = smoothstep(0.42, 0.90, nightAmount);
    float lunarHalo = pow(
        max(celestialAlignment, 0.0), 24.0)*lunarNight;
    sky += vec3(0.40, 0.58, 1.0)*lunarHalo*0.14;

    // Layered northern curtains. Azimuth is evaluated only inside the north
    // mask, keeping the atan seam behind the camera.
    float auroraNight = smoothstep(0.36, 0.88, nightAmount);
    float auroraNorth =
        smoothstep(-0.10, 0.82, -viewDirection.z);
    float azimuth = atan(viewDirection.x, -viewDirection.z);
    float elevation = viewDirection.y;
    float slowTime = timeSeconds*0.035;
    float auroraWarp = cloudFbm(
        vec2(azimuth*1.35 + slowTime*0.13,
             elevation*3.4 - slowTime*0.035) +
        vec2(13.7, 4.1));
    float lowerEdge =
        0.055 + 0.045*sin(azimuth*2.15 + slowTime) +
        0.032*sin(azimuth*5.3 - slowTime*0.48) +
        (auroraWarp - 0.5)*0.055;
    float curtainBody =
        smoothstep(lowerEdge, lowerEdge + 0.055, elevation)*
        (1.0 - smoothstep(0.56, 0.82, elevation));
    float broadFold = 0.5 + 0.5*sin(
        azimuth*7.5 + auroraWarp*5.0 + slowTime*0.72);
    float fineFold = 0.5 + 0.5*sin(
        azimuth*23.0 - auroraWarp*9.0 - slowTime*1.4);
    float verticalRays = mix(
        0.30 + broadFold*0.70,
        pow(fineFold, 5.0), 0.46);

    float secondWarp = cloudFbm(
        vec2(azimuth*1.8 - slowTime*0.10,
             elevation*4.1 + slowTime*0.025) +
        vec2(-8.3, 19.6));
    float secondEdge =
        0.14 + 0.055*sin(azimuth*3.4 - slowTime*0.62) +
        (secondWarp - 0.5)*0.07;
    float secondCurtain =
        smoothstep(secondEdge, secondEdge + 0.045, elevation)*
        (1.0 - smoothstep(0.48, 0.70, elevation));
    float secondRays = pow(
        0.5 + 0.5*sin(
            azimuth*16.0 + secondWarp*7.0 + slowTime),
        4.0);

    float auroraFolds = clamp(
        curtainBody*verticalRays +
        secondCurtain*secondRays*0.58,
        0.0, 1.0);
    float auroraVeil = smoothstep(
        0.40, 0.76, auroraWarp)*curtainBody;
    float aurora =
        auroraNight*auroraNorth*
        clamp(auroraFolds + auroraVeil*0.30, 0.0, 1.0);
    vec3 auroraGreen = vec3(0.10, 1.00, 0.52);
    vec3 auroraBlue = vec3(0.16, 0.68, 1.00);
    vec3 auroraViolet = vec3(0.72, 0.20, 1.00);
    vec3 auroraColor = mix(
        auroraGreen, auroraBlue,
        smoothstep(0.13, 0.50, elevation));
    auroraColor = mix(
        auroraColor, auroraViolet,
        smoothstep(0.38, 0.68, elevation)*
        (0.26 + secondWarp*0.48));
    sky += auroraColor*aurora*0.88;
    sky += auroraBlue*auroraVeil*
        auroraNight*auroraNorth*0.12;

    sky *= exposure;
    float luminance = dot(sky, vec3(0.2126, 0.7152, 0.0722));
    sky = mix(vec3(luminance), sky, saturation);

    // Alpha is an internal material mask for post-processing.
    // The final composite restores opaque output.
    finalColor = vec4(sky, 0.0);
}
