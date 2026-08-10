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
uniform float cloudShadowStrength;
uniform float timeSeconds;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogBandsEnabled;
uniform float fogBandCount;
uniform vec3 dayNightTint;
uniform float exposure;
uniform float saturation;
uniform float toonShadingEnabled;
uniform float toonLightSteps;
uniform float bakedAo;
uniform float vertexAoAmount;
uniform float aoStrength;
uniform float terrainAmount;
uniform vec3 terrainGrassTint;
uniform vec3 terrainDirtTint;
uniform sampler2D terrainTexture;
uniform float terrainTextureEnabled;
uniform sampler2D terrainPathMask;
uniform float terrainPathMaskEnabled;
uniform float distantFadeAmount;
uniform float hitFlashAmount;
uniform float selectionAmount;
uniform vec3 selectionTint;
uniform float inkOutlineEligible;
uniform sampler2D shadowMap;
uniform float shadowsEnabled;
uniform float constantBias;
uniform float slopeBias;
uniform float shadowStrength;
uniform float shadowMapTexelSize;

out vec4 finalColor;

float toonRamp(float value, float steps)
{
    float levelCount = max(round(steps), 2.0);
    float scaled = clamp(value, 0.0, 1.0)*(levelCount - 1.0);
    float edgeWidth = max(fwidth(scaled)*0.65, 0.015);
    float band = floor(scaled);
    float transition = smoothstep(
        0.5 - edgeWidth, 0.5 + edgeWidth, fract(scaled));
    return (band + transition)/(levelCount - 1.0);
}

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

float cloudShadowPattern(vec2 worldPosition)
{
    // Large, coherent shapes moving in the same direction as the model
    // clouds. Two broad octaves keep the terrain alive without noisy speckle.
    vec2 wind = vec2(2.20, 0.46)*timeSeconds;
    // Phase chosen so the initial playable area starts across a broad
    // light/shadow boundary instead of inside a large clear patch.
    vec2 position =
        (worldPosition - wind)*0.025 + vec2(6.7, 7.0);
    float broad = valueNoise(position);
    float shape = valueNoise(position*1.92 + vec2(17.4, -8.7));
    float field = broad*0.72 + shape*0.28;
    return smoothstep(0.48, 0.72, field);
}

