#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform int outlineRadius;

out vec4 finalColor;

void main()
{
    float centerMask = texture(texture0, fragTexCoord).r;
    if (centerMask > 0.08)
    {
        discard;
    }

    const vec2 directions[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.7071,  0.7071), vec2(-0.7071,  0.7071),
        vec2( 0.7071, -0.7071), vec2(-0.7071, -0.7071)
    );
    float outsideMask = 0.0;
    float outerRadius = float(outlineRadius);
    float innerRadius = max(1.0, outerRadius*0.5);
    for (int index = 0; index < 8; ++index)
    {
        vec2 direction = directions[index]*texelSize;
        outsideMask = max(
            outsideMask,
            texture(texture0,
                    fragTexCoord + direction*outerRadius).r);
        outsideMask = max(
            outsideMask,
            texture(texture0,
                    fragTexCoord + direction*innerRadius).r);
    }

    float alpha = smoothstep(0.02, 0.22, outsideMask);
    finalColor = vec4(1.0, 1.0, 1.0, alpha)*fragColor;
}
