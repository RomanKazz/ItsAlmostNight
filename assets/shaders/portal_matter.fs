#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

void main()
{
    vec4 source = texture(texture0, fragTexCoord);
    float coverage = source.a * fragColor.a;
    if (coverage <= 0.01)
    {
        discard;
    }

    finalColor = vec4(source.rgb * fragColor.rgb, coverage);
    // Opaque portal matter replaces the screen-space material mask. This
    // prevents silhouettes from geometry behind the portal leaking through.
    normalAo = vec4(0.0, 0.0, 0.0, coverage);
}
