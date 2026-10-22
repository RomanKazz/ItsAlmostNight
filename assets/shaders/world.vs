#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec3 vertexNormal;
in vec4 vertexColor;
// raylib uploads boneIndices with glVertexAttribPointer as unsigned bytes.
// Keep the input floating-point and convert explicitly; declaring an ivec4
// here requires glVertexAttribIPointer and is driver-dependent in OpenGL.
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightViewProjection;
uniform float timeSeconds;
uniform float windAmount;
uniform float localWindHeight;
uniform float terrainAmount;
uniform float terrainBoundaryMissingGrowth;
uniform mat4 boneMatrices[48];
uniform int skinningEnabled;
uniform int instancingEnabled;
// Two adjacent poses share the existing palette (up to 24 bones per pose).
uniform int bonePoseOffset;
uniform float bonePoseBlend;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragVertexColor;
out vec2 fragTexCoord;
out vec4 fragLightSpacePosition;
out vec4 fragTerrainNoiseA;
out vec4 fragTerrainNoiseB;

float terrainHash21(vec2 position)
{
    return fract(sin(dot(position, vec2(127.1, 311.7)))*43758.5453);
}

float terrainValueNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 local = fract(position);
    vec2 blend = local*local*(3.0 - 2.0*local);
    float bottomLeft = terrainHash21(cell);
    float bottomRight = terrainHash21(cell + vec2(1.0, 0.0));
    float topLeft = terrainHash21(cell + vec2(0.0, 1.0));
    float topRight = terrainHash21(cell + vec2(1.0, 1.0));
    return mix(mix(bottomLeft, bottomRight, blend.x),
               mix(topLeft, topRight, blend.x), blend.y);
}

uniform float vegetationAmount;
uniform float treeAmount;
uniform float bushAmount;
uniform float coniferAmount;
uniform vec3 plantBiomeAnchor;

uniform vec2 worldBiomeCenters[4];
uniform vec4 worldBiomeShape;
out vec4 fragBiomeWeights;

// Shared with visual_biomes::weights. Evaluate per vertex, not per screen pixel.
vec4 visualBiomeWeights(vec2 position)
{
    if (worldBiomeShape.x <= 0.0) return vec4(1.0, 0.0, 0.0, 0.0);
    vec2 q = position/worldBiomeShape.x;
    position += vec2(
        sin(q.y*11.0 + worldBiomeShape.z) + 0.45*sin((q.x + q.y)*19.0 + worldBiomeShape.w),
        sin(q.x*12.0 + worldBiomeShape.w) + 0.45*sin((q.y - q.x)*17.0 + worldBiomeShape.z))*worldBiomeShape.x*0.018;
    vec4 distances = vec4(
        length(position - worldBiomeCenters[0]), length(position - worldBiomeCenters[1]),
        length(position - worldBiomeCenters[2]), length(position - worldBiomeCenters[3]));
    float closest = min(min(distances.x, distances.y), min(distances.z, distances.w));
    vec4 weights = 1.0 - smoothstep(vec4(0.0), vec4(worldBiomeShape.y), distances - closest);
    return weights/dot(weights, vec4(1.0));
}

// Assign one palette to the entire plant, including crowns that cross a border.
vec4 plantBiomeWeights(vec2 origin)
{
    vec4 weights = visualBiomeWeights(origin);
    int biome = 0;
    for (int i = 1; i < 4; ++i)
        if (weights[i] > weights[biome]) biome = i;
    vec4 selected = vec4(0.0);
    selected[biome] = 1.0;
    return selected;
}

