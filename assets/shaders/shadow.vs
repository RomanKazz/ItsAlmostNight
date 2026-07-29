#version 330

in vec3 vertexPosition;
in ivec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
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
    gl_Position =
        mvp*skinMatrix*vec4(vertexPosition, 1.0);
}
