#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform int outlineRadius;

out vec4 finalColor;

void main()
{
    vec4 centerSample = texture(texture0, fragTexCoord);
    vec3 centerColor = centerSample.rgb;
    float centerMask = max(centerColor.r, max(centerColor.g, centerColor.b));
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
    vec3 outsideColor = vec3(1.0);
    float centerInverseDepth = centerSample.a;
    // Alpha is stored in an 8-bit target. A little more than one quantized
    // step prevents equal-depth surfaces from flickering at intersections.
    const float depthBias = 1.5/255.0;
    float outerRadius = float(outlineRadius);
    float innerRadius = max(1.0, outerRadius*0.5);
    for (int index = 0; index < 8; ++index)
    {
        vec2 direction = directions[index]*texelSize;
        vec4 outerSample = texture(
            texture0,
            fragTexCoord + direction*outerRadius);
        vec3 outerColor = outerSample.rgb;
        float outerMask = max(
            outerColor.r, max(outerColor.g, outerColor.b));
        // A black center pixel is chest geometry. Keep the outline only when
        // the nearby selected surface is at least as close to the camera.
        if (centerInverseDepth >
            outerSample.a + depthBias)
        {
            outerMask = 0.0;
        }
        if (outerMask > outsideMask)
        {
            outsideMask = outerMask;
            outsideColor = outerColor;
        }
        vec4 innerSample = texture(
            texture0,
            fragTexCoord + direction*innerRadius);
        vec3 innerColor = innerSample.rgb;
        float innerMask = max(
            innerColor.r, max(innerColor.g, innerColor.b));
        if (centerInverseDepth >
            innerSample.a + depthBias)
        {
            innerMask = 0.0;
        }
        if (innerMask > outsideMask)
        {
            outsideMask = innerMask;
            outsideColor = innerColor;
        }
    }

    float alpha = smoothstep(0.02, 0.22, outsideMask);
    finalColor = vec4(outsideColor, alpha)*fragColor;
}
