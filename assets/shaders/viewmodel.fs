#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragVertexColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 skyAmbientColor;
uniform vec3 groundAmbientColor;
uniform float ambientIntensity;
uniform vec3 dayNightTint;
uniform float exposure;

out vec4 finalColor;

const vec3 Luma = vec3(0.2126, 0.7152, 0.0722);

vec3 normalizedTint(vec3 color)
{
    float luminance = dot(color, Luma);
    return clamp(color/max(luminance, 0.001), vec3(0.72), vec3(1.24));
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord)*
        colDiffuse*fragVertexColor;
    if (albedo.a <= 0.001)
    {
        discard;
    }

    vec3 normal = normalize(fragNormal);
    float hemisphere = normal.y*0.5 + 0.5;
    float lightFacing = dot(normal, normalize(-sunDirection));
    // A broad transition preserves the painted low-poly facets without the
    // harsh binary split of a conventional toon ramp.
    float stylizedDiffuse = smoothstep(-0.08, 0.72, lightFacing);

    float skyAmount = mix(0.35, 1.0, hemisphere);
    vec3 ambientSource = mix(
        groundAmbientColor, skyAmbientColor, skyAmount);
    vec3 ambientTint = mix(
        vec3(1.0), normalizedTint(ambientSource), 0.38);
    vec3 directTint = mix(
        vec3(1.0), normalizedTint(sunColor), 0.48);

    float ambientScale = clamp(
        ambientIntensity/0.50, 0.75, 1.20);
    float sunScale = clamp(
        sunIntensity/1.27, 0.55, 1.10);
    // First-person items sit very close to the camera and must remain legible
    // inside tree/building shade. Lift only the fill; directional light still
    // provides the changing facet contrast that ties the item to the world.
    float ambientLevel = mix(0.57, 0.67, hemisphere)*ambientScale;
    float directLevel = stylizedDiffuse*sunScale*0.34;
    vec3 color = albedo.rgb*(
        ambientTint*ambientLevel + directTint*directLevel);

    float nightAmount = smoothstep(
        0.08, 0.24, dayNightTint.b - dayNightTint.r);
    color *= mix(1.0, 0.68, nightAmount);
    color *= mix(
        vec3(1.0), normalizedTint(dayNightTint), nightAmount*0.28);
    color *= clamp(exposure, 0.72, 1.28);

    finalColor = vec4(color, albedo.a);
}
