#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D ssaoTexture;
uniform sampler2D selectionMask;
uniform mat4 inverseProjection;
uniform vec2 ssaoTexelSize;
uniform float ssaoEnabled;
uniform float ssaoStrength;
uniform float selectionMaskEnabled;
uniform float postExposure;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float hueDegrees;
uniform float temperature;
uniform float tint;
uniform float gammaValue;
uniform float blackPoint;
uniform float curveShadows;
uniform float curveMidtones;
uniform float curveHighlights;
uniform float sharpness;
uniform float vignette;
uniform float lowHealthAmount;
uniform float lowHealthTime;
uniform float lowHealthPulse;
uniform float paletteEnabled;
uniform float paletteLevels;
uniform float ditherEnabled;
uniform float ditherStrength;
uniform float bloomEnabled;
uniform float bloomStrength;
uniform float inkOutlinesEnabled;
uniform float outlineStrength;
uniform float outlineWidth;
uniform float outlinePixelScale;
uniform float outlineFogFadeStrength;
uniform vec3 outlineFogColor;
uniform float outlineFogStart;
uniform float outlineFogEnd;
uniform float paperGrainEnabled;
uniform float paperGrainStrength;
uniform float menuDofEnabled;
uniform float menuDofFocusDistance;
uniform float menuDofFocusRange;
uniform float menuDofMaximumBlurPixels;

out vec4 finalColor;

const vec3 LuminanceWeights =
    vec3(0.2126, 0.7152, 0.0722);

float luminanceOf(vec3 color)
{
    return dot(color, LuminanceWeights);
}

vec3 reconstructViewPosition(vec2 uv, float depth)
{
    // Renderer supplies the inverse of a symmetric MatrixPerspective.
    // Omit its zero coefficients instead of multiplying a full mat4.
    vec3 clip = vec3(uv*2.0 - 1.0, depth*2.0 - 1.0);
    vec3 view = vec3(
        inverseProjection[0][0]*clip.x,
        inverseProjection[1][1]*clip.y,
        inverseProjection[2][2]*clip.z + inverseProjection[3][2]);
    float viewW = inverseProjection[2][3]*clip.z + inverseProjection[3][3];
    return view/max(viewW, 0.000001);
}

float reconstructViewZ(float depth)
{
    float clipZ = depth*2.0 - 1.0;
    float viewZ = inverseProjection[2][2]*clipZ +
        inverseProjection[3][2];
    float viewW = inverseProjection[2][3]*clipZ +
        inverseProjection[3][3];
    return viewZ/max(viewW, 0.000001);
}

vec3 decodeOctahedralNormal(vec2 encoded)
{
    vec2 octahedron = encoded*2.0 - 1.0;
    vec3 normal = vec3(
        octahedron.x, octahedron.y,
        1.0 - abs(octahedron.x) - abs(octahedron.y));
    if (normal.z < 0.0)
    {
        normal.xy =
            (1.0 - abs(normal.yx))*sign(normal.xy);
    }
    return normalize(normal);
}

float materialClassOf(vec4 packedNormal)
{
    return floor(packedNormal.b*4.0 + 0.01);
}

float screenAoMaskOf(vec3 packedNormal)
{
    float materialClass = floor(packedNormal.b*4.0 + 0.01);
    return clamp(
        (packedNormal.b - materialClass*0.25)*8.0,
        0.0, 1.0);
}

