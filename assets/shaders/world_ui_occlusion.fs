#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
out vec4 finalColor;
void main() {
    vec2 uv = gl_FragCoord.xy / viewportSize;
    float depth = texture(sceneDepth, uv).r;
    // Tiny precision allowance, not a world-space push through foreground props.
    if (gl_FragCoord.z > depth + 0.000002) discard;
    finalColor = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
}
