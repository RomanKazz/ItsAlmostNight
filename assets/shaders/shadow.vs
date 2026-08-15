#version 330

in vec3 vertexPosition;
// raylib supplies this attribute through glVertexAttribPointer, not the
// integer-pointer API required by an ivec4 input.
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 boneMatrices[48];
uniform int skinningEnabled;
uniform int instancingEnabled;

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
    gl_Position = instancingEnabled != 0
        ? mvp*instanceTransform*localPosition
        : mvp*localPosition;
}
