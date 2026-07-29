#version 330

in vec3 vertexPosition;
in ivec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform float timeSeconds;
uniform float windAmount;
uniform mat4 boneMatrices[32];
uniform int skinningEnabled;

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
    vec4 worldPosition = matModel*localPosition;
    float windHeight =
        smoothstep(0.25, 3.4, worldPosition.y)*windAmount;
    float windPhase =
        timeSeconds*1.35 +
        worldPosition.x*0.31 +
        worldPosition.z*0.23;
    float primaryWave = sin(windPhase);
    float detailWave =
        sin(windPhase*1.73 + timeSeconds*0.8)*0.35;
    localPosition.xz +=
        vec2(0.09, 0.04)*
        (primaryWave + detailWave)*windHeight;
    gl_Position = mvp*localPosition;
}
