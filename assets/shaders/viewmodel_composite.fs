#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform float outlineEnabled;
uniform float outlineWidth;
uniform float outlineStrength;
uniform float rimStrength;
uniform float brightness;
uniform float saturation;

out vec4 finalColor;

const vec3 Luma = vec3(0.2126, 0.7152, 0.0722);

void main()
{
    vec4 source = texture(texture0, fragTexCoord);
    const vec2 Directions[4] = vec2[](
        vec2(1.0, 0.0), vec2(-1.0, 0.0),
        vec2(0.0, 1.0), vec2(0.0, -1.0));
    if (source.a <= 0.001)
    {
        if (outlineEnabled <= 0.0 || outlineStrength <= 0.0)
        {
            finalColor = vec4(0.0);
            return;
        }
        float outside = 0.0;
        for (int index = 0; index < 4; ++index)
        {
            outside = max(outside, texture(
                texture0,
                fragTexCoord + Directions[index]*texelSize*outlineWidth).a);
        }
        float outlineAlpha = outlineEnabled * outlineStrength *
            smoothstep(0.02, 0.4, outside);
        finalColor = vec4(vec3(0.055, 0.075, 0.085), outlineAlpha);
        return;
    }

    float insideMinimum = 1.0;
    for (int index = 0; index < 4; ++index)
    {
        insideMinimum = min(insideMinimum, texture(
            texture0,
            fragTexCoord + Directions[index]*texelSize*outlineWidth).a);
    }
    float luminance = dot(source.rgb, Luma);
    vec3 color = mix(vec3(luminance), source.rgb, saturation) * brightness;
    float innerEdge = source.a * (1.0 - insideMinimum);
    color += vec3(0.28, 0.43, 0.55) * innerEdge * rimStrength;
    finalColor = vec4(color, source.a) * fragColor;
}
