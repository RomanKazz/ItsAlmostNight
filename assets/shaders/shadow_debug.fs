#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

out vec4 finalColor;

void main()
{
    float depth = texture(texture0, fragTexCoord).r;
    float contrast = pow(clamp(1.0 - depth, 0.0, 1.0), 0.35);
    finalColor = vec4(vec3(contrast), 1.0)*fragColor;
}
