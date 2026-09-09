#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in float fragLocalHeight;

uniform vec3 cameraPosition;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 ambientColor;
uniform float visibility;
uniform float nightAmount;

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

void main()
{
    vec3 smoothNormal = normalize(fragWorldNormal);
    vec3 facetNormal = normalize(cross(
        dFdx(fragWorldPosition), dFdy(fragWorldPosition)));
    if (dot(facetNormal, smoothNormal) < 0.0)
    {
        facetNormal = -facetNormal;
    }
    // A restrained geometric-normal contribution lets the authored planes
    // read without turning the rounded cloud meshes into hard crystals.
    vec3 normal = normalize(mix(smoothNormal, facetNormal, 0.28));
    vec3 lightDirection = normalize(-sunDirection);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float lightFacing = dot(normal, lightDirection);
    float upward = normal.y*0.5 + 0.5;
    float localHeight = smoothstep(-0.72, 0.62, fragLocalHeight);
    float tone = lightFacing*0.68 + upward*0.20 +
        (localHeight - 0.5)*0.18;

    const vec3 dayShadow = vec3(0.569, 0.667, 0.678); // #91AAAD
    const vec3 dayMid = vec3(0.737, 0.788, 0.780);    // #BCC9C7
    const vec3 dayLight = vec3(0.894, 0.894, 0.831);  // #E4E4D4
    const vec3 nightShadow = vec3(0.192, 0.275, 0.369); // #31465E
    const vec3 nightMid = vec3(0.231, 0.322, 0.408);    // #3B5268
    const vec3 nightLight = vec3(0.275, 0.369, 0.443);  // #465E71
    float night = clamp(nightAmount, 0.0, 1.0);
    vec3 shadowColor = mix(dayShadow, nightShadow, night);
    vec3 midColor = mix(dayMid, nightMid, night);
    vec3 lightColor = mix(dayLight, nightLight, night);

    float sunsetWarmth = clamp(
        (sunColor.r - sunColor.b - 0.28)*0.90, 0.0, 0.18);
    vec3 restrainedWarmLight = vec3(0.96, 0.89, 0.78);
    lightColor = mix(
        lightColor, restrainedWarmLight,
        sunsetWarmth*(0.55 + clamp(sunIntensity, 0.0, 1.0)*0.20));

    float shadowToMid = smoothstep(-0.40, 0.02, tone);
    float lightRegion = smoothstep(0.46, 0.78, tone)*
        smoothstep(-0.50, 0.18, fragLocalHeight);
    vec3 color = mix(shadowColor, midColor, shadowToMid);
    color = mix(color, lightColor, lightRegion*0.78);

    float distance = length(
        cameraPosition.xz - fragWorldPosition.xz);
    float distanceBlend = smoothstep(95.0, 215.0, distance);
    float distanceVisibility =
        1.0 - smoothstep(168.0, 218.0, distance);
    color = mix(
        color, mix(midColor, ambientColor*0.78, 0.20),
        distanceBlend*0.16);

    // Only a narrow grazing-angle fringe is softened. The main mesh body is
    // nearly opaque, removing the previous smoke-like see-through response.
    float facing = abs(dot(smoothNormal, viewDirection));
    float edgeCoverage = smoothstep(0.025, 0.17, facing);
    float edgeOpacity = mix(0.64, 1.0, edgeCoverage);
    float alpha = visibility*distanceVisibility*edgeOpacity*
        mix(1.0, 0.84, distanceBlend);

    finalColor = vec4(color, clamp(alpha, 0.0, 0.95));
    normalAo = vec4(0.0, 0.0, 0.0, 1.0);
}
