#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform float timeSeconds;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorldPosition;
out float fragLocalHeight;

void main()
{
    vec4 worldPosition =
        instanceTransform*vec4(vertexPosition, 1.0);
    float windHeight =
        smoothstep(0.02, 0.85, vertexPosition.y);
    float phase =
        timeSeconds*2.1 +
        worldPosition.x*0.47 +
        worldPosition.z*0.39;
    float wave =
        sin(phase) + sin(phase*1.91 + timeSeconds)*0.28;
    worldPosition.xz +=
        vec2(0.075, 0.035)*wave*windHeight;

    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(mat3(instanceTransform)*vertexNormal);
    fragWorldPosition = worldPosition.xyz;
    fragLocalHeight = vertexPosition.y;
    gl_Position = mvp*worldPosition;
}
