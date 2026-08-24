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

layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 normalAo;

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 lightDirection = normalize(-sunDirection);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float lightFacing = dot(normal, lightDirection);
    float diffuse = smoothstep(-0.34, 0.78, lightFacing);
    float upward = normal.y*0.5 + 0.5;
    float silhouette = pow(
        1.0 - max(dot(normal, viewDirection), 0.0), 2.4);
    float silverLining = silhouette*
        smoothstep(-0.18, 0.62, lightFacing);

    vec3 shadowColor = mix(
        ambientColor*0.86, vec3(0.57, 0.63, 0.67), 0.38);
    vec3 lightColor = mix(
        vec3(0.96, 0.93, 0.87), sunColor, 0.40);
    vec3 color = mix(
        shadowColor, lightColor,
        clamp(0.18 + diffuse*0.54 + upward*0.10, 0.0, 1.0));
    float lowerBody = 1.0 - smoothstep(-0.72, 0.58, fragLocalHeight);
    vec3 lowerBodyColor = mix(
        vec3(0.66, 0.70, 0.72), lightColor, 0.24);
    color = mix(
        color, lowerBodyColor,
        lowerBody*(0.28 + (1.0 - diffuse)*0.12));
    float warmSunSide = smoothstep(0.05, 0.72, lightFacing);
    vec3 warmSunColor = mix(
        sunColor, vec3(1.0, 0.86, 0.68), 0.36);
    color = mix(
        color, color*warmSunColor*1.10,
        warmSunSide*(0.10 + sunIntensity*0.035));
    color += lightColor*silverLining*
        (0.07 + sunIntensity*0.065);

    float distance = length(
        cameraPosition.xz - fragWorldPosition.xz);
    float distanceBlend = smoothstep(95.0, 215.0, distance);
    float distanceVisibility =
        1.0 - smoothstep(168.0, 218.0, distance);
    color = mix(color, ambientColor*0.78, distanceBlend*0.34);
    float facing = abs(dot(normal, viewDirection));
    float softEdge = mix(
        0.42, 0.84, smoothstep(0.08, 0.66, facing));
    float alpha = visibility*distanceVisibility*softEdge*
        mix(0.86, 0.62, distanceBlend);

    finalColor = vec4(color, clamp(alpha, 0.0, 0.82));
    normalAo = vec4(0.0, 0.0, 0.0, 1.0);
}
