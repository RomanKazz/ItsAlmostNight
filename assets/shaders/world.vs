#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightViewProjection;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragVertexColor;
out vec2 fragTexCoord;
out vec4 fragLightSpacePosition;

void main()
{
    vec4 worldPosition = matModel*vec4(vertexPosition, 1.0);

    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = normalize((matNormal*vec4(vertexNormal, 0.0)).xyz);
    fragVertexColor = vertexColor;
    fragTexCoord = vertexTexCoord;
    fragLightSpacePosition = lightViewProjection*worldPosition;

    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
