#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragVertexColor;
in vec2 fragTexCoord;
in vec4 fragLightSpacePosition;
in vec4 fragTerrainNoiseA;
in vec4 fragTerrainNoiseB;

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
uniform float screenAoAmount;
uniform float aoStrength;
uniform float terrainAmount;
uniform vec3 terrainGrassTint;
uniform vec3 terrainDirtTint;
uniform sampler2D terrainTexture;
uniform float terrainTextureEnabled;
uniform float groundTextureScale;
uniform float groundTextureStrength;
uniform float groundSaturation;
uniform float groundBrightness;
uniform vec3 groundTextureTint;
uniform float groundTextureContrast;
uniform float groundHighlightCompression;
uniform float groundSecondaryBlend;
uniform float groundMacroVariation;
uniform sampler2D terrainPathMask;
uniform float terrainPathMaskEnabled;
uniform float distantFadeAmount;
uniform float vegetationAmount;
uniform float treeAmount;
uniform vec3 treeLeafShadowColor;
uniform vec3 treeLeafMidColor;
uniform vec3 treeLeafLightColor;
uniform vec3 treeTrunkShadowColor;
uniform vec3 treeTrunkMidColor;
uniform vec3 treeTrunkLightColor;
uniform float treeShadowThreshold;
uniform float treeHighlightThreshold;
uniform float treeRampSmoothness;
uniform float treeHighlightStrength;
uniform float treeTextureVariation;
uniform float treeLeafFlatness;
uniform float bushAmount;
uniform vec3 bushShadowColor;
uniform vec3 bushMidColor;
uniform vec3 bushLightColor;
uniform float bushShadowThreshold;
uniform float bushHighlightThreshold;
uniform float bushRampSmoothness;
uniform float bushHighlightStrength;
uniform float coniferAmount;
uniform vec3 coniferShadowColor;
uniform vec3 coniferMidColor;
uniform vec3 coniferLightColor;
uniform float coniferShadowThreshold;
uniform float coniferHighlightThreshold;
uniform float coniferRampSmoothness;
uniform float coniferHighlightStrength;
uniform float coniferFacetBlend;
uniform float hitFlashAmount;
uniform float enemyWhiteFlashAmount;
uniform vec3 hitFlashPosition;
uniform float hitFlashRadius;
uniform float selectionAmount;
uniform vec3 selectionTint;
uniform float ghostAmount;
uniform vec3 ghostTint;
uniform float ghostOpacity;
uniform float inkOutlineEligible;
uniform float inkOccluder;
uniform float unlitAmount;
uniform float outlineMaterialClassOverride;
uniform sampler2D shadowMap;
uniform float shadowsEnabled;
uniform float constantBias;
uniform float slopeBias;
uniform float shadowStrength;
uniform float shadowMapTexelSize;
uniform int shadowSampleCount;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

in vec4 fragBiomeWeights;
uniform vec4 worldBiomeShape;
uniform vec3 worldBiomeGround[4];

vec3 visualBiomeColor(vec3 source, vec3 palette[4], vec3 greenReference)
{
    if (worldBiomeShape.x <= 0.0) return source;
    vec4 weights = max(fragBiomeWeights, vec4(0.0));
    weights /= max(dot(weights, vec4(1.0)), 0.0001);
    const vec3 luminance = vec3(0.2126, 0.7152, 0.0722);
    float detail = clamp(dot(source, luminance)/dot(greenReference, luminance), 0.35, 1.55);
    // Retain the approved green material and its shading; other zones replace
    // hue while preserving the same painted details and light/shadow ramp.
    return source*(palette[0]/greenReference)*weights.x + detail*(
        palette[1]*weights.y + palette[2]*weights.z + palette[3]*weights.w);
}

uniform vec3 worldBiomeLeaves[4];

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

vec2 terrainBiomeFields(vec2 worldPosition)
{
    // Broad warped fields form readable regions tens of metres wide. They
    // are deliberately much larger than the surface texture detail.
    vec2 broadPosition = worldPosition*0.0135;
    vec2 warp = vec2(
        valueNoise(broadPosition*0.72 + vec2(18.7, -6.4)),
        valueNoise(broadPosition*0.72 + vec2(-9.1, 27.3))) - 0.5;
    vec2 warped = broadPosition + warp*0.56;
    return vec2(
        valueNoise(warped + vec2(4.8, 13.2)),
        valueNoise(warped*0.91 + vec2(-17.5, 8.6)));
}

