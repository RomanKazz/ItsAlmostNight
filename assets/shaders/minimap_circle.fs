#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

out vec4 finalColor;

void main()
{
    vec2 offset = fragTexCoord - vec2(0.5);
    float distanceFromCenter = length(offset);
    if (distanceFromCenter >= 0.5)
    {
        discard;
    }
    float edgeAlpha = 1.0 - smoothstep(
        0.492, 0.5, distanceFromCenter);
    finalColor = texture(texture0, fragTexCoord) *
        fragColor * vec4(1.0, 1.0, 1.0, edgeAlpha);
}
