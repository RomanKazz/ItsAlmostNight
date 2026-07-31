#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPosition;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 grassTint;
uniform vec3 cameraPosition;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform vec3 skyAmbientColor;
uniform vec3 groundAmbientColor;
uniform float ambientIntensity;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogBandsEnabled;
uniform float fogBandCount;
uniform vec3 dayNightTint;
uniform float exposure;
uniform float saturation;

out vec4 finalColor;

void main()
{
    vec4 albedo =
        texture(texture0, fragTexCoord)*colDiffuse*grassTint;
    float luminance =
        dot(albedo.rgb, vec3(0.2126, 0.7152, 0.0722));
    albedo.rgb =
        mix(vec3(luminance), albedo.rgb, 1.38);
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }
    float lightFacing =
        dot(normal, normalize(-sunDirection));
    float diffuse =
        mix(max(lightFacing, 0.0),
            smoothstep(-0.08, 0.82, lightFacing), 0.42);
    float hemisphere = normal.y*0.5 + 0.5;
    vec3 ambient =
        mix(groundAmbientColor, skyAmbientColor, hemisphere)*
        ambientIntensity*mix(0.9, 1.08, hemisphere);
    vec3 direct = sunColor*sunIntensity*diffuse;
    vec3 litColor = albedo.rgb*(ambient + direct)*dayNightTint;

    float horizontalDistance =
        length(cameraPosition.xz - fragWorldPosition.xz);
    float fogRange = max(fogEnd - fogStart, 0.01);
    float fogDistance =
        clamp((horizontalDistance - fogStart)/fogRange, 0.0, 1.0);
    float fogAmount =
        (1.0 - exp(-2.5*fogDistance*fogDistance))*0.96;
    if (fogBandsEnabled > 0.5)
    {
        float bands = max(round(fogBandCount), 2.0);
        fogAmount = floor(fogAmount*(bands - 1.0) + 0.5)/
            (bands - 1.0);
    }
    vec3 atmosphereColor =
        mix(fogColor, skyAmbientColor, 0.14 + fogDistance*0.08);
    litColor = mix(litColor, atmosphereColor, fogAmount);
    litColor *= exposure;
    float litLuminance =
        dot(litColor, vec3(0.2126, 0.7152, 0.0722));
    litColor = mix(vec3(litLuminance), litColor, saturation);
    finalColor = vec4(litColor, albedo.a);
}
