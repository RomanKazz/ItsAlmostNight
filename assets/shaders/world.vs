#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in ivec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightViewProjection;
uniform float timeSeconds;
uniform float windAmount;
uniform float localWindHeight;
uniform mat4 boneMatrices[32];
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
    if (skinningEnabled != 0) {
        skinMatrix =
            boneMatrices[vertexBoneIndices.x]*vertexBoneWeights.x +
            boneMatrices[vertexBoneIndices.y]*vertexBoneWeights.y +
            boneMatrices[vertexBoneIndices.z]*vertexBoneWeights.z +
            boneMatrices[vertexBoneIndices.w]*vertexBoneWeights.w;
    }
    vec4 localPosition =
        skinMatrix*vec4(vertexPosition, 1.0);
    mat4 modelMatrix =
        instancingEnabled != 0 ? instanceTransform : matModel;
    vec4 worldPosition = modelMatrix*localPosition;
    float windHeight = mix(
        smoothstep(0.25, 3.4, worldPosition.y),
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
    vec3 localNormal =
        skinningEnabled != 0
            ? mat3(skinMatrix)*vertexNormal
            : vertexNormal;
    fragWorldNormal =
        instancingEnabled != 0
            ? normalize(mat3(modelMatrix)*localNormal)
            : normalize((matNormal*vec4(localNormal, 0.0)).xyz);
    fragVertexColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    fragLightSpacePosition = lightViewProjection*worldPosition;

    gl_Position =
        instancingEnabled != 0
            ? mvp*worldPosition
            : mvp*localPosition;
}
