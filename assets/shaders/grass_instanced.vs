#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform float timeSeconds;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorldPosition;
out float fragLocalHeight;


uniform vec2 worldBiomeCenters[4];
uniform vec4 worldBiomeShape;
out vec4 fragBiomeWeights;

// Shared with visual_biomes::weights. Evaluate per vertex, not per screen pixel.
vec4 visualBiomeWeights(vec2 position)
{
    if (worldBiomeShape.x <= 0.0) return vec4(1.0, 0.0, 0.0, 0.0);
    vec2 q = position/worldBiomeShape.x;
    position += vec2(
        sin(q.y*11.0 + worldBiomeShape.z) + 0.45*sin((q.x + q.y)*19.0 + worldBiomeShape.w),
        sin(q.x*12.0 + worldBiomeShape.w) + 0.45*sin((q.y - q.x)*17.0 + worldBiomeShape.z))*worldBiomeShape.x*0.018;
    vec4 distances = vec4(
        length(position - worldBiomeCenters[0]), length(position - worldBiomeCenters[1]),
        length(position - worldBiomeCenters[2]), length(position - worldBiomeCenters[3]));
    float closest = min(min(distances.x, distances.y), min(distances.z, distances.w));
    vec4 weights = 1.0 - smoothstep(vec4(0.0), vec4(worldBiomeShape.y), distances - closest);
    return weights/dot(weights, vec4(1.0));
}

void main()
{
    vec4 worldPosition =
        instanceTransform*vec4(vertexPosition, 1.0);
    float windHeight =
        smoothstep(0.02, 0.85, vertexPosition.y);
    float phase =
        timeSeconds*2.1 +
        worldPosition.x*0.47 +
        worldPosition.z*0.39;
    float wave =
        sin(phase) + sin(phase*1.91 + timeSeconds)*0.28;
    worldPosition.xz +=
        vec2(0.075, 0.035)*wave*windHeight;

    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(mat3(instanceTransform)*vertexNormal);
    fragBiomeWeights = visualBiomeWeights(worldPosition.xz);
    fragWorldPosition = worldPosition.xyz;
    fragLocalHeight = vertexPosition.y;
    gl_Position = mvp*worldPosition;
}