vec3 terrainMaterial(
    vec3 worldPosition, vec3 normal, vec4 vertexColor)
{
    vec2 worldXZ = worldPosition.xz;
    float broadNoise = valueNoise(worldXZ*0.045);
    float patchNoise = valueNoise(worldXZ*0.095 + vec2(19.3, -7.1));
    float detailNoise = valueNoise(worldXZ*0.42 + vec2(-31.7, 42.9));
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);

    // Steep terrace walls stay dirt-covered. Returning very steep fragments
    // to grass exposed the terrain triangulation as green/brown wedges.
    float dirtSlopeBand =
        smoothstep(0.055, 0.24, slope);
    float dirtPatch =
        smoothstep(0.68, 0.86, broadNoise*0.72 + patchNoise*0.28)*
        (1.0 - smoothstep(0.10, 0.30, slope));
    float dirtWeight = clamp(
        max(dirtSlopeBand*0.86, dirtPatch*0.66), 0.0, 1.0);

    vec3 textureSample =
        texture(terrainTexture, worldXZ*0.08).rgb;
    float textureLuminance = dot(
        textureSample, vec3(0.2126, 0.7152, 0.0722));
    float grassDetail = mix(
        0.82 + detailNoise*0.24,
        1.0,
        terrainTextureEnabled);
    vec3 grass = mix(
        terrainGrassTint*grassDetail,
        textureSample,
        terrainTextureEnabled);

    float sideDetail = valueNoise(vec2(
        worldPosition.y*0.21 + worldPosition.x*0.035,
        worldPosition.z*0.12 - worldPosition.y*0.08));
    float topProjection =
        terrainTextureEnabled*(1.0 - smoothstep(0.10, 0.34, slope));
    float dirtDetail = mix(
        0.78 + patchNoise*0.12 + detailNoise*0.06 + sideDetail*0.16,
        0.68 + textureLuminance*0.52,
        topProjection);
    vec3 dirt = terrainDirtTint*dirtDetail;

    vec3 terrain = mix(grass, dirt, dirtWeight);
    // Terrain chunks encode proximity to water as subtle greyscale vertex
    // darkening. Expand it into a cool damp shoreline material.
    float shoreWeight = clamp(
        (1.0 - vertexColor.r)/(36.0/255.0), 0.0, 1.0);
    float shoreVariation = valueNoise(worldXZ*0.24 + vec2(8.2, -15.7));
    vec3 wetEarth = mix(
        terrainDirtTint*0.54,
        vec3(0.18, 0.27, 0.13),
        shoreVariation*0.34);
    terrain = mix(terrain, wetEarth,
                  shoreWeight*(0.62 + shoreVariation*0.16));

    // Dedicated bilinear mask keeps narrow path edges independent from the
    // terrain triangle grid. Backdrop UVs use another scale, so exclude it.
    float playableTerrain =
        1.0 - step(0.001, abs(vertexColor.r - vertexColor.b));
    float pathWeight = texture(
        terrainPathMask, fragTexCoord).r*
        terrainPathMaskEnabled*playableTerrain;
    float pathDetail = valueNoise(
        worldXZ*0.31 + vec2(-12.4, 26.8));
    vec3 pathTexture = mix(
        vec3(textureLuminance), textureSample, 0.16)*
        vec3(0.68, 0.54, 0.36);
    vec3 pathEarth = mix(
        terrainDirtTint*0.82, pathTexture,
        terrainTextureEnabled*0.82);
    float packedEarth = smoothstep(0.18, 0.90, pathWeight);
    terrain = mix(
        terrain, pathEarth*(0.96 + pathDetail*0.08), packedEarth*0.54);

    // Backdrop vertices encode a mountain amount as R-B. In-map terrain
    // keeps all three channels equal, so this mask cannot turn shoreline
    // pixels into rock. The transition is green at the map edge, then
    // becomes exposed grey stone on increasingly steep mountain faces.
    float mountainAmount = clamp(
        vertexColor.r - vertexColor.b, 0.0, 1.0);
    float rockSlope = smoothstep(0.12, 0.42, slope);
    float rockWeight = clamp(
        mountainAmount*(0.60 + rockSlope*0.40), 0.0, 1.0);
    float rockNoise = valueNoise(
        worldXZ*0.115 + vec2(11.7, -24.6));
    vec3 rock = mix(
        vec3(0.24, 0.27, 0.28),
        vec3(0.56, 0.59, 0.59),
        rockNoise*0.72 + 0.14);
    terrain = mix(terrain, rock, rockWeight);

    // The green channel carries a second, height-and-normal-aware mask for
    // the mountain cap. It is zero on ordinary terrain because mountain
    // amount is zero there, even around wet shore vertices.
    float snowAmount = mountainAmount*clamp(
        1.0 - vertexColor.g, 0.0, 1.0);
    float snowNoise = valueNoise(
        worldXZ*0.072 + vec2(-4.2, 18.1));
    // A narrow threshold makes the snowline visibly crisp while preserving
    // small procedural breaks along the ridge.
    float snowEdge = smoothstep(0.30, 0.50, snowAmount);
    float snowWeight = snowEdge*(0.84 + snowNoise*0.16);
    vec3 snow = mix(
        vec3(0.72, 0.80, 0.86),
        vec3(0.98, 0.995, 1.0),
        snowNoise*0.72 + 0.18);
    return mix(terrain, snow, clamp(snowWeight, 0.0, 1.0));
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
    // A compact 3x3 PCF kernel keeps the shadow edge soft enough while
    // avoiding 16 redundant texture reads per world fragment.
    const float kernel[3] = float[3](1.0, 2.0, 1.0);
    float occlusion = 0.0;
    float totalWeight = 0.0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            float weight = kernel[offsetX + 1]*kernel[offsetY + 1];
            vec2 offset =
                vec2(float(offsetX), float(offsetY))*
                shadowMapTexelSize*2.0;
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
    float diffuseRamp = smoothstep(-0.12, 0.78, lightFacing);
    float stylizedDiffuse = mix(lambert, diffuseRamp, 0.42);
    if (toonShadingEnabled > 0.5)
    {
        stylizedDiffuse = toonRamp(
            stylizedDiffuse, toonLightSteps);
    }

    float hemisphere = normal.y*0.5 + 0.5;
    if (toonShadingEnabled > 0.5)
    {
        hemisphere = toonRamp(
            hemisphere, max(toonLightSteps - 1.0, 2.0));
    }
    vec3 ambientColor = mix(groundAmbientColor, skyAmbientColor, hemisphere);
    float ambientShape = mix(0.86, 1.10, hemisphere);
    vec3 ambient = ambientColor*ambientIntensity*ambientShape;
    ambient += skyAmbientColor*ambientIntensity*0.16;
    float shadow = sampleShadow(normal);
    float cloudLight = 1.0;
    if (terrainAmount > 0.5 && cloudShadowStrength > 0.001)
    {
        float cloudShadow = cloudShadowPattern(fragWorldPosition.xz);
        cloudLight -= cloudShadow*
            clamp(cloudShadowStrength, 0.0, 0.50);
    }
    float sunHighlight = pow(max(lightFacing, 0.0), 2.0)*0.06;
    float silhouetteRim =
        pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.15);
    float lightSideRim =
        smoothstep(-0.24, 0.38, lightFacing);
    float coolLightAmount =
        smoothstep(0.04, 0.22, sunColor.b - sunColor.r);
    float timeOfDayRimStrength =
        mix(0.62, 1.0, coolLightAmount);
    float directionalRim =
        silhouetteRim*lightSideRim*
        mix(0.58, 0.075, clamp(terrainAmount, 0.0, 1.0))*
        timeOfDayRimStrength;
    vec3 direct = sunColor*sunIntensity*
        (stylizedDiffuse + sunHighlight)*shadow;
    float rim =
        pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)*
        smoothstep(-0.2, 0.75, normal.y);
    vec3 skyRim = skyAmbientColor*ambientIntensity*rim*0.08;

    vec4 albedo = baseColor*colDiffuse*texture(texture0, fragTexCoord)*fragVertexColor;
    albedo.a = baseColor.a*colDiffuse.a*
        mix(fragVertexColor.a, 1.0, clamp(vertexAoAmount, 0.0, 1.0));
    vec3 terrainAlbedo = albedo.rgb;
    if (terrainAmount > 0.001)
    {
        terrainAlbedo = terrainMaterial(
            fragWorldPosition, normal, fragVertexColor);
    }
    albedo.rgb = mix(albedo.rgb, terrainAlbedo,
                     clamp(terrainAmount, 0.0, 1.0));
    float vertexAo =
        mix(1.0, fragVertexColor.a, clamp(vertexAoAmount, 0.0, 1.0));
    float aoSource = clamp(bakedAo*vertexAo, 0.0, 1.0);
    float ao = mix(1.0, aoSource, clamp(aoStrength, 0.0, 1.0));
    vec3 litColor = albedo.rgb*(ambient + direct + skyRim)*ao;
    float cloudOcclusion = 1.0 - cloudLight;
    vec3 cloudShadowTint = vec3(
        1.0 - cloudOcclusion*1.06,
        1.0 - cloudOcclusion*1.03,
        1.0 - cloudOcclusion*0.98);
    litColor *= mix(
        vec3(1.0), cloudShadowTint,
        clamp(terrainAmount, 0.0, 1.0));
    vec3 directionalRimColor =
        mix(sunColor, vec3(1.0), 0.24)*sunIntensity;
    litColor += directionalRimColor*directionalRim*
        mix(albedo.rgb, vec3(1.0), 0.42);
    litColor *= dayNightTint;
    float distantFade = clamp(distantFadeAmount, 0.0, 1.0);
    float distantLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 distantColor = mix(
        vec3(distantLuminance), fogColor, 0.46);
    litColor = mix(litColor, distantColor, distantFade*0.64);
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
        fogAmount*0.34);
    litColor = mix(litColor, atmosphereColor, fogAmount);
    litColor *= exposure;
    float luminance = dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(luminance), litColor, saturation);

    finalColor = vec4(litColor,
                      albedo.a*step(0.5, inkOutlineEligible));
}