float bilateralSsao(vec2 uv)
{
    if (ssaoEnabled < 0.5)
    {
        return 0.0;
    }
    float centerDepth = texture(sceneDepth, uv).r;
    vec3 centerPackedNormal = texture(sceneNormal, uv).rgb;
    float centerMaterialClass = materialClassOf(
        vec4(centerPackedNormal, 1.0));
    float centerAoMask = screenAoMaskOf(centerPackedNormal);
    if (centerDepth >= 0.999999 ||
        centerMaterialClass < 0.5 || centerAoMask <= 0.001)
    {
        return 0.0;
    }
    vec3 centerPosition = reconstructViewPosition(uv, centerDepth);
    vec3 centerNormal = decodeOctahedralNormal(centerPackedNormal.rg);
    float total = 0.0;
    float totalWeight = 0.0;
    for (int sampleIndex = 0; sampleIndex < 5; ++sampleIndex)
    {
        vec2 offset = sampleIndex == 0
            ? vec2(0.0)
            : sampleIndex == 1
                ? vec2(1.0, 0.0)
                : sampleIndex == 2
                    ? vec2(-1.0, 0.0)
                    : sampleIndex == 3
                        ? vec2(0.0, 1.0)
                        : vec2(0.0, -1.0);
        vec2 sampleUv = clamp(
            uv + offset*ssaoTexelSize,
            ssaoTexelSize*0.5,
            vec2(1.0) - ssaoTexelSize*0.5);
        float sampleDepth = texture(sceneDepth, sampleUv).r;
        vec3 samplePackedNormal = texture(sceneNormal, sampleUv).rgb;
        float sampleMaterialClass = materialClassOf(
            vec4(samplePackedNormal, 1.0));
        float sampleAoMask = screenAoMaskOf(samplePackedNormal);
        if (sampleDepth >= 0.999999 ||
            sampleMaterialClass < 0.5 || sampleAoMask <= 0.001)
        {
            continue;
        }
        vec3 samplePosition = reconstructViewPosition(
            sampleUv, sampleDepth);
        vec3 sampleNormal = decodeOctahedralNormal(
            samplePackedNormal.rg);
        float depthWeight = exp(
            -abs(samplePosition.z - centerPosition.z)*3.6);
        float normalWeight = pow(
            max(dot(centerNormal, sampleNormal), 0.0), 5.0);
        float spatialWeight = sampleIndex == 0 ? 1.0 : 0.62;
        float weight = depthWeight*normalWeight*spatialWeight*
            sampleAoMask;
        total += texture(ssaoTexture, sampleUv).r*weight;
        totalWeight += weight;
    }
    return total/max(totalWeight, 0.0001);
}

vec4 normalPixel(ivec2 coordinate)
{
    ivec2 size = textureSize(sceneNormal, 0);
    return texelFetch(
        sceneNormal,
        clamp(coordinate, ivec2(0), size - ivec2(1)),
        0);
}

