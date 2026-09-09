#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float useTexture;

out vec4 finalColor;

void main()
{
    vec4 source = texture(texture0, fragTexCoord);
    float texturedCoverage = source.a*colDiffuse.a*fragColor.a;
    float coverage = mix(fragColor.a, texturedCoverage, useTexture);
    if (coverage <= 0.01)
    {
        discard;
    }

    // RGB is premultiplied so a one/one blend remains identical to the
    // authored additive effect. Alpha carries a post-process-only tag that
    // prevents world ink from being composited over the impact sprite.
    vec3 baseColor = mix(
        fragColor.rgb,
        source.rgb*colDiffuse.rgb*fragColor.rgb,
        useTexture);
    vec3 color = baseColor*coverage;
    finalColor = vec4(color, 0.375);
}
