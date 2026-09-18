#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragVertexColor;

void main()
{
    vec3 transformedNormal =
        (matNormal*vec4(vertexNormal, 0.0)).xyz;
    fragNormal = dot(transformedNormal, transformedNormal) < 0.000001
        ? vec3(0.0, 1.0, 0.0)
        : normalize(transformedNormal);
    fragTexCoord = vertexTexCoord;
    fragVertexColor = vertexColor;
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
