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

float cloudField(vec2 position)
{
    float body = cloudFbm(position);
    float erosion = valueNoise(position*5.3 + vec2(4.7, 12.9));
    return body*0.88 + erosion*0.12;
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

    // Seamless northern aurora. World-space direction keeps the curtain
    // stable while the camera rotates; no longitude UV seam is involved.
    float auroraNight = smoothstep(0.46, 0.92, nightAmount);
    float auroraNorth =
        smoothstep(-0.18, 0.70, -viewDirection.z);
    float auroraAltitude =
        smoothstep(0.015, 0.13, viewDirection.y)*
        (1.0 - smoothstep(0.48, 0.78, viewDirection.y));
    vec2 auroraDrift =
        vec2(timeSeconds*0.006, -timeSeconds*0.002);
    float auroraWarp = cloudFbm(
        viewDirection.xz*2.8 + auroraDrift + vec2(13.7, 4.1));
    float auroraCoordinate =
        viewDirection.x*31.0 + viewDirection.z*5.0 +
        auroraWarp*8.0 + timeSeconds*0.055;
    float broadCurtain =
        0.58 + 0.42*sin(auroraCoordinate*0.42);
    float fineCurtain =
        0.5 + 0.5*sin(auroraCoordinate*1.65 + auroraWarp*5.0);
    float auroraFolds =
        mix(broadCurtain, pow(fineCurtain, 3.0), 0.48);
    float auroraVeil = smoothstep(
        0.32, 0.76,
        valueNoise(viewDirection.xz*6.0 + auroraDrift*2.0));
    float aurora =
        auroraNight*auroraNorth*auroraAltitude*
        clamp(auroraFolds*0.78 + auroraVeil*0.35, 0.0, 1.0);
    vec3 auroraGreen = vec3(0.20, 1.00, 0.58);
    vec3 auroraBlue = vec3(0.24, 0.72, 1.00);
    vec3 auroraViolet = vec3(0.68, 0.30, 1.00);
    vec3 auroraColor = mix(
        auroraGreen, auroraBlue,
        smoothstep(0.16, 0.48, viewDirection.y));
    auroraColor = mix(
        auroraColor, auroraViolet,
        smoothstep(0.46, 0.70, viewDirection.y)*
        (0.35 + auroraWarp*0.35));
    sky += auroraColor*aurora*0.62;

    // Project onto a rounded dome instead of a flat plane. The positive
    // denominator keeps clouds puffy near the horizon and avoids a visible
    // transition seam there. Fade finishes below the useful sky area.
    float cloudAltitude = smoothstep(-0.16, 0.035, viewDirection.y);
    vec2 cloudPosition =
        viewDirection.xz/max(viewDirection.y + 0.34, 0.12)*0.78;
    vec2 wind = vec2(timeSeconds*0.010, timeSeconds*0.0035);
    cloudPosition += wind;

    // Two nearby cloud layers provide slow parallax and a thicker silhouette.
    float lowerField = cloudField(cloudPosition);
    float upperField = cloudField(
        cloudPosition*1.08 + vec2(6.2, -3.7) - wind*0.16);
    float volumeField = lowerField*0.70 + upperField*0.30;
    float cloudBody = smoothstep(0.50, 0.63, volumeField);
    float cloudCore = smoothstep(0.62, 0.78, volumeField);
    float cloudEdge =
        smoothstep(0.47, 0.53, volumeField) -
        smoothstep(0.57, 0.66, volumeField);
    float cloudWisps =
        smoothstep(0.43, 0.58, upperField)*
        (1.0 - cloudBody)*0.24;
    float cloudNightFade =
        1.0 - smoothstep(0.44, 0.94, nightAmount);
    float clouds =
        clamp(cloudBody + cloudWisps, 0.0, 1.0)*
        cloudAltitude*cloudNightFade;

    vec2 lightDirection = normalize(
        celestialDirection.xz + vec2(0.0001));
    float lightProbe = cloudField(
        cloudPosition + lightDirection*0.16);
    float sunFacing = clamp(
        (volumeField - lightProbe)*5.5 +
        dot(normalize(celestialDirection), viewDirection)*0.16,
        0.0, 1.0);
    float silverLining =
        cloudEdge*(0.32 + sunFacing*0.68);

    vec3 dayCloudShadow =
        mix(horizonColor, vec3(0.78, 0.82, 0.83), 0.78);
    vec3 dayCloudLight =
        mix(vec3(1.00, 1.00, 0.98), celestialColor, 0.12);
    vec3 nightCloudShadow =
        mix(lowerSkyColor, vec3(0.43, 0.49, 0.60), 0.72);
    vec3 nightCloudLight =
        mix(vec3(0.66, 0.72, 0.84), celestialColor, 0.24);
    vec3 cloudShadow =
        mix(dayCloudShadow, nightCloudShadow, nightAmount);
    vec3 cloudLight =
        mix(dayCloudLight, nightCloudLight, nightAmount);
    float cloudLighting =
        clamp(0.58 + sunFacing*0.30 + (1.0 - cloudCore)*0.12,
              0.0, 1.0);
    vec3 cloudColor = mix(cloudShadow, cloudLight, cloudLighting);
    cloudColor += cloudLight*silverLining*0.78;
    sky = mix(sky, cloudColor, clouds*0.76);

    sky *= exposure;
    float luminance = dot(sky, vec3(0.2126, 0.7152, 0.0722));
    sky = mix(vec3(luminance), sky, saturation);

    // Alpha is an internal material mask for post-processing.
    // The final composite restores opaque output.
    finalColor = vec4(sky, 0.0);
}
