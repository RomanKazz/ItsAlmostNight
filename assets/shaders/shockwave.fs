#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform vec3 cameraPosition;
uniform float progress;
uniform float timeSeconds;

out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float fresnel = pow(1.0 - abs(dot(normal, viewDirection)), 2.15);
    float ripple = 0.5 + 0.5*sin(
        fragWorldPosition.y*13.0 +
        fragWorldPosition.x*2.1 - fragWorldPosition.z*1.7 -
        timeSeconds*8.0);
    float movingBand = exp(-pow((ripple - 0.5)*4.2, 2.0));
    float envelope = smoothstep(0.0, 0.06, progress)*
        (1.0 - smoothstep(0.58, 1.0, progress));
    float alpha = envelope*(0.035 + fresnel*0.42 + movingBand*fresnel*0.14);
    if (alpha < 0.004) discard;
    vec3 coolBlue = vec3(0.25, 0.74, 1.0);
    vec3 pearl = vec3(0.86, 0.98, 1.0);
    vec3 color = mix(coolBlue, pearl, clamp(fresnel + movingBand*0.24, 0.0, 1.0));
    finalColor = vec4(color, alpha);
}
