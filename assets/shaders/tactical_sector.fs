#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform vec4 colDiffuse;
// Outer radius, inner dead zone, half angle in radians, rim width in metres.
uniform vec4 sectorShape;
out vec4 finalColor;

void main()
{
    float distance = length(fragTexCoord);
    float outerDistance = sectorShape.x - distance;
    float innerDistance = distance - sectorShape.y;
    float edgeAngle = sectorShape.z - abs(atan(fragTexCoord.x, fragTexCoord.y));
    bool circle = sectorShape.z >= 3.14059;
    float sideDistance = distance * sin(clamp(edgeAngle, -1.570796, 1.570796));

    // Antialias the exact circle/sector boundary without polygon seams.
    float pixel = max(fwidth(distance), 0.001);
    float coverage = smoothstep(-pixel, pixel, outerDistance);
    if (!circle) coverage *= smoothstep(-pixel, pixel, sideDistance);
    if (sectorShape.y > 0.001)
        coverage *= smoothstep(-pixel, pixel, innerDistance);
    if (coverage <= 0.001) discard;

    float edge = 1.0 - smoothstep(0.0, sectorShape.w, outerDistance);
    if (!circle)
        edge = max(edge, 1.0 - smoothstep(0.0, sectorShape.w, sideDistance));
    if (sectorShape.y > 0.001)
        edge = max(edge, 1.0 - smoothstep(0.0, sectorShape.w, innerDistance));
    finalColor = vec4(fragColor.rgb * colDiffuse.rgb,
        fragColor.a * colDiffuse.a * coverage * (0.012 + edge * 0.58));
}