vec3 painterlyGroundMaterial(
    vec3 worldPosition, vec3 normal, vec4 vertexColor, vec2 pathUv)
{
    vec2 worldXZ = worldPosition.xz;
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float textureScale = max(groundTextureScale, 0.0001);

    // Two continuous world-space projections break the square repetition.
    // Unlike per-tile rotations, both projections remain seamless.
    vec2 primaryUv = worldXZ*textureScale;
    mat2 secondaryRotation = mat2(0.8, -0.6, 0.6, 0.8);
    vec2 secondaryUv =
        secondaryRotation*worldXZ*(textureScale*0.73) +
        vec2(17.37, 9.13);
    vec3 primary = texture(terrainTexture, primaryUv).rgb;
    vec3 secondary = texture(terrainTexture, secondaryUv).rgb;
    vec3 grass = mix(
        primary, secondary,
        clamp(groundSecondaryBlend, 0.0, 0.35));

    float luminance = dot(grass, vec3(0.2126, 0.7152, 0.0722));
    grass = mix(
        vec3(luminance), grass,
        clamp(groundSaturation, 0.0, 1.25));
    // Pull fine painted values toward the texture's olive mean. Large color
    // regions survive, while close-up detail no longer reads as carpet pile.
    const vec3 authoredGrassMean = vec3(0.49, 0.58, 0.18);
    grass = mix(
        authoredGrassMean, grass,
        clamp(groundTextureContrast, 0.0, 1.25));
    grass *= groundTextureTint;

    // Compress only the yellow-lime peaks; mid and dark patches keep their
    // separation and the approved world-space pattern remains unchanged.
    float tintedLuminance = dot(
        grass, vec3(0.2126, 0.7152, 0.0722));
    float highlightAmount = smoothstep(0.48, 0.72, tintedLuminance);
    grass *= 1.0 - highlightAmount*
        clamp(groundHighlightCompression, 0.0, 0.35);
    grass *= max(groundBrightness, 0.0);

    // Very broad world-space fields create metre-scale composition without
    // replacing or rescaling the approved painterly micro texture.
    vec2 macroFields = fragTerrainNoiseA.xy;
    float macroStrength = clamp(groundMacroVariation, 0.0, 0.10);
    float oliveRegion = smoothstep(0.18, 0.82, macroFields.x);
    float greenerRegion = smoothstep(0.68, 0.90, 1.0 - macroFields.x)*
        smoothstep(0.22, 0.72, macroFields.y);
    float dryRegion = smoothstep(0.73, 0.91, macroFields.y)*
        (1.0 - smoothstep(0.52, 0.82, macroFields.x));
    grass *= mix(
        1.0 - macroStrength*0.70,
        1.0 + macroStrength*0.65,
        macroFields.x);
    grass *= vec3(
        1.0 + macroStrength*oliveRegion*0.45,
        1.0 - macroStrength*oliveRegion*0.18,
        1.0 - macroStrength*oliveRegion*0.75);
    grass *= vec3(
        1.0 - macroStrength*greenerRegion*0.28,
        1.0 + macroStrength*greenerRegion*0.22,
        1.0 + macroStrength*greenerRegion*0.08);
    grass = mix(grass, vec3(0.565, 0.510, 0.255),
                macroStrength*dryRegion*0.86);

    // Wide, irregular bare patches give the open field a readable material
    // rhythm. Their scale is measured in world space, so they do not crawl
    // with the camera or reveal the terrain triangles.
    vec2 wearFields = fragTerrainNoiseA.zw;
    float flatGround = 1.0 - smoothstep(0.035, 0.115, slope);
    float barePatch = smoothstep(0.595, 0.715, wearFields.x)*
        flatGround;
    float bareEdge = smoothstep(0.24, 0.74, wearFields.y);
    barePatch *= mix(0.70, 1.0, bareEdge);
    float earthDetail = fragTerrainNoiseB.x;
    float grassValue = dot(
        grass, vec3(0.2126, 0.7152, 0.0722));
    vec3 exposedEarth = mix(
        terrainDirtTint*vec3(0.98, 1.00, 0.98),
        vec3(0.48, 0.355, 0.205),
        0.36 + earthDetail*0.30);
    exposedEarth *= 0.92 + earthDetail*0.17;
    exposedEarth = mix(
        exposedEarth,
        exposedEarth*(0.84 + grassValue*0.22),
        0.28);
    grass = mix(grass, exposedEarth, barePatch*0.88);

    // Slopes expose streaks of warm subsoil, then cooler stone as they get
    // steeper. Two world-space projections prevent a single uniform brown
    // band around every terrace.
    float slopeField = fragTerrainNoiseB.y;
    float slopeDetail = fragTerrainNoiseB.z;
    float slopeBreakup = smoothstep(
        0.24, 0.78, slopeField*0.68 + slopeDetail*0.32);
    float soilSlope = smoothstep(0.025, 0.115, slope)*
        (1.0 - smoothstep(0.20, 0.42, slope));
    float slopeSoilWeight = soilSlope*mix(0.28, 0.76, slopeBreakup);
    vec3 slopeSoil = terrainDirtTint*
        mix(vec3(1.00, 1.04, 1.03), vec3(1.18, 1.06, 0.90),
            slopeDetail);
    slopeSoil *= 0.88 + slopeField*0.20;
    grass = mix(grass, slopeSoil, slopeSoilWeight);

    float steepSlope = smoothstep(0.13, 0.33, slope);
    float rockBreakup = smoothstep(
        0.38, 0.72, slopeField*0.42 + slopeDetail*0.58);
    vec3 slopeRock = mix(
        vec3(0.35, 0.355, 0.325),
        vec3(0.46, 0.415, 0.335),
        slopeDetail);
    grass = mix(
        grass, slopeRock,
        steepSlope*mix(0.48, 0.82, rockBreakup));

    // Existing terrain vertices already encode a soft 3.6 m shoreline
    // proximity. Apply it only in irregular broad sections so the lake edge
    // gains damp/worn variation without becoming a uniform painted ring.
    float shoreWeight = clamp(
        (1.0 - vertexColor.r)/(36.0/255.0), 0.0, 1.0);
    float shorePatch = smoothstep(
        0.40, 0.72, macroFields.y);
    vec3 dampOlive = mix(
        grass, vec3(0.300, 0.365, 0.205), 0.34);
    grass = mix(
        grass, dampOlive,
        shoreWeight*shorePatch*0.42);

    // The cached bilinear path mask supplies a soft irregular shoulder and
    // a packed-earth core. Only broad color regions are used: no second dirt
    // texture and no baked lighting.
    float pathWeight = texture(terrainPathMask, pathUv).r*
        terrainPathMaskEnabled;
    float wornShoulder = smoothstep(0.06, 0.64, pathWeight);
    float packedCore = smoothstep(0.43, 0.91, pathWeight);
    float pathTone = mix(macroFields.x, macroFields.y, 0.42);
    // Derive most of the wear from the already textured grass. This keeps the
    // approved micro pattern visible and prevents a broad constant-color slab.
    const vec3 pathEdge = vec3(0.447, 0.518, 0.263); // #728443
    const vec3 pathMain = vec3(0.506, 0.475, 0.290); // #81794A
    const vec3 pathDry = vec3(0.596, 0.518, 0.298);  // #98844C
    float grassLuminance = dot(
        grass, vec3(0.2126, 0.7152, 0.0722));
    vec3 trampledGrass = mix(
        vec3(grassLuminance), grass, 0.74);
    trampledGrass *= vec3(1.025, 0.97, 0.84);
    trampledGrass = mix(trampledGrass, pathEdge, 0.12);
    grass = mix(grass, trampledGrass, wornShoulder*0.80);

    // The olive-earth center remains narrow and translucent. Only selected
    // broad patches receive the lighter dry accent.
    float dryCenterPatch = smoothstep(0.68, 0.88, pathTone)*
        smoothstep(0.62, 0.92, pathWeight);
    vec3 oliveCenter = mix(grass, pathMain, 0.30);
    grass = mix(grass, oliveCenter, packedCore*0.78);
    grass = mix(grass, pathDry, dryCenterPatch*0.17);

    float textureWeight = clamp(
        terrainTextureEnabled*groundTextureStrength, 0.0, 1.0);
    return visualBiomeColor(mix(terrainGrassTint, grass, textureWeight),
        worldBiomeGround, vec3(0.44, 0.50, 0.24));
}

