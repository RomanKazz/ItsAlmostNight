#version 330

in vec3 vertexPosition;
// raylib supplies this attribute through glVertexAttribPointer, not the
// integer-pointer API required by an ivec4 input.
in vec4 vertexBoneIndices;
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
    float skinWeight = dot(vertexBoneWeights, vec4(1.0));
    if (skinningEnabled != 0 && skinWeight > 0.0001) {
        ivec4 safeBoneIndices = ivec4(clamp(
            vertexBoneIndices + vec4(0.5),
            vec4(0.0), vec4(31.0)));
        skinMatrix =
            (boneMatrices[safeBoneIndices.x]*vertexBoneWeights.x +
             boneMatrices[safeBoneIndices.y]*vertexBoneWeights.y +
             boneMatrices[safeBoneIndices.z]*vertexBoneWeights.z +
             boneMatrices[safeBoneIndices.w]*vertexBoneWeights.w) /
            skinWeight;
    }
    vec4 localPosition =
        skinMatrix*vec4(vertexPosition, 1.0);
    vec4 worldPosition = matModel*localPosition;
    // Match world.vs: bend from model-local height. World Y makes roots on
    // elevated terrain sway and shifts selection mask away from tree mesh.
    float windHeight =
        smoothstep(0.25, 3.4, localPosition.y)*windAmount;
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