void main()
{
    mat4 skinMatrix = mat4(1.0);
    float skinWeight = dot(vertexBoneWeights, vec4(1.0));
    if (skinningEnabled != 0 && skinWeight > 0.0001) {
        ivec4 safeBoneIndices = ivec4(clamp(
            vertexBoneIndices + vec4(0.5),
            vec4(0.0), vec4(bonePoseOffset > 0 ? float(bonePoseOffset - 1) : 47.0)));
        skinMatrix =
            (boneMatrices[safeBoneIndices.x]*vertexBoneWeights.x +
             boneMatrices[safeBoneIndices.y]*vertexBoneWeights.y +
             boneMatrices[safeBoneIndices.z]*vertexBoneWeights.z +
             boneMatrices[safeBoneIndices.w]*vertexBoneWeights.w) /
            skinWeight;
        if (bonePoseOffset > 0) {
            ivec4 nextIndices = safeBoneIndices + ivec4(bonePoseOffset);
            mat4 nextSkinMatrix =
                (boneMatrices[nextIndices.x]*vertexBoneWeights.x +
                 boneMatrices[nextIndices.y]*vertexBoneWeights.y +
                 boneMatrices[nextIndices.z]*vertexBoneWeights.z +
                 boneMatrices[nextIndices.w]*vertexBoneWeights.w) / skinWeight;
            float blend = instancingEnabled != 0 ? instanceTransform[0][3] : bonePoseBlend;
            skinMatrix = skinMatrix*(1.0 - blend) + nextSkinMatrix*blend;
        }
    }
    vec4 localPosition =
        skinMatrix*vec4(vertexPosition, 1.0);
    if (terrainAmount > 0.001) {
        localPosition.y -= vertexTexCoord2.x * terrainBoundaryMissingGrowth;
    }
    mat4 modelMatrix =
        instancingEnabled != 0 ? instanceTransform : matModel;
    // The affine matrix's otherwise unused bottom-row component carries
    // each enemy's individual fraction without another vertex attribute.
    if (instancingEnabled != 0 && bonePoseOffset > 0) modelMatrix[0][3] = 0.0;
    vec4 worldPosition = modelMatrix*localPosition;
    if (windAmount > 0.0)
    {
        float windHeight = mix(
            // Wind bending starts at the model's own base.
            smoothstep(0.25, 3.4, localPosition.y),
            smoothstep(-0.015, 0.20, localPosition.y),
            clamp(localWindHeight, 0.0, 1.0))*windAmount;
        float windSpeed = mix(1.35, 2.1,
            clamp(localWindHeight, 0.0, 1.0));
        float windPhase =
            timeSeconds*windSpeed +
            worldPosition.x*0.31 +
            worldPosition.z*0.23;
        float primaryWave = sin(windPhase);
        float detailWave =
            sin(windPhase*1.73 + timeSeconds*0.8)*0.35;
        localPosition.xz +=
            vec2(0.09, 0.04)*
            (primaryWave + detailWave)*windHeight;
        worldPosition = modelMatrix*localPosition;
    }

    fragBiomeWeights = vec4(1.0, 0.0, 0.0, 0.0);
    if (terrainAmount > 0.001)
        fragBiomeWeights = visualBiomeWeights(worldPosition.xz);
    else if (vegetationAmount > 0.001 || treeAmount > 0.001 ||
             bushAmount > 0.001 || coniferAmount > 0.001)
        fragBiomeWeights = plantBiomeWeights(
            instancingEnabled == 0 && plantBiomeAnchor.z > 0.5
                ? plantBiomeAnchor.xy : modelMatrix[3].xz);
    fragWorldPosition = worldPosition.xyz;
    vec3 localNormal = vertexNormal;
    if (skinningEnabled != 0 && skinWeight > 0.0001) {
        localNormal = transpose(inverse(mat3(skinMatrix))) * vertexNormal;
    }
    if (dot(localNormal, localNormal) < 0.000001) {
        localNormal = vec3(0.0, 1.0, 0.0);
    } else {
        localNormal = normalize(localNormal);
    }
    vec3 worldNormal;
    if (instancingEnabled != 0)
    {
        // Instance transforms are rotation plus per-axis scale. Their exact
        // inverse-transpose can be reduced to three squared axis lengths,
        // avoiding a full matrix inverse for every vertex.
        mat3 instanceMatrix = mat3(modelMatrix);
        vec3 scaleSquared = max(vec3(
            dot(instanceMatrix[0], instanceMatrix[0]),
            dot(instanceMatrix[1], instanceMatrix[1]),
            dot(instanceMatrix[2], instanceMatrix[2])), vec3(0.0000001));
        worldNormal = instanceMatrix*(localNormal/scaleSquared);
    }
    else
    {
        worldNormal = (matNormal*vec4(localNormal, 0.0)).xyz;
    }
    if (terrainAmount > 0.001 && vertexTexCoord2.x > 0.001) {
        worldNormal = mix(worldNormal, vec3(0.0, 1.0, 0.0), terrainBoundaryMissingGrowth);
    }
    fragWorldNormal = dot(worldNormal, worldNormal) < 0.000001
        ? vec3(0.0, 1.0, 0.0)
        : normalize(worldNormal);
    fragVertexColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    fragLightSpacePosition = lightViewProjection*worldPosition;

    // These fields vary over metres, while the terrain grid is spaced at
    // roughly 0.75 m. Evaluating them per vertex and interpolating produces
    // the same smooth shapes without repeating dozens of noise hashes for
    // every covered screen pixel. Non-terrain draws skip the work entirely.
    fragTerrainNoiseA = vec4(0.0);
    fragTerrainNoiseB = vec4(0.0);
    if (terrainAmount > 0.001)
    {
        vec2 worldXZ = worldPosition.xz;
        vec2 cloudPosition =
            (worldXZ - vec2(2.20, 0.46)*timeSeconds)*0.025 +
            vec2(6.7, 7.0);
        float cloudBroad = terrainValueNoise(cloudPosition);
        float cloudShape = terrainValueNoise(
            cloudPosition*1.92 + vec2(17.4, -8.7));
        fragTerrainNoiseB.w = smoothstep(
            0.48, 0.72, cloudBroad*0.72 + cloudShape*0.28);

        float playableTerrain = 1.0 - step(
            0.001, abs(vertexColor.r - vertexColor.b));
        if (playableTerrain > 0.5)
        {
            mat2 macroRotation = mat2(0.6, -0.8, 0.8, 0.6);
            vec2 macroFields = vec2(
                terrainValueNoise(worldXZ*0.012 + vec2(31.7, -18.4)),
                terrainValueNoise(
                    macroRotation*worldXZ*0.0095 + vec2(-14.8, 27.1)));

            vec2 wearPosition = worldXZ*0.0205;
            vec2 wearWarp = vec2(
                terrainValueNoise(
                    wearPosition*0.61 + vec2(7.1, -19.4)),
                terrainValueNoise(
                    wearPosition*0.61 + vec2(-23.8, 11.6))) - 0.5;
            vec2 warpedWear = wearPosition + wearWarp*0.82;
            float wearBreakup = terrainValueNoise(
                warpedWear*2.18 + vec2(-8.6, 29.2));
            vec2 wearFields = vec2(
                terrainValueNoise(warpedWear + vec2(16.3, 5.7))*0.78 +
                    wearBreakup*0.22,
                wearBreakup);

            fragTerrainNoiseA = vec4(macroFields, wearFields);
            fragTerrainNoiseB.xyz = vec3(
                terrainValueNoise(worldXZ*0.19 + vec2(12.4, -35.7)),
                terrainValueNoise(vec2(
                    worldPosition.x*0.030 + worldPosition.y*0.17,
                    worldPosition.z*0.041 - worldPosition.y*0.11)),
                terrainValueNoise(
                    worldXZ*0.105 + vec2(-27.5, 14.2)));
        }
    }

    gl_Position =
        instancingEnabled != 0
            ? mvp*worldPosition
            : mvp*localPosition;
}
