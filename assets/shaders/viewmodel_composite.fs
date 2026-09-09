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
uniform vec3 environmentTint;
uniform float environmentBrightness;

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
        finalColor = vec4(vec3(0.035, 0.032, 0.030), outlineAlpha);
        return;
    }

    float insideMinimum = 1.0;
    float neighboringLuminance = 0.0;
    for (int index = 0; index < 4; ++index)
    {
        vec4 neighbor = texture(
            texture0,
            fragTexCoord + Directions[index]*texelSize*outlineWidth);
        insideMinimum = min(insideMinimum, neighbor.a);
        neighboringLuminance += dot(neighbor.rgb, Luma);
    }
    float luminance = dot(source.rgb, Luma);
    vec3 color = mix(vec3(luminance), source.rgb, saturation) * brightness;
    // The viewmodel remains a gameplay-readable overlay pass, but its surface
    // now follows the same continuously interpolated environment as the world.
    // The exterior outline branch above is deliberately unaffected.
    color *= environmentTint * environmentBrightness;
    // Screen-space key/fill treatment for the isolated viewmodel: a warm
    // top-left key and a restrained blue-grey lower fill make the tool share
    // the world's lighting direction without flattening its painted facets.
    float keyAmount = clamp(
        (1.0 - fragTexCoord.y) * 0.72 +
        (1.0 - fragTexCoord.x) * 0.28,
        0.0, 1.0);
    vec3 coolFill = vec3(0.88, 0.93, 1.01);
    vec3 warmKey = vec3(1.07, 1.025, 0.94);
    color *= mix(coolFill, warmKey, keyAmount);
    color *= mix(0.93, 1.06, keyAmount);
    // Darken narrow tonal recesses (for example head/handle joints) using the
    // already sampled neighbourhood as a lightweight contact-AO cue.
    float localAverage = neighboringLuminance * 0.25;
    float contactOcclusion = clamp(
        (localAverage - luminance) * 0.55, 0.0, 0.14);
    color *= 1.0 - contactOcclusion;
    float innerEdge = source.a * (1.0 - insideMinimum);
    color += vec3(0.22, 0.32, 0.42) * innerEdge * rimStrength;
    finalColor = vec4(color, source.a) * fragColor;
}
