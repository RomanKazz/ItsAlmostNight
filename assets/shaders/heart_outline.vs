#version 330

in vec3 vertexPosition;
uniform mat4 mvp;

void main()
{
    // The heart has different proportions and origin than the coins.
    // Expanding around its actual bounds center keeps the white hull even.
    const vec3 center = vec3(0.0, 0.811347, 0.0);
    // 40% wider than the previous heart-specific outline.
    const vec3 outlineScale = vec3(1.154, 1.1792, 1.322);
    vec3 expanded = center + (vertexPosition - center)*outlineScale;
    gl_Position = mvp*vec4(expanded, 1.0);
}
