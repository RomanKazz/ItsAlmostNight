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

out vec4 finalColor;

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
        vec3(0.72, 0.77, 0.82), ambientColor, 0.20);
    vec3 lightColor = mix(
        vec3(1.0, 0.99, 0.95), sunColor, 0.10);
    vec3 color = mix(
        shadowColor, lightColor,
        clamp(0.30 + diffuse*0.58 + upward*0.12, 0.0, 1.0));
    float lowerBody = 1.0 - smoothstep(-0.72, 0.58, fragLocalHeight);
    vec3 lowerBodyColor = vec3(0.62, 0.70, 0.77);
    color = mix(
        color, lowerBodyColor,
        lowerBody*(0.18 + (1.0 - diffuse)*0.24));
    color += lightColor*silverLining*
        (0.12 + sunIntensity*0.10);

    finalColor = vec4(color, clamp(visibility, 0.0, 1.0));
}
