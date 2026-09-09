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
    vec4 sourceAlbedo =
        texture(texture0, fragTexCoord)*colDiffuse*grassTint;
    float sourceLuminance = dot(
        sourceAlbedo.rgb, vec3(0.2126, 0.7152, 0.0722));

    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }
    float lightFacing = dot(normal, normalize(-sunDirection));
    float grassRamp = clamp(lightFacing*0.5 + 0.5, 0.0, 1.0);
    float shadowToMid = smoothstep(0.18, 0.62, grassRamp);
    float midToLight = smoothstep(0.68, 0.98, grassRamp);
    const vec3 grassShadow = vec3(0.192, 0.357, 0.208);
    const vec3 grassMid = vec3(0.376, 0.576, 0.231);
    const vec3 grassLight = vec3(0.490875, 0.644875, 0.216125);
    vec3 litColor = mix(grassShadow, grassMid, shadowToMid);
    litColor = mix(litColor, grassLight, midToLight);

    // Preserve restrained authored variation and green contact shading. The
    // texture no longer drives bright lime highlights.
    float textureDetail = mix(
        0.96, 1.035,
        smoothstep(0.08, 0.78, sourceLuminance));
    float normalizedHeight = clamp(
        (fragLocalHeight + 0.05)/0.94, 0.0, 1.0);
    float baseGradient = smoothstep(0.0, 0.28, normalizedHeight);
    litColor *= textureDetail*mix(
        vec3(0.80, 0.88, 0.82), vec3(1.0), baseGradient);
    litColor *= colDiffuse.rgb*grassTint.rgb*dayNightTint;

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
