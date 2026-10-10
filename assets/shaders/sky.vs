#version 330

in vec3 vertexPosition;
uniform mat4 mvp;

void main()
{
    gl_Position = mvp*vec4(vertexPosition, 1.0);
    // Empty scene pixels have depth one. Let fixed-function early depth
    // reject hidden sky before the expensive stars and aurora shader runs.
    gl_Position.z = gl_Position.w;
}
