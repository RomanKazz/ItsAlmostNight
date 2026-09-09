#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D sceneDepth;
uniform vec2 texelSize;
uniform int outlineRadius;
uniform mat4 inverseProjection;
uniform float depthOcclusionEnabled;

out vec4 finalColor;

vec4 selectionSample(vec2 uv)
{
    vec4 value = texture(texture0, uv);
    // RGB is bilinear coverage for a smooth silhouette. Keep the packed
    // inverse depth point-sampled so interpolation cannot move occlusion
    // boundaries forward or backward.
    ivec2 dimensions = textureSize(texture0, 0);
    ivec2 coordinate = clamp(
        ivec2(floor(uv*vec2(dimensions))),
        ivec2(0), dimensions - ivec2(1));
    value.a = texelFetch(texture0, coordinate, 0).a;
    return value;
}

float sceneInverseDepthAt(vec2 uv)
{
    float depth = texture(sceneDepth, uv).r;
    if (depth >= 0.999999)
    {
        return 0.0;
    }
    vec4 clip = vec4(uv*2.0 - 1.0, depth*2.0 - 1.0, 1.0);
    vec4 view = inverseProjection*clip;
    float viewDepth = max(-view.z/max(view.w, 0.000001), 0.0001);
    return clamp(0.5/viewDepth, 1.0/255.0, 1.0);
}

bool hiddenByScene(float selectedInverseDepth)
{
    if (depthOcclusionEnabled < 0.5)
    {
        return false;
    }
    // The selection depth is stored in an RGBA8 alpha channel. Allow a small
    // quantization margin so coplanar pixels from the selected object do not
    // erase their own exterior contour.
    const float depthBias = 1.5/255.0;
    return sceneInverseDepthAt(fragTexCoord) >
        selectedInverseDepth + depthBias;
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
    float centerInverseDepth = centerSample.a;
    // Alpha is stored in an 8-bit target. A little more than one quantized
    // step prevents equal-depth surfaces from flickering at intersections.
    const float depthBias = 1.5/255.0;
    float outerRadius = float(outlineRadius);
    float innerRadius = max(1.0, outerRadius*0.5);
    for (int index = 0; index < 8; ++index)
    {
        vec2 direction = directions[index]*texelSize;
        vec4 outerSample = selectionSample(
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
        if (hiddenByScene(outerSample.a))
        {
            outerMask = 0.0;
        }
        if (outerMask > outsideMask)
        {
            outsideMask = outerMask;
            outsideColor = outerColor;
        }
        vec4 innerSample = selectionSample(
            fragTexCoord + direction*innerRadius);
        vec3 innerColor = innerSample.rgb;
        float innerMask = max(
            innerColor.r, max(innerColor.g, innerColor.b));
        if (centerInverseDepth >
            innerSample.a + depthBias)
        {
            innerMask = 0.0;
        }
        if (hiddenByScene(innerSample.a))
        {
            innerMask = 0.0;
        }
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
