#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out float fragLocalHeight;

void main()
{
    vec4 worldPosition = instanceTransform*vec4(vertexPosition, 1.0);
    fragWorldPosition = worldPosition.xyz;
    // Cloud lighting is deliberately soft; normalizing the transformed
    // normal avoids a per-vertex matrix inverse for non-uniform instances.
    fragWorldNormal = normalize(mat3(instanceTransform)*vertexNormal);
    fragLocalHeight = vertexPosition.y;
    gl_Position = mvp*worldPosition;
}
