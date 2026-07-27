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
uniform vec3 dayNightTint;
uniform float exposure;
uniform float saturation;
uniform float bakedAo;
uniform float vertexAoAmount;
uniform float aoStrength;
uniform float terrainAmount;
uniform vec3 terrainPrimaryTint;
uniform vec3 terrainSecondaryTint;
uniform vec3 terrainPatchTint;
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

vec3 terrainTint(vec3 worldPosition, vec3 normal)
{
    vec2 worldXZ = worldPosition.xz;
    float broadNoise = valueNoise(worldXZ*0.055);
    float mediumNoise = valueNoise(worldXZ*0.14 + vec2(19.3, -7.1));
    float colorBlend = clamp(broadNoise*0.72 + mediumNoise*0.18, 0.0, 1.0);

    float heightBlend = smoothstep(-0.5, 5.0, worldPosition.y);
    colorBlend = clamp(colorBlend + heightBlend*0.14, 0.0, 1.0);
    vec3 tint = mix(terrainPrimaryTint, terrainSecondaryTint, colorBlend);

    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float rarePatchNoise =
        valueNoise(worldXZ*0.038 + vec2(-31.7, 42.9));
    float rarePatch = smoothstep(0.76, 0.91, rarePatchNoise);
    float exposedSlope = smoothstep(0.22, 0.72, slope);
    float patchAmount = clamp(max(rarePatch*0.72, exposedSlope*0.88),
                              0.0, 0.88);
    return mix(tint, terrainPatchTint, patchAmount);
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
    float occlusion = 0.0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            vec2 offset = vec2(float(offsetX), float(offsetY))*shadowMapTexelSize;
            float closestDepth = texture(shadowMap, projected.xy + offset).r;
            occlusion += projected.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    occlusion /= 9.0;
    return 1.0 - occlusion*clamp(shadowStrength, 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 lightDirection = normalize(-sunDirection);
    float lambert = max(dot(normal, lightDirection), 0.0);
    float stylizedDiffuse = mix(lambert, smoothstep(0.05, 0.95, lambert), 0.28);

    float hemisphere = normal.y*0.5 + 0.5;
    vec3 ambientColor = mix(groundAmbientColor, skyAmbientColor, hemisphere);
    vec3 ambient = ambientColor*ambientIntensity;
    float shadow = sampleShadow(normal);
    vec3 direct = sunColor*sunIntensity*stylizedDiffuse*shadow;

    vec4 albedo = baseColor*colDiffuse*texture(texture0, fragTexCoord)*fragVertexColor;
    albedo.a = baseColor.a*
        mix(fragVertexColor.a, 1.0, clamp(vertexAoAmount, 0.0, 1.0));
    vec3 variedTerrain =
        albedo.rgb*terrainTint(fragWorldPosition, normal);
    albedo.rgb = mix(albedo.rgb, variedTerrain,
                     clamp(terrainAmount, 0.0, 1.0));
    float vertexAo =
        mix(1.0, fragVertexColor.a, clamp(vertexAoAmount, 0.0, 1.0));
    float aoSource = clamp(bakedAo*vertexAo, 0.0, 1.0);
    float ao = mix(1.0, aoSource, clamp(aoStrength, 0.0, 1.0));
    vec3 litColor = albedo.rgb*(ambient + direct)*ao;
    litColor *= dayNightTint;
    litColor = mix(litColor, selectionTint, clamp(selectionAmount, 0.0, 1.0));
    litColor = mix(litColor, vec3(1.0, 0.28, 0.12), clamp(hitFlashAmount, 0.0, 1.0));

    float distanceToCamera = distance(cameraPosition, fragWorldPosition);
    float fogAmount = smoothstep(fogStart, max(fogStart + 0.01, fogEnd), distanceToCamera);
    litColor = mix(litColor, fogColor, fogAmount);
    litColor *= exposure;
    float luminance = dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(luminance), litColor, saturation);

    finalColor = vec4(litColor, albedo.a);
}
