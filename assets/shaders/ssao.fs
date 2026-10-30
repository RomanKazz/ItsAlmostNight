#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform mat4 projection;
uniform mat4 inverseProjection;
uniform mat4 viewMatrix;
uniform vec2 texelSize;
uniform float radius;
uniform float bias;
uniform float fadeStart;
uniform float fadeEnd;
uniform int sampleCount;

out vec4 finalColor;

const vec3 Kernel[12] = vec3[12](
    vec3( 0.5381,  0.1856, 0.4319),
    vec3(-0.1379,  0.2486, 0.4430),
    vec3( 0.3371,  0.5679, 0.2058),
    vec3(-0.6999, -0.0451, 0.2869),
    vec3( 0.0689, -0.6999, 0.3141),
    vec3( 0.5624, -0.4085, 0.2448),
    vec3(-0.4186, -0.4237, 0.3525),
    vec3(-0.3219,  0.6082, 0.2843),
    vec3( 0.8210,  0.1320, 0.1960),
    vec3(-0.7320,  0.3710, 0.2240),
    vec3( 0.2040, -0.8540, 0.1710),
    vec3(-0.0860,  0.1120, 0.9130));

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

void main()
{
    float centerDepth = texture(sceneDepth, fragTexCoord).r;
    vec3 packedNormal = texture(sceneNormal, fragTexCoord).rgb;
    float materialClass = floor(packedNormal.b*4.0 + 0.01);
    float aoMask = clamp(
        (packedNormal.b - materialClass*0.25)*8.0,
        0.0, 1.0);
    if (centerDepth >= 0.999999 ||
        materialClass < 0.5 || aoMask <= 0.001)
    {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 centerPosition = reconstructViewPosition(
        fragTexCoord, centerDepth);
    float distanceToCamera = length(centerPosition);
    float distanceFade = 1.0 - smoothstep(
        fadeStart, max(fadeEnd, fadeStart + 0.01), distanceToCamera);
    // Beyond the existing fade range every sample is multiplied by zero.
    // Avoid normal reconstruction and the entire depth/normal sample kernel.
    if (distanceFade <= 0.0)
    {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0)*fragColor;
        return;
    }
    vec3 worldNormal = decodeOctahedralNormal(packedNormal.rg);
    vec3 normal = normalize(mat3(viewMatrix)*worldNormal);
    vec3 helper = abs(normal.z) < 0.999
        ? vec3(0.0, 0.0, 1.0)
        : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(helper, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tangentBasis = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float totalWeight = 0.0;
    for (int index = 0; index < sampleCount; ++index)
    {
        float scale = float(index + 1)/12.0;
        scale = mix(0.16, 1.0, scale*scale);
        vec3 samplePosition = centerPosition +
            tangentBasis*Kernel[index]*radius*scale;
        // Only xy/w is consumed, and the perspective matrix is sparse.
        float sampleW = projection[2][3]*samplePosition.z + projection[3][3];
        if (sampleW <= 0.0)
        {
            continue;
        }
        vec2 sampleUv = vec2(
            projection[0][0]*samplePosition.x,
            projection[1][1]*samplePosition.y)/sampleW*0.5 + 0.5;
        if (sampleUv.x <= texelSize.x ||
            sampleUv.x >= 1.0 - texelSize.x ||
            sampleUv.y <= texelSize.y ||
            sampleUv.y >= 1.0 - texelSize.y)
        {
            continue;
        }
        float sampleDepth = texture(sceneDepth, sampleUv).r;
        if (sampleDepth >= 0.999999)
        {
            continue;
        }
        vec3 samplePackedNormal = texture(sceneNormal, sampleUv).rgb;
        float sampleMaterialClass = floor(
            samplePackedNormal.b*4.0 + 0.01);
        float sampleAoMask = clamp(
            (samplePackedNormal.b - sampleMaterialClass*0.25)*8.0,
            0.0, 1.0);
        if (sampleMaterialClass < 0.5 || sampleAoMask <= 0.001)
        {
            continue;
        }
        float sampleViewZ = reconstructViewZ(sampleDepth);
        float depthDelta = abs(centerPosition.z - sampleViewZ);
        float rangeWeight = smoothstep(
            0.0, 1.0, radius/max(depthDelta, 0.0001));
        float weight = rangeWeight*sampleAoMask;
        occlusion +=
            (sampleViewZ >= samplePosition.z + bias ? 1.0 : 0.0)*
            weight;
        totalWeight += weight;
    }

    float amount = totalWeight > 0.001
        ? occlusion/totalWeight
        : 0.0;
    amount = pow(clamp(amount, 0.0, 1.0), 1.18)*
        aoMask*distanceFade;
    finalColor = vec4(vec3(amount), 1.0)*fragColor;
}
