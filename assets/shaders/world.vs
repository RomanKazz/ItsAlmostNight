#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
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
uniform mat4 boneMatrices[48];
uniform int skinningEnabled;
uniform int instancingEnabled;

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

void main()
{
    mat4 skinMatrix = mat4(1.0);
    float skinWeight = dot(vertexBoneWeights, vec4(1.0));
    if (skinningEnabled != 0 && skinWeight > 0.0001) {
        ivec4 safeBoneIndices = ivec4(clamp(
            vertexBoneIndices + vec4(0.5),
            vec4(0.0), vec4(47.0)));
        skinMatrix =
            (boneMatrices[safeBoneIndices.x]*vertexBoneWeights.x +
             boneMatrices[safeBoneIndices.y]*vertexBoneWeights.y +
             boneMatrices[safeBoneIndices.z]*vertexBoneWeights.z +
             boneMatrices[safeBoneIndices.w]*vertexBoneWeights.w) /
            skinWeight;
    }
    vec4 localPosition =
        skinMatrix*vec4(vertexPosition, 1.0);
    mat4 modelMatrix =
        instancingEnabled != 0 ? instanceTransform : matModel;
    vec4 worldPosition = modelMatrix*localPosition;
    float windHeight = mix(
        // Wind bending must start at the model's own base. Using world Y
        // makes an object placed uphill sway at full strength at its roots.
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
