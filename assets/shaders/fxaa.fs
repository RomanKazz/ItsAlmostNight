#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

out vec4 finalColor;

float luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 texel = 1.0/vec2(textureSize(texture0, 0));
    vec3 center = texture(texture0, fragTexCoord).rgb;
    vec3 northWest = texture(
        texture0, fragTexCoord + vec2(-1.0, -1.0)*texel).rgb;
    vec3 northEast = texture(
        texture0, fragTexCoord + vec2( 1.0, -1.0)*texel).rgb;
    vec3 southWest = texture(
        texture0, fragTexCoord + vec2(-1.0,  1.0)*texel).rgb;
    vec3 southEast = texture(
        texture0, fragTexCoord + vec2( 1.0,  1.0)*texel).rgb;

    float centerLuma = luma(center);
    float nwLuma = luma(northWest);
    float neLuma = luma(northEast);
    float swLuma = luma(southWest);
    float seLuma = luma(southEast);
    float minimumLuma = min(
        centerLuma, min(min(nwLuma, neLuma), min(swLuma, seLuma)));
    float maximumLuma = max(
        centerLuma, max(max(nwLuma, neLuma), max(swLuma, seLuma)));
    float contrast = maximumLuma - minimumLuma;
    float threshold = max(0.030, maximumLuma*0.105);
    if (contrast < threshold)
    {
        finalColor = vec4(center, 1.0)*fragColor;
        return;
    }

    vec2 direction;
    direction.x = -((nwLuma + neLuma) - (swLuma + seLuma));
    direction.y =  ((nwLuma + swLuma) - (neLuma + seLuma));
    float directionReduce = max(
        (nwLuma + neLuma + swLuma + seLuma)*0.03125,
        1.0/128.0);
    float reciprocalMinimum = 1.0/
        (min(abs(direction.x), abs(direction.y)) + directionReduce);
    direction = clamp(
        direction*reciprocalMinimum,
        vec2(-7.0), vec2(7.0))*texel;

    vec3 resultA = 0.5*(
        texture(texture0,
            fragTexCoord + direction*(1.0/3.0 - 0.5)).rgb +
        texture(texture0,
            fragTexCoord + direction*(2.0/3.0 - 0.5)).rgb);
    vec3 resultB = resultA*0.5 + 0.25*(
        texture(texture0,
            fragTexCoord + direction*-0.5).rgb +
        texture(texture0,
            fragTexCoord + direction*0.5).rgb);
    float resultLuma = luma(resultB);
    vec3 result = resultLuma < minimumLuma || resultLuma > maximumLuma
        ? resultA
        : resultB;
    finalColor = vec4(result, 1.0)*fragColor;
}
