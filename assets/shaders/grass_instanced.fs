#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPosition;
in float fragLocalHeight;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 grassTint;
uniform vec3 cameraPosition;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 skyAmbientColor;
uniform vec3 groundAmbientColor;
uniform float ambientIntensity;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogBandsEnabled;
uniform float fogBandCount;
uniform vec3 dayNightTint;
uniform float exposure;
uniform float saturation;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

float hash21(vec2 position)
{
    return fract(sin(dot(position, vec2(127.1, 311.7)))*43758.5453);
}

float valueNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 local = fract(position);
    vec2 blend = local*local*(3.0 - 2.0*local);
    float bottomLeft = hash21(cell);
    float bottomRight = hash21(cell + vec2(1.0, 0.0));
    float topLeft = hash21(cell + vec2(0.0, 1.0));
    float topRight = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(bottomLeft, bottomRight, blend.x),
               mix(topLeft, topRight, blend.x), blend.y);
}

vec2 terrainBiomeFields(vec2 worldPosition)
{
    vec2 broadPosition = worldPosition*0.0135;
    vec2 warp = vec2(
        valueNoise(broadPosition*0.72 + vec2(18.7, -6.4)),
        valueNoise(broadPosition*0.72 + vec2(-9.1, 27.3))) - 0.5;
    vec2 warped = broadPosition + warp*0.56;
    return vec2(
        valueNoise(warped + vec2(4.8, 13.2)),
        valueNoise(warped*0.91 + vec2(-17.5, 8.6)));
}

void main()
{
    vec4 albedo =
        texture(texture0, fragTexCoord)*colDiffuse*grassTint;
    // Grass uses cheap authored contact shading. Only lower 25-30% darkens;
    // individual blades stay out of SSAO, avoiding noisy depth speckles.
    float normalizedHeight = clamp(
        (fragLocalHeight + 0.05)/0.94, 0.0, 1.0);
    float baseGradient = smoothstep(0.0, 0.28, normalizedHeight);
    albedo.rgb *= mix(
        vec3(0.62, 0.72, 0.66), vec3(1.0), baseGradient);
    float luminance =
        dot(albedo.rgb, vec3(0.2126, 0.7152, 0.0722));
    albedo.rgb =
        mix(vec3(luminance), albedo.rgb, 0.82)*
        vec3(1.045, 0.99, 0.91);
    vec2 biome = terrainBiomeFields(fragWorldPosition.xz);
    float clearing = smoothstep(0.60, 0.78, biome.x)*
        (1.0 - smoothstep(0.38, 0.68, biome.y));
    float grove = smoothstep(0.59, 0.79, biome.y)*
        (1.0 - clearing*0.80);
    vec3 mutedGrass = mix(vec3(luminance), albedo.rgb, 0.70);
    albedo.rgb = mix(
        albedo.rgb,
        mutedGrass*vec3(1.10, 1.055, 0.93),
        clearing*0.42);
    albedo.rgb = mix(
        albedo.rgb,
        mutedGrass*vec3(0.77, 0.88, 0.97),
        grove*0.46);
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }
    float lightFacing =
        dot(normal, normalize(-sunDirection));
    float diffuse =
        mix(max(lightFacing, 0.0),
            smoothstep(-0.02, 0.78, lightFacing), 0.22);
    float hemisphere = normal.y*0.5 + 0.5;
    vec3 ambient =
        mix(groundAmbientColor, skyAmbientColor, hemisphere)*
        ambientIntensity*mix(0.90, 1.10, hemisphere);
    ambient += skyAmbientColor*ambientIntensity*0.25;
    float backFacingFill = 1.0 - smoothstep(
        -0.24, 0.34, lightFacing);
    vec3 coolShadowFill = mix(
        groundAmbientColor, skyAmbientColor, 0.78);
    ambient += coolShadowFill*ambientIntensity*
        (0.10 + backFacingFill*0.18);
    float leafTransmission =
        max(-lightFacing, 0.0)*0.16;
    vec3 viewDirection =
        normalize(cameraPosition - fragWorldPosition);
    float silhouetteRim =
        pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.25);
    float lightSideRim =
        smoothstep(-0.24, 0.38, lightFacing);
    float coolLightAmount =
        smoothstep(0.04, 0.22, sunColor.b - sunColor.r);
    float timeOfDayRimStrength =
        mix(0.62, 1.0, coolLightAmount);
    float directionalRim =
        silhouetteRim*lightSideRim*0.34*timeOfDayRimStrength;
    vec3 direct =
        sunColor*sunIntensity*
        (diffuse + leafTransmission + directionalRim)*0.88;
    vec3 litColor = albedo.rgb*(ambient + direct)*dayNightTint;
    vec3 directionalRimColor =
        mix(sunColor, vec3(1.0), 0.20)*sunIntensity;
    litColor += directionalRimColor*directionalRim*
        mix(albedo.rgb, vec3(1.0), 0.34)*dayNightTint;

    float horizontalDistance =
        length(cameraPosition.xz - fragWorldPosition.xz);
    // Blades retain their silhouette nearby, then shed local color contrast
    // before fog takes over. This avoids noisy high-frequency vegetation in
    // the middle and far planes.
    float detailFade = smoothstep(22.0, 58.0, horizontalDistance);
    float detailLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 calmGrass = mix(
        vec3(detailLuminance)*vec3(0.88, 0.96, 1.0),
        litColor, 0.58);
    litColor = mix(litColor, calmGrass, detailFade*0.48);
    float fogRange = max(fogEnd - fogStart, 0.01);
    float fogDistance =
        clamp((horizontalDistance - fogStart)/fogRange, 0.0, 1.0);
    float fogAmount =
        (1.0 - exp(-2.15*fogDistance*fogDistance))*0.74;
    if (fogBandsEnabled > 0.5)
    {
        float bands = max(round(fogBandCount), 2.0);
        fogAmount = floor(fogAmount*(bands - 1.0) + 0.5)/
            (bands - 1.0);
    }
    vec3 atmosphereColor =
        mix(fogColor, skyAmbientColor, 0.14 + fogDistance*0.08);
    float preFogLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(
        litColor, vec3(preFogLuminance), fogAmount*0.14);
    litColor = mix(litColor, atmosphereColor, fogAmount);
    litColor *= exposure;
    float litLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(litLuminance), litColor, saturation);
    // Scene alpha doubles as a lightweight material tag for the ink pass.
    // 0.125 uniquely marks grass: unlike sky (0) and regular geometry (1),
    // it lets the post-process suppress both sides of a grass boundary.
    finalColor = vec4(litColor, 0.125);
    normalAo = vec4(0.0);
}
