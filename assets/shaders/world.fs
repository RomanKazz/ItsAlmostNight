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
uniform float screenAoAmount;
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
uniform float ghostAmount;
uniform vec3 ghostTint;
uniform float ghostOpacity;
uniform float inkOutlineEligible;
uniform sampler2D shadowMap;
uniform float shadowsEnabled;
uniform float constantBias;
uniform float slopeBias;
uniform float shadowStrength;
uniform float shadowMapTexelSize;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

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
        smoothstep(0.045, 0.16, slope);
    float dirtPatch =
        smoothstep(0.68, 0.86, broadNoise*0.72 + patchNoise*0.28)*
        (1.0 - smoothstep(0.10, 0.30, slope));
    float dirtWeight = clamp(
        max(dirtSlopeBand*0.96, dirtPatch*0.66), 0.0, 1.0);

    vec3 textureSample =
        texture(terrainTexture, worldXZ*0.08).rgb;
    float textureLuminance = dot(
        textureSample, vec3(0.2126, 0.7152, 0.0722));
    // Keep the watercolor lively, but below the saturation of foliage and
    // gameplay accents so the ground still supports the scene.
    vec3 gradedGrassTexture = mix(
        vec3(textureLuminance), textureSample, 0.74)*
        vec3(1.04, 0.93, 0.88);
    float grassDetail = mix(
        0.82 + detailNoise*0.24,
        1.0,
        terrainTextureEnabled);
    vec3 grass = mix(
        terrainGrassTint*grassDetail,
        gradedGrassTexture,
        terrainTextureEnabled);

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

    // Terrain vertex R-B encodes the mountain band on the raised boundary
    // and backdrop. Shore vertices remain greyscale, so water-darkening
    // cannot accidentally become rock.
    float mountainAmount = clamp(
        vertexColor.r - vertexColor.b, 0.0, 1.0);
    float rockSlope = smoothstep(0.12, 0.42, slope);
    float rockWeight = clamp(
        mountainAmount*(0.60 + rockSlope*0.40), 0.0, 1.0);
    float rockNoise = valueNoise(
        worldXZ*0.115 + vec2(11.7, -24.6));
    float rockMass = valueNoise(
        worldXZ*0.028 + vec2(-7.4, 16.9));
    float weathering = valueNoise(vec2(
        worldPosition.x*0.038 + worldPosition.y*0.052,
        worldPosition.z*0.038 - worldPosition.y*0.031));
    vec3 rock = mix(
        vec3(0.19, 0.24, 0.25),
        vec3(0.46, 0.42, 0.34),
        0.22 + rockMass*0.58);
    rock *= 0.88 + rockNoise*0.13;
    rock = mix(rock, rock*vec3(0.72, 0.78, 0.80),
               smoothstep(0.58, 0.86, weathering)*0.34);
    vec3 treeLineRock = vec3(0.20, 0.29, 0.22);
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
    // A compact disc-shaped PCF kernel avoids the square, technical-looking
    // edge produced by a regular grid. It deliberately keeps the old nine
    // texture reads: softer silhouettes without additional fill-rate cost.
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
    for (int sampleIndex = 0; sampleIndex < 9; ++sampleIndex)
    {
        float centerWeight = sampleIndex == 0 ? 1.55 : 1.0;
        vec2 offset = disc[sampleIndex]*shadowMapTexelSize*3.15;
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
    // Preserve readable ambient colour in full shadow. With the normal
    // 0.58 setting this caps direct-light darkening below fifty percent.
    float artisticStrength = clamp(shadowStrength, 0.0, 1.0)*0.82;
    return 1.0 - occlusion*artisticStrength*edgeFade;
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
    float lightFacing = dot(normal, lightDirection);
    float lambert = max(lightFacing, 0.0);
    float diffuseRamp = smoothstep(-0.02, 0.74, lightFacing);
    float stylizedDiffuse = mix(lambert, diffuseRamp, 0.22);
    if (toonShadingEnabled > 0.5)
    {
        stylizedDiffuse = toonRamp(
            stylizedDiffuse, toonLightSteps);
    }
    float mountainDiffuse = 0.16 + stylizedDiffuse*0.58;
    stylizedDiffuse = mix(
        stylizedDiffuse, mountainDiffuse,
        mountainSurface*0.78);
    // Terrace walls are built from long triangle strips. Compress their
    // normal-dependent response so interpolation cannot reveal each cell.
    float wallDiffuse = 0.55;
    stylizedDiffuse = mix(
        stylizedDiffuse, wallDiffuse, terraceWall);

    float hemisphere = normal.y*0.5 + 0.5;
    if (toonShadingEnabled > 0.5)
    {
        hemisphere = toonRamp(
            hemisphere, max(toonLightSteps - 1.0, 2.0));
    }
    hemisphere = mix(hemisphere, 0.62, terraceWall);
    vec3 ambientColor = mix(groundAmbientColor, skyAmbientColor, hemisphere);
    float ambientShape = mix(0.84, 1.08, hemisphere);
    vec3 ambient = ambientColor*ambientIntensity*ambientShape;
    // Open-sky bounce keeps back-facing and shadowed surfaces readable.
    // Match the grass fill closely without flattening the direct sun contrast.
    ambient += skyAmbientColor*ambientIntensity*0.26;
    float backFacingFill = 1.0 - smoothstep(
        -0.24, 0.34, lightFacing);
    vec3 coolShadowFill = mix(
        groundAmbientColor, skyAmbientColor, 0.78);
    ambient += coolShadowFill*ambientIntensity*
        (0.12 + backFacingFill*0.24);
    vec3 wallAmbient = mix(
        groundAmbientColor, skyAmbientColor, 0.68)*
        ambientIntensity*1.12 +
        skyAmbientColor*ambientIntensity*0.24;
    ambient = mix(ambient, wallAmbient, terraceWall);
    float shadow = mix(
        sampleShadow(normal), 1.0, terraceWall);
    float cloudLight = 1.0;
    if (terrainAmount > 0.5 && cloudShadowStrength > 0.001)
    {
        float cloudShadow = cloudShadowPattern(fragWorldPosition.xz);
        cloudLight -= cloudShadow*
            clamp(cloudShadowStrength, 0.0, 0.50);
    }
    float sunHighlight = pow(max(lightFacing, 0.0), 2.0)*0.06*
        mix(1.0, 0.24, mountainSurface)*
        mix(1.0, 0.20, terraceWall);
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
        timeOfDayRimStrength*(1.0 - terraceWall);
    vec3 direct = sunColor*sunIntensity*
        (stylizedDiffuse + sunHighlight)*shadow;
    float rim =
        pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)*
        smoothstep(-0.2, 0.75, normal.y)*
        (1.0 - terraceWall);
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
    // Shadows lean toward cool forest green instead of neutral black. This
    // is intentionally subtle and luminance-preserving: the ambient term
    // remains responsible for keeping back-facing surfaces readable.
    float directionalShadowAmount = clamp(1.0 - shadow, 0.0, 1.0);
    vec3 directionalShadowTint = vec3(0.90, 0.965, 1.035);
    litColor *= mix(
        vec3(1.0), directionalShadowTint,
        directionalShadowAmount*0.46);
    // Sunlit ground leans gently warm; indirect and shadowed ground leans
    // cool. Luminance contrast changes more than saturation.
    float terrainSunAmount = smoothstep(
        0.04, 0.72, lightFacing)*shadow*cloudLight;
    vec3 terrainTemperature = mix(
        vec3(0.88, 0.96, 1.07),
        vec3(1.07, 1.015, 0.92),
        terrainSunAmount);
    litColor *= mix(
        vec3(1.0), terrainTemperature,
        clamp(terrainAmount, 0.0, 1.0)*0.48);
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
        vec3(distantLuminance)*vec3(0.86, 0.94, 0.92),
        fogColor, 0.28);
    litColor = mix(litColor, distantColor, distantFade*0.38);
    litColor = mix(litColor, selectionTint, clamp(selectionAmount, 0.0, 1.0));
    litColor = mix(litColor, vec3(1.0, 0.28, 0.12), clamp(hitFlashAmount, 0.0, 1.0));

    float horizontalDistance =
        length(cameraPosition.xz - fragWorldPosition.xz);
    float fogRange = max(fogEnd - fogStart, 0.01);
    float fogDistance =
        clamp((horizontalDistance - fogStart)/fogRange, 0.0, 1.0);
    float distanceFog =
        (1.0 - exp(-2.15*fogDistance*fogDistance))*0.74;
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
        fogAmount*0.14);
    litColor = mix(litColor, atmosphereColor, fogAmount);
    litColor *= exposure;
    float luminance = dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(luminance), litColor, saturation);

    float ghost = clamp(ghostAmount, 0.0, 1.0);
    float outputAlpha = albedo.a*step(0.5, inkOutlineEligible);
    if (ghost > 0.001)
    {
        // A stable world-space hologram: soft cyan body, animated vertical
        // scan bands, and a view-dependent rim that makes the silhouette
        // readable against both bright grass and dark terrain.
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

        vec3 ghostBody = ghostTint*(0.62 + broadWave*0.13);
        vec3 ghostHighlight = mix(ghostTint, vec3(0.78, 0.96, 1.0), 0.78);
        vec3 hologramColor = ghostBody + ghostHighlight*
            (fresnel*0.72 + scanLine*0.26 + shimmer*0.055);
        litColor = mix(litColor, hologramColor, ghost);
        outputAlpha = mix(
            outputAlpha,
            clamp(ghostOpacity*(0.68 + fresnel*0.34 + scanLine*0.16),
                  0.0, 0.82),
            ghost);
    }

    finalColor = vec4(litColor, outputAlpha);
    normalAo = vec4(
        encodeOctahedralNormal(normal),
        clamp(screenAoAmount, 0.0, 1.0)*(1.0 - ghost)*
        (1.0 - terraceWall), 1.0);
}