vec3 terrainMaterial(
    vec3 worldPosition, vec3 normal, vec4 vertexColor)
{
    vec2 worldXZ = worldPosition.xz;
    float broadNoise = valueNoise(worldXZ*0.045);
    float patchNoise = valueNoise(worldXZ*0.095 + vec2(19.3, -7.1));
    float detailNoise = valueNoise(worldXZ*0.42 + vec2(-31.7, 42.9));
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    // Backdrop vertices encode a mountain band in R-B. Ordinary playable
    // terrain, including shoreline tinting, remains greyscale.
    float playableTerrain =
        1.0 - step(0.001, abs(vertexColor.r - vertexColor.b));

    // Steep terrace walls stay dirt-covered. Returning very steep fragments
    // to grass exposed the terrain triangulation as green/brown wedges.
    float dirtSlopeBand =
        smoothstep(0.045, 0.16, slope);
    float dirtPatch =
        smoothstep(0.68, 0.86, broadNoise*0.72 + patchNoise*0.28)*
        (1.0 - smoothstep(0.10, 0.30, slope));
    float dirtWeight = clamp(
        max(dirtSlopeBand*0.96, dirtPatch*0.66), 0.0, 1.0);

    // The authored texture supplies the cellular shapes. A continuous,
    // low-frequency UV warp breaks the square repetition without seams and
    // is substantially cheaper than evaluating Worley per terrain pixel.
    vec2 grassUvWarp = vec2(
        broadNoise - 0.5,
        patchNoise - 0.5);
    vec2 grassUv = worldXZ*0.106667 + grassUvWarp*0.075;
    vec3 textureSample = texture(terrainTexture, grassUv).rgb;
    float textureLuminance = dot(
        textureSample, vec3(0.2126, 0.7152, 0.0722));
    // The texture is authored as final grass albedo. Do not reinterpret it
    // as a mask or replace its yellow-green palette with the old dark tint.
    // The regular world lighting, shadows and atmosphere are applied later.
    vec3 grass = mix(
        terrainGrassTint, textureSample,
        clamp(terrainTextureEnabled, 0.0, 1.0));

    float sideDetail = valueNoise(vec2(
        worldPosition.y*0.21 + worldPosition.x*0.035,
        worldPosition.z*0.12 - worldPosition.y*0.08));
    float topProjection =
        terrainTextureEnabled*(1.0 - smoothstep(0.06, 0.18, slope));
    float dirtDetail = mix(
        0.78 + patchNoise*0.12 + detailNoise*0.06 + sideDetail*0.16,
        0.68 + textureLuminance*0.52,
        topProjection);
    vec3 dirt = terrainDirtTint*dirtDetail;

    vec3 terrain = mix(grass, dirt, dirtWeight);
    float stableTerraceWall = smoothstep(0.06, 0.20, slope);
    vec3 warmTerraceEarth = mix(
        terrainGrassTint*vec3(1.05, 0.88, 0.68),
        terrainDirtTint*vec3(1.16, 1.08, 0.94),
        smoothstep(0.04, 0.30, slope));
    warmTerraceEarth *= 0.92 + sideDetail*0.10;
    terrain = mix(
        terrain, warmTerraceEarth,
        stableTerraceWall*0.88);
    // Terrain chunks encode proximity to water as subtle greyscale vertex
    // darkening. Expand it into a cool damp shoreline material.
    float shoreWeight = clamp(
        (1.0 - vertexColor.r)/(36.0/255.0), 0.0, 1.0);
    float shoreVariation = valueNoise(worldXZ*0.24 + vec2(8.2, -15.7));
    vec3 wetEarth = mix(
        vec3(0.19, 0.245, 0.19),
        vec3(0.13, 0.205, 0.19),
        shoreVariation*0.58);
    terrain = mix(terrain, wetEarth,
                  shoreWeight*(0.68 + shoreVariation*0.16));

    // Four broad visual regions break up the otherwise uniform olive field.
    // Water remains driven by the authored shoreline mask; the other zones
    // fade through low-frequency fields so no hard biome borders appear.
    vec2 biome = terrainBiomeFields(worldXZ);
    float flatGround = 1.0 - smoothstep(0.08, 0.24, slope);
    float clearing = smoothstep(0.60, 0.78, biome.x)*
        flatGround*(1.0 - shoreWeight)*
        (1.0 - smoothstep(0.38, 0.68, biome.y));
    float grove = smoothstep(0.59, 0.79, biome.y)*
        (1.0 - clearing*0.80)*(1.0 - shoreWeight*0.74);
    float highGround = smoothstep(2.2, 7.6, worldPosition.y);
    float highlandField = valueNoise(
        worldXZ*0.020 + vec2(31.4, -22.8));
    float rockyHighland = smoothstep(0.48, 0.72, highlandField)*
        highGround*(0.46 + slope*0.74)*
        (1.0 - shoreWeight);

    float terrainLuminance = dot(
        terrain, vec3(0.2126, 0.7152, 0.0722));
    vec3 meadowColor = mix(
        vec3(terrainLuminance), terrain, 0.78)*
        vec3(1.15, 1.10, 0.92);
    vec3 groveColor = mix(
        vec3(terrainLuminance), terrain, 0.61)*
        vec3(0.76, 0.88, 0.96);
    float highlandNoise = valueNoise(
        worldXZ*0.13 + vec2(-15.2, 37.1));
    vec3 highlandColor = mix(
        vec3(0.32, 0.335, 0.30),
        vec3(0.46, 0.405, 0.31),
        highlandNoise);
    terrain = mix(terrain, meadowColor, clearing*0.56);
    terrain = mix(terrain, groveColor, grove*0.54);
    terrain = mix(terrain, highlandColor, rockyHighland*0.64);

    // Dedicated bilinear mask keeps narrow path edges independent from the
    // terrain triangle grid. Backdrop UVs use another scale, so exclude it.
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

    // Terrain vertex R-B encodes the mountain band on the raised boundary
    // and backdrop. Shore vertices remain greyscale, so water-darkening
    // cannot accidentally become rock.
    float mountainAmount = clamp(
        vertexColor.r - vertexColor.b, 0.0, 1.0);
    float rockSlope = smoothstep(0.12, 0.42, slope);
    float rockWeight = clamp(
        mountainAmount*(0.92 + rockSlope*0.08), 0.0, 1.0);
    float rockNoise = valueNoise(
        worldXZ*0.115 + vec2(11.7, -24.6));
    float rockMass = valueNoise(
        worldXZ*0.028 + vec2(-7.4, 16.9));
    float weathering = valueNoise(vec2(
        worldPosition.x*0.038 + worldPosition.y*0.052,
        worldPosition.z*0.038 - worldPosition.y*0.031));
    vec3 rock = mix(
        vec3(0.22, 0.27, 0.30),
        vec3(0.48, 0.52, 0.55),
        0.22 + rockMass*0.58);
    rock *= 0.88 + rockNoise*0.13;
    rock = mix(rock, rock*vec3(0.72, 0.78, 0.80),
               smoothstep(0.58, 0.86, weathering)*0.34);
    vec3 treeLineRock = vec3(0.25, 0.30, 0.32);
    rock = mix(
        treeLineRock, rock,
        smoothstep(0.18, 0.58, mountainAmount));
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
        vec3(0.66, 0.72, 0.75),
        vec3(0.88, 0.90, 0.89),
        snowNoise*0.72 + 0.18);
    vec3 result = mix(
        terrain, snow, clamp(snowWeight, 0.0, 1.0));
    // Restore middle-value readability below the mountain band while the
    // raised boundary and backdrop retain a deeper silhouette.
    return result*mix(1.10, 1.0, mountainAmount);
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
    // Nested disc-shaped PCF kernels let quality presets reduce the dominant
    // foliage fill-rate cost without changing shadow resolution. The first
    // five samples form a balanced kernel; High adds the outer detail taps.
    const vec2 disc[9] = vec2[9](
        vec2( 0.00,  0.00),
        vec2(-0.34, -0.91),
        vec2( 0.82, -0.57),
        vec2(-0.91,  0.24),
        vec2( 0.43,  0.86),
        vec2(-0.69, -0.32),
        vec2( 0.19, -0.68),
        vec2( 0.72,  0.31),
        vec2(-0.24,  0.70));
    float occlusion = 0.0;
    float totalWeight = 0.0;
    int effectiveSampleCount = terrainAmount > 0.5
        ? min(shadowSampleCount, 5)
        : shadowSampleCount;
    for (int sampleIndex = 0; sampleIndex < 9; ++sampleIndex)
    {
        if (sampleIndex >= effectiveSampleCount)
        {
            break;
        }
        float centerWeight = sampleIndex == 0 ? 1.55 : 1.0;
        // A one-texel kernel keeps the cast shadow crisp. Bilinear depth
        // sampling still smooths its sub-texel stair steps.
        vec2 offset = disc[sampleIndex]*shadowMapTexelSize*1.0;
        float closestDepth = texture(
            shadowMap, projected.xy + offset).r;
        occlusion +=
            (projected.z - bias > closestDepth ? 1.0 : 0.0)*centerWeight;
        totalWeight += centerWeight;
    }
    occlusion /= totalWeight;
    // Fade before the orthographic map border so a moving camera never
    // exposes a hard rectangular cutoff.
    vec2 mapEdge = abs(projected.xy*2.0 - 1.0);
    float edgeFade = 1.0 - smoothstep(
        0.72, 0.98, max(mapEdge.x, mapEdge.y));
    // Keep cast shadows readable. With the normal 0.58 setting direct-light
    // occlusion stays near thirty percent instead of collapsing to black.
    float artisticStrength = clamp(shadowStrength, 0.0, 1.0)*0.52;
    return 1.0 - occlusion*artisticStrength*edgeFade;
}

