#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform int outlineRadius;

out vec4 finalColor;

vec4 selectionSample(vec2 uv)
{
    return texture(texture0, uv);
}

void main()
{
    vec4 centerSample = selectionSample(fragTexCoord);
    vec3 centerColor = centerSample.rgb;
    float centerMask = max(centerColor.r, max(centerColor.g, centerColor.b));
    float exteriorCoverage = 1.0 -
        smoothstep(0.05, 0.95, centerMask);

    const vec2 directions[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.7071,  0.7071), vec2(-0.7071,  0.7071),
        vec2( 0.7071, -0.7071), vec2(-0.7071, -0.7071)
    );
    float outsideMask = 0.0;
    vec3 outsideColor = vec3(1.0);
    float outerRadius = float(outlineRadius);
    float innerRadius = max(1.0, outerRadius*0.5);
    for (int index = 0; index < 8; ++index)
    {
        vec2 direction = directions[index]*texelSize;
        vec2 outerUv = fragTexCoord + direction*outerRadius;
        vec4 outerSample = selectionSample(outerUv);
        vec3 outerColor = outerSample.rgb;
        float outerMask = max(
            outerColor.r, max(outerColor.g, outerColor.b));
        if (outerMask > outsideMask)
        {
            outsideMask = outerMask;
            outsideColor = outerColor;
        }
        // Keep the inner ring for thin branches, posts and weapon parts: a
        // six-pixel-only probe can jump completely across that geometry.
        vec2 innerUv = fragTexCoord + direction*innerRadius;
        vec4 innerSample = selectionSample(innerUv);
        vec3 innerColor = innerSample.rgb;
        float innerMask = max(
            innerColor.r, max(innerColor.g, innerColor.b));
        if (innerMask > outsideMask)
        {
            outsideMask = innerMask;
            outsideColor = innerColor;
        }
    }

    float alpha = exteriorCoverage*
        smoothstep(0.05, 0.95, outsideMask);
    if (alpha <= 0.001)
    {
        discard;
    }
    finalColor = vec4(outsideColor, alpha)*fragColor;
}