float inkDifference(
    float centerDepth, float centerMaterialClass,
    vec3 centerPosition,
    float centerIsObject, float centerIsTerrain,
    ivec2 centerCoordinate, ivec2 coordinate,
    float directionWeight)
{
    ivec2 depthSize = textureSize(sceneDepth, 0);
    float neighborDepth = texelFetch(
        sceneDepth,
        clamp(coordinate, ivec2(0), depthSize - ivec2(1)),
        0).r;
    // Ground pixels only ink against sky. Most neighbors have ground depth,
    // so avoid reading the normal buffer for those probes.
    if (centerIsTerrain > 0.5 && neighborDepth < 0.9999)
    {
        return 0.0;
    }
    vec4 neighborPackedNormal = normalPixel(coordinate);
    float neighborMaterialClass =
        materialClassOf(neighborPackedNormal);
    float neighborIsObject = step(1.5, neighborMaterialClass);
    float neighborIsBackground =
        1.0 - step(0.5, neighborMaterialClass);

    // Terrain only receives ink against the sky. It covers most screen
    // pixels, so return before the object-contact and slope calculations.
    if (centerIsTerrain > 0.5)
    {
        return neighborIsBackground*step(0.9999, neighborDepth)*
            directionWeight;
    }

    // Ordinary object ink belongs to the front surface. A same-depth or
    // nearer object neighbor produces neither depth nor class ink here.
    // Effect fragments keep their explicit class-boundary exception.
    if (neighborIsObject > 0.5 && centerMaterialClass < 3.5 &&
        neighborDepth <= centerDepth)
    {
        return 0.0;
    }

    float neighborHasDepth = 1.0 - step(0.999999, neighborDepth);
    vec2 bufferSize = vec2(depthSize);
    vec3 neighborPosition = reconstructViewPosition(
        (vec2(coordinate) + 0.5)/bufferSize, neighborDepth);

    // Object-to-terrain and object-to-sky edges do not use the expensive
    // same-class slope test. Keep their contact suppression on this path.
    if (neighborIsObject < 0.5)
    {
        float neighborSceneAlpha = texelFetch(
            texture0,
            clamp(
                coordinate, ivec2(0), textureSize(texture0, 0) - ivec2(1)),
            0).a;
        float neighborSceneTag =
            1.0 - step(0.02, abs(neighborSceneAlpha - 0.125));
        float neighborNormalTag =
            1.0 - step(0.02, abs(neighborPackedNormal.b - 0.1875));
        float neighborIsForegroundInkOccluder = max(
            neighborSceneTag, neighborNormalTag);
        float neighborIsOpaqueNoInk =
            (1.0 - step(0.5, neighborMaterialClass))*
            step(0.98, neighborSceneAlpha);
        float neighborIsInFront = step(neighborDepth, centerDepth);

    // Material tags can be absent beneath late ground overlays. Determine
    // actual ground contact from the separation of the two visible surfaces
    // in view space as well: adjacent contact pixels are close, while terrain
    // seen behind an object's outer side is substantially farther away.
        float surfaceSeparation = length(neighborPosition - centerPosition);
        float closeSurface =
            1.0 - smoothstep(0.16, 0.52, surfaceSeparation);
        // Foreground grass and thin plants must always mask ink from geometry
        // behind them. Opaque no-ink surfaces only suppress nearby contact
        // edges so they do not erase silhouettes across a large depth gap.
        float neighborSuppressesInk = max(
            neighborIsForegroundInkOccluder*neighborHasDepth*
                neighborIsInFront,
            neighborIsOpaqueNoInk * closeSurface);
        float groundContact =
            (1.0 - neighborIsObject)*neighborHasDepth*closeSurface;

        // Keep the exterior silhouette against water and sky, but leave the
        // object-to-terrain contact clean. Contact AO grounds props without
        // the dark sticker-like seam along their base.
        return centerIsObject*
            (1.0 - neighborIsObject)*(1.0 - groundContact)*
            (1.0 - neighborSuppressesInk)*directionWeight;
    }

    // Preserve separation between overlapping props of the same class. A
    // screen-space derivative of depth is unstable exactly on silhouettes:
    // neighboring 2x2 fragment groups can produce very different fwidth
    // values and turn one continuous overlap into a dotted line. Compare the
    // surfaces in view space instead, with a tolerance that grows gently at
    // distance to avoid outlining low-poly facets.
    float frontSurfaceGap = max(
        centerPosition.z - neighborPosition.z, 0.0);
    float viewDistance = length(centerPosition);
    // At distance, stacked meshes of the same material class still belong to
    // one visual object. Raise only their overlap threshold as projected
    // depth precision falls. Keep cross-class edges and background silhouettes
    // at their original thresholds so outlines do not become patchy.
    float sameClass = 1.0 - step(
        0.5, abs(centerMaterialClass - neighborMaterialClass));
    float separateSurfaceDistance =
        0.080 + viewDistance*0.0015 +
        sameClass*2.5*smoothstep(20.0, 100.0, viewDistance);
    // Depth already assigns an overlapping-object edge to the front surface.
    // Drawing a plain class boundary on both surfaces doubles the visible
    // stroke whenever vegetation overlaps a rock or another prop. Class 4 is
    // reserved for impact fragments, which still need their own contour when
    // they sit almost flush with the source object.
    float centerIsEffect = step(3.5, centerMaterialClass);
    float frontClassBoundary = smoothstep(
        separateSurfaceDistance * 0.08,
        separateSurfaceDistance * 0.30,
        frontSurfaceGap);
    float objectClassBoundary = centerIsObject*neighborIsObject*
        max(centerIsEffect, frontClassBoundary)*
        step(0.5, abs(centerMaterialClass - neighborMaterialClass));
    // A continuous face has no depth boundary. Most object neighbours are
    // on that same face: avoid two extra depth reads and the slope test when
    // smoothstep(separateSurfaceDistance, ..., frontSurfaceGap) is zero.
    if (frontSurfaceGap <= separateSurfaceDistance)
    {
        return objectClassBoundary*directionWeight;
    }
    // Hardware depth is affine across a projected planar face. Compare the
    // candidate jump with the one-sided slopes INSIDE each surface. Unlike
    // fwidth, these samples never straddle the candidate edge in 2x2 quads.
    // This rejects steep facets while preserving separate crown silhouettes.
    ivec2 offset = coordinate - centerCoordinate;
    float innerDepth = texelFetch(sceneDepth,
        clamp(centerCoordinate - offset, ivec2(0), depthSize - ivec2(1)), 0).r;
    float outerDepth = texelFetch(sceneDepth,
        clamp(coordinate + offset, ivec2(0), depthSize - ivec2(1)), 0).r;
    float innerSlope = abs(centerDepth - innerDepth);
    float outerSlope = abs(neighborDepth - outerDepth);
    // If an extra sample reaches sky, it cannot estimate this surface's slope.
    innerSlope *= 1.0 - step(0.999999, innerDepth);
    outerSlope *= 1.0 - step(0.999999, outerDepth);
    float surfaceSlope = max(innerSlope, outerSlope);
    float depthDiscontinuity = smoothstep(
        surfaceSlope*1.5 + 0.0000001,
        surfaceSlope*3.0 + 0.0000002,
        neighborDepth - centerDepth);
    float objectDepthBoundary = centerIsObject*neighborIsObject*smoothstep(
        separateSurfaceDistance,
        separateSurfaceDistance * 3.0,
        frontSurfaceGap)*depthDiscontinuity;
    // Deliberately ignore color and normal changes inside one surface.
    // Low-poly facets belong to the fill; ink is reserved for silhouettes.
    return max(objectClassBoundary, objectDepthBoundary)*directionWeight;
}