vec3 normalizedLightingTint(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return clamp(color/max(luminance, 0.001), vec3(0.72), vec3(1.24));
}

vec3 applyStylizedWorldLight(
    vec3 materialColor, float directAmount,
    float hemisphere, float authoredRampAmount)
{
    float skyAmount = mix(0.35, 1.0, clamp(hemisphere, 0.0, 1.0));
    vec3 ambientSource = mix(
        groundAmbientColor, skyAmbientColor, skyAmount);
    vec3 coolFillTint = mix(
        vec3(1.0), normalizedLightingTint(ambientSource), 0.55);
    vec3 warmSunTint = mix(
        vec3(1.0), normalizedLightingTint(sunColor), 0.48);

    float ambientScale = clamp(
        ambientIntensity/0.50, 0.75, 1.20);
    float sunScale = clamp(
        sunIntensity/1.27, 0.55, 1.10);
    float ambientLevel = mix(0.48, 0.58, hemisphere)*ambientScale;
    // Tree palettes already encode their shadow/midtone/light value ramp.
    // Preserve those authored values while applying the same environment
    // temperature as every other world material.
    float authoredAmbient = mix(0.86, 0.94, hemisphere);
    ambientLevel = mix(
        ambientLevel, authoredAmbient, authoredRampAmount);
    float directLevel = directAmount*sunScale*
        mix(0.44, 0.14, authoredRampAmount);

    vec3 result = materialColor*(
        coolFillTint*ambientLevel + warmSunTint*directLevel);
    // Sunset stays colorful and well filled. The substantial value reduction
    // begins only once the environment tint turns decisively blue at night.
    float nightAmount = smoothstep(
        0.08, 0.24, dayNightTint.b - dayNightTint.r);
    result *= mix(1.0, 0.68, nightAmount);
    result *= mix(
        vec3(1.0), normalizedLightingTint(dayNightTint),
        nightAmount*0.28);
    return result;
}

