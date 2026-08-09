#version 330

in vec3 vertexPosition;
uniform mat4 mvp;

void main()
{
    // Normal extrusion splits at the coin's hard low-poly edges. Expanding
    // from its authored center keeps the inverted hull watertight. Per-axis
    // factors compensate for the much thinner Z extent and produce roughly
    // the same chunky 0.055-unit border on every silhouette edge.
    const vec3 center = vec3(0.0, 0.20833333, 0.0);
    const vec3 outlineScale = vec3(1.265, 1.264, 1.660);
    vec3 expanded = center + (vertexPosition - center)*outlineScale;
    gl_Position = mvp*vec4(expanded, 1.0);
}
