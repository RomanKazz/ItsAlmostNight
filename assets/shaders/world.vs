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
uniform mat4 boneMatrices[48];
uniform int skinningEnabled;
uniform int instancingEnabled;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragVertexColor;
out vec2 fragTexCoord;
out vec4 fragLightSpacePosition;

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
    vec3 worldNormal = instancingEnabled != 0
        ? transpose(inverse(mat3(modelMatrix))) * localNormal
        : (matNormal*vec4(localNormal, 0.0)).xyz;
    fragWorldNormal = dot(worldNormal, worldNormal) < 0.000001
        ? vec3(0.0, 1.0, 0.0)
        : normalize(worldNormal);
    fragVertexColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    fragLightSpacePosition = lightViewProjection*worldPosition;

    gl_Position =
        instancingEnabled != 0
            ? mvp*worldPosition
            : mvp*localPosition;
}