vec2 encodeOctahedralNormal(vec3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    if (normal.z < 0.0)
    {
        normal.xy =
            (1.0 - abs(normal.yx))*sign(normal.xy);
    }
    return normal.xy*0.5 + 0.5;
}

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    float mountainSurface = clamp(
        fragVertexColor.r - fragVertexColor.b, 0.0, 1.0)*
        clamp(terrainAmount, 0.0, 1.0);
    float terrainSlope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float terraceWall = clamp(terrainAmount, 0.0, 1.0)*
        (1.0 - mountainSurface)*
        smoothstep(0.06, 0.20, terrainSlope);
    vec3 lightDirection = normalize(-sunDirection);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float shadow = sampleShadow(normal);
    float lightFacing = dot(normal, lightDirection);
    float stylizedDiffuse = smoothstep(-0.05, 0.76, lightFacing);
    if (toonShadingEnabled > 0.5)
    {
        stylizedDiffuse = toonRamp(
            stylizedDiffuse, toonLightSteps);
    }
    float shadowVisibility = smoothstep(0.54, 0.96, shadow);
    float cloudLight = 1.0;
    if (terrainAmount > 0.5 && cloudShadowStrength > 0.001)
    {
        float cloudShadow = fragTerrainNoiseB.w;
        cloudLight -= cloudShadow*
            clamp(cloudShadowStrength, 0.0, 0.50);
    }
    float directAmount = stylizedDiffuse*shadowVisibility*cloudLight;
    float hemisphere = normal.y*0.5 + 0.5;
    directAmount = mix(
        directAmount, 0.52, terraceWall);
    float mountainDirect = 0.25 + directAmount*0.67;
    directAmount = mix(
        directAmount, mountainDirect,
        mountainSurface*0.78);

    vec4 albedo = baseColor*colDiffuse*texture(texture0, fragTexCoord)*fragVertexColor;
    albedo.a = baseColor.a*colDiffuse.a*
        mix(fragVertexColor.a, 1.0, clamp(vertexAoAmount, 0.0, 1.0));
    vec3 terrainAlbedo = albedo.rgb;
    if (terrainAmount > 0.001)
    {
        if (mountainSurface > 0.001)
        {
            // Keep the approved painterly grass on playable terrain, while
            // restored backdrop vertices reuse their authored rock/snow masks.
            terrainAlbedo = terrainMaterial(
                fragWorldPosition, normal, fragVertexColor);
        }
        else
        {
            terrainAlbedo = painterlyGroundMaterial(
                fragWorldPosition, normal, fragVertexColor, fragTexCoord);
        }
    }
    albedo.rgb = mix(albedo.rgb, terrainAlbedo,
                     clamp(terrainAmount, 0.0, 1.0));
    float vertexAo =
        mix(1.0, fragVertexColor.a, clamp(vertexAoAmount, 0.0, 1.0));
    float aoSource = clamp(bakedAo*vertexAo, 0.0, 1.0);
    float ao = mix(1.0, aoSource, clamp(aoStrength, 0.0, 1.0));
    vec3 litColor = applyStylizedWorldLight(
        albedo.rgb, directAmount, hemisphere, 0.0);
    litColor *= mix(0.78, 1.0, ao);
    if (terrainAmount > 0.5)
    {
        // Terrain keeps broad, soft illumination. Cast shadows remain visible
        // without crushing the painterly albedo or exposing mesh facets.
        float terrainDiffuse =
            clamp(dot(normal, lightDirection)*0.5 + 0.5, 0.0, 1.0);
        float terrainDirect = mix(
            stylizedDiffuse,
            smoothstep(0.18, 0.85, terrainDiffuse), 0.55)*
            shadowVisibility*cloudLight;
        terrainDirect = mix(
            terrainDirect, 0.52, terraceWall);
        terrainDirect = mix(
            terrainDirect, 0.25 + terrainDirect*0.67,
            mountainSurface*0.78);
        litColor = applyStylizedWorldLight(
            albedo.rgb, terrainDirect,
            mix(0.58, hemisphere, 0.42), 0.0);
        litColor *= mix(0.82, 1.0, ao);
    }
    if (treeAmount > 0.5)
    {
        float maximumShadowDarkening =
            max(clamp(shadowStrength, 0.0, 1.0)*0.52, 0.001);
        float castShadowAmount = clamp(
            (1.0 - shadow)/maximumShadowDarkening, 0.0, 1.0);
        float treeNdotL = max(dot(normal, lightDirection), 0.0);
        float treeRamp = clamp(treeNdotL*0.70 + 0.30, 0.0, 1.0);
        treeRamp *= mix(1.0, 0.48, castShadowAmount);

        vec3 dpdx = dFdx(fragWorldPosition);
        vec3 dpdy = dFdy(fragWorldPosition);
        vec3 faceNormal = normalize(cross(dpdx, dpdy));
        if (dot(faceNormal, normal) < 0.0)
        {
            faceNormal = -faceNormal;
        }
        vec3 crownNormal = normalize(mix(
            normal, faceNormal,
            clamp(treeLeafFlatness, 0.0, 1.0)));
        float crownSunFacing = dot(crownNormal, lightDirection);
        float faceSunFacing = dot(faceNormal, lightDirection);
        float crownDirectRamp = crownSunFacing*0.5 + 0.5;
        // Use constant triangle normals for shadow-side fill. Sunlit crowns
        // retain their soft authored response; unlit crowns reveal low-poly
        // planes instead of a smooth gradient or one flat green mass.
        float crownShadowSide =
            1.0 - smoothstep(-0.12, 0.30, faceSunFacing);
        float crownFacetedFillRamp = clamp(
            faceSunFacing*0.38 + 0.58, 0.0, 1.0);
        float crownRamp = clamp(
            mix(crownDirectRamp, crownFacetedFillRamp,
                crownShadowSide*0.88) -
            castShadowAmount*0.12,
            0.0, 1.0);
        float rampSmoothness = clamp(treeRampSmoothness, 0.02, 0.28);
        float shadowThreshold = clamp(
            treeShadowThreshold, 0.05, 0.75);
        float highlightThreshold = clamp(
            treeHighlightThreshold,
            shadowThreshold + rampSmoothness*2.0, 0.98);
        float leafShadowToMid = smoothstep(
            shadowThreshold - rampSmoothness,
            shadowThreshold + rampSmoothness,
            crownRamp);
        float leafMidToLight = smoothstep(
            highlightThreshold - rampSmoothness,
            highlightThreshold + rampSmoothness,
            crownRamp);
        vec3 restrainedLeafLight = mix(
            treeLeafMidColor, treeLeafLightColor,
            clamp(treeHighlightStrength, 0.0, 1.0));
        vec3 filledLeafShadow = mix(
            treeLeafShadowColor, treeLeafMidColor, 0.30);
        vec3 leafColor = mix(
            filledLeafShadow, treeLeafMidColor, leafShadowToMid);
        leafColor = mix(
            leafColor, restrainedLeafLight, leafMidToLight);

        float trunkShadowToMid = smoothstep(0.14, 0.62, treeRamp);
        float trunkMidToLight = smoothstep(0.44, 0.96, treeRamp);
        vec3 filledTrunkShadow = mix(
            treeTrunkShadowColor, treeTrunkMidColor, 0.22);
        vec3 trunkColor = mix(
            filledTrunkShadow, treeTrunkMidColor, trunkShadowToMid);
        trunkColor = mix(
            trunkColor, treeTrunkLightColor, trunkMidToLight);

        float greenDominance =
            albedo.g - max(albedo.r, albedo.b);
        float leafMask = smoothstep(-0.015, 0.075, greenDominance);
        leafColor = visualBiomeColor(leafColor, worldBiomeLeaves, vec3(0.400, 0.553, 0.243));
        vec3 treeColor = mix(trunkColor, leafColor, leafMask);
        float sourceLuminance = dot(
            albedo.rgb, vec3(0.2126, 0.7152, 0.0722));
        float textureDetail = mix(
            0.94, 1.06,
            smoothstep(0.04, 0.72, sourceLuminance));
        treeColor *= mix(
            1.0, textureDetail,
            clamp(treeTextureVariation, 0.0, 1.0));
        float treeDirectAmount =
            smoothstep(0.22, 0.88, treeRamp)*shadowVisibility;
        litColor = applyStylizedWorldLight(
            treeColor, treeDirectAmount,
            hemisphere, 1.0);
        litColor *= mix(0.86, 1.0, ao);
    }
    if (bushAmount > 0.5)
    {
        // Rounded decorative bushes keep their interpolated mesh normals.
        // A restrained three-tone ramp integrates them with the tree family
        // without borrowing the tree crown's geometric-normal treatment.
        float bushRamp = clamp(
            dot(normal, lightDirection)*0.5 + 0.5,
            0.0, 1.0);
        bushRamp *= mix(0.38, 1.0, shadowVisibility);
        float rampSmoothness = clamp(
            bushRampSmoothness, 0.02, 0.22);
        float shadowThreshold = clamp(
            bushShadowThreshold, 0.08, 0.60);
        float highlightThreshold = clamp(
            bushHighlightThreshold,
            shadowThreshold + rampSmoothness*2.0, 0.96);
        float shadowToMid = smoothstep(
            shadowThreshold - rampSmoothness,
            shadowThreshold + rampSmoothness,
            bushRamp);
        float midToLight = smoothstep(
            highlightThreshold - rampSmoothness,
            highlightThreshold + rampSmoothness,
            bushRamp);
        vec3 restrainedLight = mix(
            bushMidColor, bushLightColor,
            clamp(bushHighlightStrength, 0.0, 1.0));
        vec3 filledBushShadow = mix(
            bushShadowColor, bushMidColor, 0.30);
        vec3 bushColor = mix(
            filledBushShadow, bushMidColor, shadowToMid);
        bushColor = mix(
            bushColor, restrainedLight, midToLight);

        // Preserve only quiet value variation from the authored texture; its
        // saturated hue no longer controls the bush palette. Non-green model
        // details continue through the generic material path unchanged.
        float sourceLuminance = dot(
            albedo.rgb, vec3(0.2126, 0.7152, 0.0722));
        float textureDetail = mix(
            0.96, 1.04,
            smoothstep(0.05, 0.75, sourceLuminance));
        bushColor *= mix(1.0, textureDetail, 0.45);
        float greenDominance =
            albedo.g - max(albedo.r, albedo.b);
        float leafMask = smoothstep(-0.015, 0.075, greenDominance);
        float bushDirectAmount =
            smoothstep(0.30, 0.88, bushRamp)*shadowVisibility;
        bushColor = visualBiomeColor(bushColor, worldBiomeLeaves, vec3(0.400, 0.553, 0.243));
        vec3 integratedBush = applyStylizedWorldLight(
            bushColor, bushDirectAmount, hemisphere, 1.0);
        integratedBush *= mix(0.86, 1.0, ao);
        litColor = mix(litColor, integratedBush, leafMask);
    }
    if (coniferAmount > 0.5)
    {
        // Conifers keep their layered silhouette but reveal its planes a
        // little more clearly than round crowns. The blend stays well below
        // fully geometric shading, so facets remain soft rather than crystal-like.
        vec3 dpdx = dFdx(fragWorldPosition);
        vec3 dpdy = dFdy(fragWorldPosition);
        vec3 faceNormal = normalize(cross(dpdx, dpdy));
        if (dot(faceNormal, normal) < 0.0)
        {
            faceNormal = -faceNormal;
        }
        vec3 coniferNormal = normalize(mix(
            normal, faceNormal,
            clamp(coniferFacetBlend, 0.0, 0.75)));
        float coniferRamp = clamp(
            dot(coniferNormal, lightDirection)*0.5 + 0.5,
            0.0, 1.0);
        coniferRamp *= mix(0.40, 1.0, shadowVisibility);
        float rampSmoothness = clamp(
            coniferRampSmoothness, 0.02, 0.22);
        float shadowThreshold = clamp(
            coniferShadowThreshold, 0.08, 0.60);
        float highlightThreshold = clamp(
            coniferHighlightThreshold,
            shadowThreshold + rampSmoothness*2.0, 0.96);
        float shadowToMid = smoothstep(
            shadowThreshold - rampSmoothness,
            shadowThreshold + rampSmoothness,
            coniferRamp);
        float midToLight = smoothstep(
            highlightThreshold - rampSmoothness,
            highlightThreshold + rampSmoothness,
            coniferRamp);
        vec3 restrainedLight = mix(
            coniferMidColor, coniferLightColor,
            clamp(coniferHighlightStrength, 0.0, 1.0));
        vec3 filledConiferShadow = mix(
            coniferShadowColor, coniferMidColor, 0.28);
        vec3 coniferColor = mix(
            filledConiferShadow, coniferMidColor, shadowToMid);
        coniferColor = mix(
            coniferColor, restrainedLight, midToLight);

        float sourceLuminance = dot(
            albedo.rgb, vec3(0.2126, 0.7152, 0.0722));
        float textureDetail = mix(
            0.96, 1.04,
            smoothstep(0.05, 0.75, sourceLuminance));
        coniferColor *= mix(1.0, textureDetail, 0.35);
        float greenDominance =
            albedo.g - max(albedo.r, albedo.b);
        float foliageMask = smoothstep(-0.015, 0.075, greenDominance);
        float coniferDirectAmount =
            smoothstep(0.30, 0.90, coniferRamp)*shadowVisibility;
        coniferColor = visualBiomeColor(coniferColor, worldBiomeLeaves, vec3(0.400, 0.553, 0.243));
        vec3 integratedConifer = applyStylizedWorldLight(
            coniferColor, coniferDirectAmount,
            coniferNormal.y*0.5 + 0.5, 1.0);
        integratedConifer *= mix(0.86, 1.0, ao);
        litColor = mix(litColor, integratedConifer, foliageMask);
    }

    // Add restrained bounce light only to object faces turned away from the
    // sun. Terrain and already-lit faces retain their existing values.
    float objectShadowSide =
        (1.0 - smoothstep(-0.12, 0.48, lightFacing))*
        (1.0 - clamp(terrainAmount, 0.0, 1.0));
    litColor *= 1.0 + objectShadowSide*0.24;

    float horizontalDistance =
        length(cameraPosition.xz - fragWorldPosition.xz);
    float fogRange = max(fogEnd - fogStart, 0.01);
    float fogDistance =
        clamp((horizontalDistance - fogStart)/fogRange, 0.0, 1.0);
    // Hold atmospheric replacement outside normal interaction distance, then
    // let it rise smoothly through the distant world. At roughly half the
    // configured range the curve is still intentionally restrained.
    float hazeCurve = pow(
        smoothstep(0.18, 1.0, fogDistance), 1.65);

    // The profile fog color remains the time-of-day anchor. Sky ambient adds
    // altitude color, while sunset contributes only a luminance-preserving
    // trace of warmth near the distant horizon.
    vec3 atmosphereColor = mix(
        fogColor, skyAmbientColor, 0.14 + hazeCurve*0.08);
    float sunsetAmount = smoothstep(
        0.10, 0.24, dayNightTint.r - dayNightTint.b);
    float viewRelativeHeight =
        fragWorldPosition.y - cameraPosition.y;
    float peakClarity = smoothstep(
        4.0, 24.0, max(viewRelativeHeight, 0.0));
    float horizonWarmth = sunsetAmount*0.055*
        (1.0 - peakClarity);
    float atmosphereLuminance = dot(
        atmosphereColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 warmAtmosphere = normalizedLightingTint(sunColor)*
        atmosphereLuminance;
    atmosphereColor = mix(
        atmosphereColor, warmAtmosphere, horizonWarmth);

    // Scenic materials use distance as an art-direction axis. The foreground
    // stays unchanged, the middle ground rests, and distant vegetation keeps
    // most of its authored hue instead of becoming a cyan silhouette.
    float depthPalette = clamp(distantFadeAmount, 0.0, 1.0);
    float nearPlane =
        1.0 - smoothstep(17.0, 34.0, horizontalDistance);
    float middlePlane =
        smoothstep(0.10, 0.38, fogDistance)*
        (1.0 - smoothstep(0.55, 0.82, fogDistance));
    float farPlane = smoothstep(0.22, 0.78, fogDistance);
    vec3 nearColor = litColor*vec3(1.035, 1.012, 0.965);
    float nearLuminance =
        dot(nearColor, vec3(0.2126, 0.7152, 0.0722));
    nearColor = mix(vec3(nearLuminance), nearColor, 1.055);
    litColor = mix(
        litColor, nearColor, depthPalette*nearPlane*0.72);
    float middleLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 middleColor = mix(
        vec3(middleLuminance)*vec3(0.975, 1.0, 0.985),
        litColor, 0.84);
    litColor = mix(
        litColor, middleColor, depthPalette*middlePlane*0.34);
    float distantLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 distantIdentity = mix(
        vec3(distantLuminance), litColor, 0.86);
    distantIdentity *= mix(
        vec3(1.0), normalizedLightingTint(atmosphereColor), 0.14);
    vec3 distantColor = mix(
        distantIdentity, atmosphereColor, 0.08);
    litColor = mix(
        litColor, distantColor, depthPalette*farPlane*0.34);

    // Backdrop mountain vertices carry a dedicated material mask. Two smooth
    // distance shoulders create near/middle/far mountain groups without new
    // geometry, texture, or noise. Peaks retain more local color below.
    float nearMountain = mountainSurface*
        (1.0 - smoothstep(0.36, 0.60, fogDistance));
    float middleMountain = mountainSurface*
        smoothstep(0.36, 0.60, fogDistance)*
        (1.0 - smoothstep(0.68, 0.88, fogDistance));
    float farMountain = mountainSurface*
        smoothstep(0.68, 0.92, fogDistance);
    float mountainLuminance = dot(
        litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 nearMountainColor = mix(
        vec3(mountainLuminance), litColor, 0.78)*1.04;
    litColor = mix(
        litColor, nearMountainColor, nearMountain*0.42);
    litColor = mix(
        litColor, atmosphereColor, middleMountain*0.08);
    litColor = mix(
        litColor, atmosphereColor, farMountain*0.18);
    litColor = mix(litColor, selectionTint, clamp(selectionAmount, 0.0, 1.0));
    float hitFlashMask = 1.0;
    if (hitFlashRadius > 0.001) {
        float hitDistance = distance(
            fragWorldPosition, hitFlashPosition);
        hitFlashMask = 1.0 - smoothstep(
            hitFlashRadius*0.28, hitFlashRadius, hitDistance);
    }
    float localHitFlash = clamp(
        hitFlashAmount*hitFlashMask, 0.0, 1.0);
    litColor = mix(
        litColor, vec3(1.0, 0.95, 0.78), localHitFlash);

    // Cheap height shaping lets distant bases settle into haze while peaks
    // remain clearer. Both terms are smooth, avoiding a horizontal fog band.
    float lowerWorld =
        1.0 - smoothstep(1.0, 12.0, max(viewRelativeHeight, 0.0));
    float distanceFog =
        (1.0 - exp(-2.10*hazeCurve*hazeCurve))*0.62;
    float heightFogScale =
        1.0 + lowerWorld*0.06 - peakClarity*0.14;
    float fogAmount = clamp(
        distanceFog*heightFogScale, 0.0, 0.62);
    // Keep nearby tree silhouettes distinct from the mountain backdrop.
    float forestClarity = clamp(treeAmount, 0.0, 1.0)*
        (1.0 - smoothstep(0.28, 0.68, fogDistance));
    fogAmount *= 1.0 - forestClarity*0.18;
    if (fogBandsEnabled > 0.5)
    {
        float bands = max(round(fogBandCount), 2.0);
        fogAmount = floor(fogAmount*(bands - 1.0) + 0.5)/
            (bands - 1.0);
    }
    float preFogLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 softenedIdentity = mix(
        vec3(preFogLuminance), litColor, 0.88);
    softenedIdentity *= mix(
        vec3(1.0), normalizedLightingTint(atmosphereColor), 0.18);
    float softeningAmount =
        smoothstep(0.10, 0.82, fogDistance)*0.28;
    litColor = mix(
        litColor, softenedIdentity, softeningAmount);
    preFogLuminance = dot(
        litColor, vec3(0.2126, 0.7152, 0.0722));
    float liftedLuminance = mix(
        preFogLuminance,
        max(preFogLuminance, atmosphereLuminance*0.98),
        hazeCurve*(0.10 + mountainSurface*0.08));
    litColor = mix(
        litColor, vec3(liftedLuminance),
        hazeCurve*0.06);
    // Mountain haze is slightly cooler and lighter than forest haze;
    // scaling the existing atmosphere preserves dusk and night brightness.
    vec3 mountainAtmosphere = atmosphereColor*vec3(1.025, 1.035, 1.105);
    vec3 surfaceAtmosphere = mix(
        atmosphereColor, mountainAtmosphere, mountainSurface);
    litColor = mix(litColor, surfaceAtmosphere, fogAmount);
    litColor *= exposure;
    float luminance = dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(luminance), litColor, saturation);
    litColor = mix(
        litColor, vec3(1.0),
        clamp(enemyWhiteFlashAmount, 0.0, 1.0));

    float ghost = clamp(ghostAmount, 0.0, 1.0);
    // Scene opacity and outline eligibility are independent. The outline
    // class lives in normalAo below; clearing scene alpha here made every
    // no-ink prop (reeds, flowers and small rocks) visually transparent.
    float outputAlpha = albedo.a;
    if (ghost > 0.001)
    {
        // A restrained world-space hologram: the body stays translucent while
        // the view-dependent rim carries most of the silhouette definition.
        float fresnel = pow(
            1.0 - clamp(abs(dot(normal, viewDirection)), 0.0, 1.0),
            2.15);
        float broadWave = 0.5 + 0.5*sin(
            fragWorldPosition.y*7.0 - timeSeconds*2.8 +
            (fragWorldPosition.x + fragWorldPosition.z)*0.75);
        float scanWave = 0.5 + 0.5*sin(
            fragWorldPosition.y*20.0 - timeSeconds*5.4);
        float scanLine = pow(scanWave, 14.0);
        float shimmer = 0.5 + 0.5*sin(
            (fragWorldPosition.x - fragWorldPosition.z)*3.2 +
            fragWorldPosition.y*4.5 + timeSeconds*2.1);

        vec3 ghostBody = ghostTint*(0.46 + broadWave*0.09);
        vec3 ghostHighlight = mix(ghostTint, vec3(0.86, 1.0, 0.92), 0.46);
        vec3 hologramColor = ghostBody + ghostHighlight*
            (fresnel*0.76 + scanLine*0.15 + shimmer*0.03);
        litColor = mix(litColor, hologramColor, ghost);
        outputAlpha = mix(
            outputAlpha,
            clamp(ghostOpacity*(0.74 + fresnel*0.24 + scanLine*0.08),
                  0.0, 0.58),
            ghost);
    }

    // Glowing projectiles still write the normal/ink target of the world pass.
    // This prevents old geometry tags from bleeding through their silhouette.
    litColor = mix(litColor, albedo.rgb, clamp(unlitAmount, 0.0, 1.0));
    finalColor = vec4(litColor, outputAlpha);
    // Pack the outline material class and the SSAO mask together.  Keeping
    // the class separate from scene color alpha is important: alpha is also
    // used by transparent ghosts and blending, while the ink pass needs a
    // stable distinction between terrain, props and vegetation.
    //
    //   0 = no ink geometry, 1 = terrain, 2 = solid prop, 3 = vegetation
    //   fractional half of the bucket = screen-space AO amount
    float outlineMaterialClass = mix(
        2.0, 1.0, step(0.5, terrainAmount));
    outlineMaterialClass = mix(
        outlineMaterialClass, 3.0,
        step(0.5, max(vegetationAmount, treeAmount)));
    outlineMaterialClass = mix(
        outlineMaterialClass,
        outlineMaterialClassOverride,
        step(0.5, outlineMaterialClassOverride));
    outlineMaterialClass *= step(0.5, inkOutlineEligible);
    float packedClassAo = outlineMaterialClass*0.25 +
        clamp(screenAoAmount, 0.0, 1.0)*(1.0 - ghost)*
        (1.0 - terraceWall)*0.125;
    // Class zero leaves the upper quarter of the packed blue channel free.
    // Store the foreground-occluder tag there while keeping alpha opaque, so
    // this MRT attachment fully replaces geometry rendered behind the plant.
    packedClassAo = mix(
        packedClassAo, 0.1875, step(0.5, inkOccluder));
    normalAo = vec4(
        encodeOctahedralNormal(normal), packedClassAo, 1.0);
}