float bayer4(vec2 pixel)
{
    ivec2 position = ivec2(mod(floor(pixel), 4.0));
    const float matrix[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    return matrix[position.y*4 + position.x]/16.0 - 0.5;
}

float paperNoise(vec2 pixel)
{
    vec2 cell = floor(pixel);
    return fract(sin(dot(cell, vec2(12.9898, 78.233)))*43758.5453);
}

vec3 bloomSample(vec2 uv, vec2 texel)
{
    // A nine-tap tent retains the soft highlight halo while cutting the
    // previous 21 full-resolution texture reads by more than half.
    vec3 center = texture(texture0, uv).rgb;
    vec3 north = texture(texture0, uv + vec2(0.0, -1.5)*texel).rgb;
    vec3 south = texture(texture0, uv + vec2(0.0, 1.5)*texel).rgb;
    vec3 west = texture(texture0, uv + vec2(-1.5, 0.0)*texel).rgb;
    vec3 east = texture(texture0, uv + vec2(1.5, 0.0)*texel).rgb;
    vec3 northWest = texture(
        texture0, uv + vec2(-1.4, -1.4)*texel).rgb;
    vec3 northEast = texture(
        texture0, uv + vec2(1.4, -1.4)*texel).rgb;
    vec3 southWest = texture(
        texture0, uv + vec2(-1.4, 1.4)*texel).rgb;
    vec3 southEast = texture(
        texture0, uv + vec2(1.4, 1.4)*texel).rgb;
    center *= smoothstep(0.62, 1.0, luminanceOf(center))*4.0;
    north *= smoothstep(0.62, 1.0, luminanceOf(north))*2.0;
    south *= smoothstep(0.62, 1.0, luminanceOf(south))*2.0;
    west *= smoothstep(0.62, 1.0, luminanceOf(west))*2.0;
    east *= smoothstep(0.62, 1.0, luminanceOf(east))*2.0;
    northWest *= smoothstep(
        0.62, 1.0, luminanceOf(northWest));
    northEast *= smoothstep(
        0.62, 1.0, luminanceOf(northEast));
    southWest *= smoothstep(
        0.62, 1.0, luminanceOf(southWest));
    southEast *= smoothstep(
        0.62, 1.0, luminanceOf(southEast));
    return (center + north + south + west + east +
            northWest + northEast + southWest + southEast)/16.0;
}

float menuDepthOfFieldAmount(vec2 uv)
{
    if (menuDofEnabled < 0.5)
    {
        return 0.0;
    }
    float depth = texture(sceneDepth, uv).r;
    float viewDistance = depth < 0.999999
        ? length(reconstructViewPosition(uv, depth))
        : menuDofFocusDistance + menuDofFocusRange*4.0;
    float distanceFromFocus =
        abs(viewDistance - menuDofFocusDistance);
    return smoothstep(
        menuDofFocusRange*0.18,
        menuDofFocusRange,
        distanceFromFocus);
}

vec3 menuDepthOfFieldSample(vec2 uv, vec2 texel, float amount)
{
    if (amount <= 0.001)
    {
        return texture(texture0, uv).rgb;
    }
    vec2 radius = texel*menuDofMaximumBlurPixels*amount;
    vec3 result = texture(texture0, uv).rgb*0.262;
    result += texture(texture0, clamp(
        uv + vec2( 0.000,  1.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.707,  0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 1.000,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.707, -0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.000, -1.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-0.707, -0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-1.000,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2(-0.707,  0.707)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.082;
    result += texture(texture0, clamp(
        uv + vec2( 0.360,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.041;
    result += texture(texture0, clamp(
        uv + vec2(-0.360,  0.000)*radius,
        vec2(0.0), vec2(1.0))).rgb*0.041;
    return result;
}

vec3 rotateHue(vec3 color, float angle)
{
    float cosine = cos(angle);
    float sine = sin(angle);
    mat3 rotation = mat3(
        0.299 + 0.701*cosine + 0.168*sine,
        0.587 - 0.587*cosine + 0.330*sine,
        0.114 - 0.114*cosine - 0.497*sine,
        0.299 - 0.299*cosine - 0.328*sine,
        0.587 + 0.413*cosine + 0.035*sine,
        0.114 - 0.114*cosine + 0.292*sine,
        0.299 - 0.300*cosine + 1.250*sine,
        0.587 - 0.588*cosine - 1.050*sine,
        0.114 + 0.886*cosine - 0.203*sine);
    return rotation*color;
}

void main()
{
    ivec2 sourceSize = textureSize(texture0, 0);
    vec2 healthCentered = fragTexCoord*2.0 - 1.0;
    float healthRadius = length(healthCentered);
    float healthEdge = smoothstep(0.18, 1.18, healthRadius);
    vec2 healthDirection = vec2(0.0);
    vec2 healthUv = fragTexCoord;
    if (lowHealthAmount > 0.001)
    {
        healthDirection = healthRadius > 0.0001
            ? healthCentered/healthRadius
            : vec2(0.0);
        float healthWave = sin(
            healthRadius*17.0 - lowHealthTime*2.4);
        healthUv = clamp(
            fragTexCoord + healthDirection*healthWave*healthEdge*
                lowHealthAmount*0.0032,
            vec2(0.0), vec2(1.0));
    }
    ivec2 sourceCoordinate = clamp(
        ivec2(floor(healthUv*vec2(sourceSize))),
        ivec2(0), sourceSize - ivec2(1));
    vec2 pixelUv =
        (vec2(sourceCoordinate) + 0.5)/vec2(sourceSize);
    vec4 source =
        texelFetch(texture0, sourceCoordinate, 0)*fragColor;
    float selectedSurface = 0.0;
    if (selectionMaskEnabled > 0.5)
    {
        vec3 selectionColor = texture(selectionMask, pixelUv).rgb;
        selectedSurface = smoothstep(
            0.05, 0.35,
            max(selectionColor.r,
                max(selectionColor.g, selectionColor.b)));
    }
    // Opaque world geometry leaves scene alpha at one. Transparent world
    // overlays (leaves, pollen, grass and authored impact sprites) change or
    // explicitly tag that channel without replacing the geometry normal and
    // depth behind them. Do not composite the rear object's ink over those
    // foreground pixels.
    float foregroundOverlayMask =
        step(0.025, abs(source.a - 1.0));
    vec2 texel = 1.0/vec2(sourceSize);
    float menuDofAmount = menuDepthOfFieldAmount(pixelUv);
    source.rgb = menuDepthOfFieldSample(
        pixelUv, texel, menuDofAmount)*fragColor.rgb;
    if (lowHealthAmount > 0.001)
    {
        vec2 chromaticOffset = healthDirection*texel*2.2*
            healthEdge*lowHealthAmount;
        source.r = texture(
            texture0, clamp(healthUv + chromaticOffset,
                            vec2(0.0), vec2(1.0))).r;
        source.b = texture(
            texture0, clamp(healthUv - chromaticOffset,
                            vec2(0.0), vec2(1.0))).b;
    }
    vec2 stylePixel = vec2(sourceCoordinate);
    vec3 color = source.rgb;
    float contactAo = bilateralSsao(pixelUv)*
        clamp(ssaoStrength, 0.0, 0.6)*
        (1.0 - selectedSurface);
    // Cool green-grey contact, never black. Keeps bright low-poly palette.
    color *= mix(
        vec3(1.0), vec3(0.38, 0.52, 0.47), contactAo);
    if (abs(sharpness) > 0.001)
    {
        vec3 neighborhood =
            (texture(texture0, pixelUv + vec2(texel.x, 0.0)).rgb +
             texture(texture0, pixelUv - vec2(texel.x, 0.0)).rgb +
             texture(texture0, pixelUv + vec2(0.0, texel.y)).rgb +
             texture(texture0, pixelUv - vec2(0.0, texel.y)).rgb)*0.25;
        color += (source.rgb - neighborhood)*sharpness*0.72*
            (1.0 - menuDofAmount);
    }
    if (bloomEnabled > 0.5)
    {
        color += bloomSample(pixelUv, texel)*bloomStrength;
    }
    color *= exp2(postExposure);
    color += brightness;
    color =
        max(color - vec3(blackPoint), vec3(0.0))/
        max(1.0 - blackPoint, 0.001);
    color = (color - 0.5)*contrast + 0.5;

    color += vec3(
        temperature*0.10 - tint*0.025,
        tint*0.075,
        -temperature*0.10 - tint*0.025);
    color = rotateHue(color, radians(hueDegrees));
    float luminance = dot(color, LuminanceWeights);
    color = mix(vec3(luminance), color, saturation);

    // Uniform neutral settings have the same result without per-pixel
    // curve weights/exponentials. Edited curves retain the original path.
    if (curveShadows != 0.0 || curveMidtones != 0.0 || curveHighlights != 0.0)
    {
        float shadowWeight = 1.0 - smoothstep(0.08, 0.52, luminance);
        float highlightWeight = smoothstep(0.48, 0.92, luminance);
        float midtoneWeight = clamp(1.0 - shadowWeight - highlightWeight, 0.0, 1.0);
        float curveExposure = curveShadows*shadowWeight +
            curveMidtones*midtoneWeight + curveHighlights*highlightWeight;
        color *= exp2(curveExposure*0.85);
    }
    color = max(color, vec3(0.0));
    if (gammaValue != 1.0)
    {
        color = pow(color, vec3(1.0/max(gammaValue, 0.01)));
    }

    float dither = 0.0;
    if (ditherEnabled > 0.5)
    {
        dither = bayer4(stylePixel)*ditherStrength;
    }
    if (paletteEnabled > 0.5)
    {
        float levels = max(round(paletteLevels), 2.0);
        color = floor(clamp(color, 0.0, 1.0)*(levels - 1.0) +
                      0.5 + dither)/(levels - 1.0);
    }
    else if (ditherEnabled > 0.5)
    {
        color += dither/255.0*6.0;
    }

    vec4 centerPackedNormal = normalPixel(sourceCoordinate);
    float centerMaterialClass = materialClassOf(centerPackedNormal);
    float effectFragment = step(3.5, centerMaterialClass);
    if (inkOutlinesEnabled > 0.5 && centerMaterialClass > 0.5 &&
        foregroundOverlayMask < 0.5 &&
        (selectedSurface < 0.5 || effectFragment > 0.5))
    {
        float centerDepth = texture(sceneDepth, pixelUv).r;
        vec3 centerPosition = reconstructViewPosition(
            pixelUv, centerDepth);
        float viewDistance = centerDepth < 0.999999
            ? length(centerPosition)
            : 100000.0;
        // Distance controls both opacity and width. Far silhouettes keep a
        // fine line instead of disappearing, while nearby props receive the
        // chunky contour used by the HUD and concept art.
        float nearDistance =
            1.0 - smoothstep(24.0, 120.0, viewDistance);
        // Distant silhouettes stay solid enough to survive FXAA instead of
        // breaking into short dashes along thin tree crowns and branches.
        float outlineDistance = mix(0.56, 1.0, nearDistance);
        // The auxiliary normal/AO target marks trees and sizeable plants.
        // Their ink disappears earlier than prop/enemy ink, including the
        // terrain-side half of an edge, so distant foliage becomes a clean
        // mass instead of line noise without making its RGB transparent.
        float vegetationOutlineFade = mix(
            0.82, 1.0,
            1.0 - smoothstep(32.0, 125.0, viewDistance));
        float sourceVegetation =
            1.0 - step(
                0.5, abs(centerMaterialClass - 3.0));
        outlineDistance *= mix(
            1.0, vegetationOutlineFade, sourceVegetation);
        float edge = 0.0;
        // Keep the close-up contour only slightly wider than the distant
        // one. The previous 1.75 multiplier turned the minimum UI setting
        // into a nearly two-pixel line.
        float distanceWidthScale = mix(
            1.00, 1.40,
            1.0 - smoothstep(10.0, 78.0, viewDistance));
        // Trees become thinner sooner than solid props, but retain one full
        // pixel even in the distant forest.
        distanceWidthScale *= mix(
            1.0,
            mix(1.0, 0.94, smoothstep(28.0, 105.0, viewDistance)),
            sourceVegetation);
        float effectiveWidth = clamp(
            outlineWidth * distanceWidthScale, 1.0, 3.0);
        effectiveWidth = min(
            effectiveWidth,
            mix(3.0, 2.0, sourceVegetation));
        float sourceTerrain = step(0.5, centerMaterialClass)*
            (1.0 - step(1.5, centerMaterialClass));
        float sourceObject = step(1.5, centerMaterialClass);
        // Mountain contours stay a single fine screen pixel at every distance.
        effectiveWidth = mix(effectiveWidth, 1.0, sourceTerrain);
        // The scene target can be smaller or larger than the display.
        // Convert the requested screen-pixel width to scene texels before
        // probing depth and material boundaries.
        effectiveWidth *= outlinePixelScale;
        // Fractional outer rings give nearby silhouettes a soft extra edge
        // instead of jumping directly between hard one- and two-pixel lines.
        // Six rings cover the largest 2x scene target without clipping the
        // configured three-pixel contour.
        int pixelWidth = min(int(ceil(effectiveWidth)), 6);
        for (int ring = 1; ring <= pixelWidth; ++ring)
        {
            ivec2 horizontal = ivec2(ring, 0);
            ivec2 vertical = ivec2(0, ring);
            float ringEdge = 0.0;
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass,
                centerPosition, sourceObject, sourceTerrain,
                sourceCoordinate, sourceCoordinate + horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass,
                centerPosition, sourceObject, sourceTerrain,
                sourceCoordinate, sourceCoordinate - horizontal, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass,
                centerPosition, sourceObject, sourceTerrain,
                sourceCoordinate, sourceCoordinate + vertical, 1.0));
            ringEdge = max(ringEdge, inkDifference(
                centerDepth, centerMaterialClass,
                centerPosition, sourceObject, sourceTerrain,
                sourceCoordinate, sourceCoordinate - vertical, 1.0));
            float ringCoverage = clamp(
                effectiveWidth - float(ring) + 1.0,
                0.0, 1.0);
            edge = max(edge, ringEdge * ringCoverage);
        }
        // An interior pixel with no edge has zero ink. Skip the atmospheric
        // exponentials for it without changing silhouette width or fading.
        if (edge > 0.0)
        {
            // A steeper response restores the chunky foreground silhouette.
            // Distance still removes it from the background independently.
            float nearBoost = mix(1.0, 1.24,
                1.0 - smoothstep(18.0, 32.0, viewDistance));
            float ink = smoothstep(0.10, 0.52, edge)*
                sqrt(clamp(outlineStrength, 0.0, 1.0))*1.3*
                outlineDistance*nearBoost;
            // A sub-texel stroke cannot shrink geometrically in a low-resolution
            // target, so reduce its coverage before the image is enlarged.
            ink *= min(effectiveWidth, 1.0);
            // Keep one stable near-black ink colour for every world material.
            // Vegetation already receives its own distance/width treatment, so
            // tinting its contour only makes the style inconsistent at night.
            const vec3 inkColor = vec3(0.006, 0.008, 0.010);
            // Match the world shader's atmospheric curve using view distance
            // reconstructed from the same scene depth. Near ink is untouched;
            // distant ink approaches haze colour without a hard disappearance.
            float outlineFogRange = max(
                outlineFogEnd - outlineFogStart, 0.01);
            float outlineFogDistance = clamp(
                (viewDistance - outlineFogStart)/outlineFogRange,
                0.0, 1.0);
            float outlineHazeCurve = pow(
                smoothstep(0.0, 1.0, outlineFogDistance), 1.45);
            float outlineDistanceFog =
                (1.0 - exp(
                    -2.45*outlineHazeCurve*outlineHazeCurve))*0.82;
            float outlineFogFade = clamp(
                outlineDistanceFog*outlineFogFadeStrength,
                0.0, 1.0);
            // Atmosphere fades distant ink instead of recolouring it with the
            // current fog. This keeps contours black through the day/night cycle.
            ink *= 1.0 - outlineFogFade*0.65;
            color = mix(color, inkColor, clamp(ink, 0.0, 0.985));
        }
    }

    if (paperGrainEnabled > 0.5)
    {
        float grain = paperNoise(stylePixel) - 0.5;
        color *= 1.0 + grain*paperGrainStrength*2.0;
    }

    vec2 centered = fragTexCoord*2.0 - 1.0;
    float vignetteMask =
        smoothstep(0.28, 1.25, dot(centered, centered));
    color *= 1.0 - vignetteMask*vignette;

    float heartbeat = 0.5 + 0.5*sin(lowHealthTime*4.1);
    heartbeat = heartbeat*heartbeat*lowHealthPulse;
    float dangerMask = smoothstep(
        0.12, 1.08, dot(centered, centered));
    float danger = clamp(
        dangerMask*lowHealthAmount*(0.72 + heartbeat*0.18),
        0.0, 0.88);
    vec3 dangerColor =
        color*vec3(0.58, 0.24, 0.29) +
        vec3(0.045 + heartbeat*0.018, 0.0, 0.008);
    color = mix(color, dangerColor, danger);
    float centerDrain = lowHealthAmount*
        (1.0 - smoothstep(
            0.05, 1.0, dot(centered, centered)))*0.08;
    float healthLuminance = luminanceOf(color);
    color = mix(color, vec3(healthLuminance), centerDrain);

    finalColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
