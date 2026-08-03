#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragVertexColor;
in vec2 fragTexCoord;
in vec4 fragLightSpacePosition;

uniform vec4 baseColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
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
uniform float bakedAo;
uniform float vertexAoAmount;
uniform float aoStrength;
uniform float terrainAmount;
uniform vec3 terrainGrassTint;
uniform vec3 terrainDirtTint;
uniform vec3 terrainRockTint;
uniform sampler2D terrainTexture;
uniform float terrainTextureEnabled;
uniform float hitFlashAmount;
uniform float selectionAmount;
uniform vec3 selectionTint;
uniform sampler2D shadowMap;
uniform float shadowsEnabled;
uniform float constantBias;
uniform float slopeBias;
uniform float shadowStrength;
uniform float shadowMapTexelSize;

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

    float bottomLeft = hash21(cell);
    float bottomRight = hash21(cell + vec2(1.0, 0.0));
    float topLeft = hash21(cell + vec2(0.0, 1.0));
    float topRight = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(bottomLeft, bottomRight, blend.x),
               mix(topLeft, topRight, blend.x), blend.y);
}

vec3 terrainMaterial(vec3 worldPosition, vec3 normal)
{
    vec2 worldXZ = worldPosition.xz;
    float broadNoise = valueNoise(worldXZ*0.045);
    float patchNoise = valueNoise(worldXZ*0.095 + vec2(19.3, -7.1));
    float detailNoise = valueNoise(worldXZ*0.42 + vec2(-31.7, 42.9));
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float mountain = smoothstep(9.0, 28.0, worldPosition.y);

    float rockFromSlope = smoothstep(0.16, 0.48, slope);
    float rockFromHeight = mountain*
        smoothstep(0.18, 0.72, broadNoise + slope*1.35);
    float rockWeight = clamp(
        max(rockFromSlope, rockFromHeight*0.88), 0.0, 1.0);

    float dirtSlopeBand =
        smoothstep(0.045, 0.18, slope)*
        (1.0 - smoothstep(0.24, 0.46, slope));
    float dirtPatch =
        smoothstep(0.68, 0.86, broadNoise*0.72 + patchNoise*0.28)*
        (1.0 - smoothstep(0.10, 0.30, slope));
    float dirtWeight = clamp(
        max(dirtSlopeBand*0.72, dirtPatch*0.66)*
        (1.0 - rockWeight), 0.0, 1.0);

    vec3 textureSample =
        texture(terrainTexture, worldXZ*0.08).rgb;
    float textureLuminance = dot(
        textureSample, vec3(0.2126, 0.7152, 0.0722));
    float grassDetail = mix(
        0.82 + detailNoise*0.24,
        0.72 + textureLuminance*0.48,
        terrainTextureEnabled);
    vec3 grass = terrainGrassTint*grassDetail;

    float dirtGrain =
        0.82 + patchNoise*0.22 + detailNoise*0.10;
    vec3 dirt = terrainDirtTint*dirtGrain;

    float rockNoise = valueNoise(
        worldXZ*0.21 + vec2(8.7, -16.4));
    float rockGrain =
        0.78 + rockNoise*0.30 + detailNoise*0.08;
    vec3 rock = terrainRockTint*rockGrain;

    vec3 grassAndDirt = mix(grass, dirt, dirtWeight);
    return mix(grassAndDirt, rock, rockWeight);
}

float sampleShadow(vec3 normal)
{
    if (shadowsEnabled < 0.5 || fragLightSpacePosition.w <= 0.0)
    {
        return 1.0;
    }

    vec3 projected = fragLightSpacePosition.xyz/fragLightSpacePosition.w;
    projected = projected*0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0 ||
        projected.z <= 0.0 || projected.z >= 1.0)
    {
        return 1.0;
    }

    float lightFacing = max(dot(normal, normalize(-sunDirection)), 0.0);
    float bias = constantBias + slopeBias*(1.0 - lightFacing);
    const float kernel[5] = float[5](1.0, 2.0, 3.0, 2.0, 1.0);
    float occlusion = 0.0;
    float totalWeight = 0.0;
    for (int offsetY = -2; offsetY <= 2; ++offsetY)
    {
        for (int offsetX = -2; offsetX <= 2; ++offsetX)
        {
            float weight = kernel[offsetX + 2]*kernel[offsetY + 2];
            vec2 offset =
                vec2(float(offsetX), float(offsetY))*
                shadowMapTexelSize*1.35;
            float closestDepth = texture(shadowMap, projected.xy + offset).r;
            occlusion +=
                (projected.z - bias > closestDepth ? 1.0 : 0.0)*weight;
            totalWeight += weight;
        }
    }
    occlusion /= totalWeight;
    return 1.0 - occlusion*clamp(shadowStrength, 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 lightDirection = normalize(-sunDirection);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float lightFacing = dot(normal, lightDirection);
    float lambert = max(lightFacing, 0.0);
    float diffuseRamp = smoothstep(-0.08, 0.82, lightFacing);
    float stylizedDiffuse = mix(lambert, diffuseRamp, 0.3);

    float hemisphere = normal.y*0.5 + 0.5;
    vec3 ambientColor = mix(groundAmbientColor, skyAmbientColor, hemisphere);
    float ambientShape = mix(0.78, 1.05, hemisphere);
    vec3 ambient = ambientColor*ambientIntensity*ambientShape;
    float shadow = sampleShadow(normal);
    vec3 direct = sunColor*sunIntensity*stylizedDiffuse*shadow;
    float rim =
        pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)*
        smoothstep(-0.2, 0.75, normal.y);
    vec3 skyRim = skyAmbientColor*ambientIntensity*rim*0.08;

    vec4 albedo = baseColor*colDiffuse*texture(texture0, fragTexCoord)*fragVertexColor;
    albedo.a = baseColor.a*colDiffuse.a*
        mix(fragVertexColor.a, 1.0, clamp(vertexAoAmount, 0.0, 1.0));
    vec3 terrainAlbedo =
        terrainMaterial(fragWorldPosition, normal);
    albedo.rgb = mix(albedo.rgb, terrainAlbedo,
                     clamp(terrainAmount, 0.0, 1.0));
    float vertexAo =
        mix(1.0, fragVertexColor.a, clamp(vertexAoAmount, 0.0, 1.0));
    float aoSource = clamp(bakedAo*vertexAo, 0.0, 1.0);
    float ao = mix(1.0, aoSource, clamp(aoStrength, 0.0, 1.0));
    vec3 litColor = albedo.rgb*(ambient + direct + skyRim)*ao;
    litColor *= dayNightTint;
    litColor = mix(litColor, selectionTint, clamp(selectionAmount, 0.0, 1.0));
    litColor = mix(litColor, vec3(1.0, 0.28, 0.12), clamp(hitFlashAmount, 0.0, 1.0));

    float horizontalDistance =
        length(cameraPosition.xz - fragWorldPosition.xz);
    float fogRange = max(fogEnd - fogStart, 0.01);
    float fogDistance =
        clamp((horizontalDistance - fogStart)/fogRange, 0.0, 1.0);
    float distanceFog =
        (1.0 - exp(-2.5*fogDistance*fogDistance))*0.96;
    float groundDensity = exp(-max(fragWorldPosition.y, 0.0)*0.16);
    float fogAmount =
        distanceFog*mix(0.72, 1.0, groundDensity);
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
        litColor, vec3(preFogLuminance),
        fogAmount*0.16);
    litColor = mix(litColor, atmosphereColor, fogAmount);
    litColor *= exposure;
    float luminance = dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(luminance), litColor, saturation);

    finalColor = vec4(litColor, albedo.a);
}
