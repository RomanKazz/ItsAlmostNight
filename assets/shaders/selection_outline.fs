#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D sceneDepth;
uniform sampler2D selectionDepth;
uniform sampler2D effectCoverage;
uniform float effectCoverageEnabled;
uniform vec2 texelSize;
uniform int outlineRadius;
uniform mat4 inverseProjection;
uniform float depthOcclusionEnabled;

out vec4 finalColor;

vec4 selectionSample(vec2 uv)
{
    return texture(texture0, uv);
}

float viewDepthAt(vec2 uv, float depth)
{
    if (depth >= 0.999999)
    {
        return 1000000.0;
    }
    vec4 clip = vec4(uv*2.0 - 1.0, depth*2.0 - 1.0, 1.0);
    vec4 view = inverseProjection*clip;
    return max(-view.z/max(view.w, 0.000001), 0.001);
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
    float outsideInverseDepth = 0.0;
    vec2 outsideUv = fragTexCoord;
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
            outsideInverseDepth = outerSample.a/max(outerMask, 0.001);
            outsideUv = outerUv;
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
            outsideInverseDepth = innerSample.a/max(innerMask, 0.001);
            outsideUv = innerUv;
        }
    }

    float alpha = exteriorCoverage*
        smoothstep(0.05, 0.95, outsideMask);
    // Use the mask's actual depth attachment, not its quantised RGBA8 alpha.
    // Alpha precision allowed close terrain to look level with a selected
    // surface, drawing a contour along the terrain's cut through the object.
    float selectedDepth = viewDepthAt(outsideUv, texture(selectionDepth, outsideUv).r);
    if (centerMask < 0.001 && centerSample.a > 0.0)
    {
        float maskDepth = viewDepthAt(fragTexCoord, texture(selectionDepth, fragTexCoord).r);
        if (maskDepth + 0.002 < selectedDepth) discard;
    }
    if (depthOcclusionEnabled > 0.5)
    {
        // Selection is rendered after the world. Hide a dilated outline pixel
        // whenever the visible scene surface is in front of the selected
        // sample that produced it. A 2 mm tolerance leaves the true silhouette
        // intact while suppressing even shallow terrain intersections.
        float visibleDepth = viewDepthAt(fragTexCoord, texture(sceneDepth, fragTexCoord).r);
        if (visibleDepth + 0.002 < selectedDepth) discard;
    }
    if (alpha <= 0.001)
    {
        discard;
    }
    if (effectCoverageEnabled > 0.5)
    {
        vec2 effect = texture(effectCoverage, fragTexCoord).rg;
        float effectInverseDepth = effect.y/max(effect.x, 0.001);
        // A wake behind the selected object must not erase its silhouette.
        float inFront = step(outsideInverseDepth - 1.0/255.0, effectInverseDepth);
        alpha *= 1.0 - effect.x*inFront;
    }
    finalColor = vec4(outsideColor, alpha)*fragColor;
}
