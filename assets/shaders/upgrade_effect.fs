#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform vec3 effectOrigin;
uniform float effectHeight;
uniform float progress;
uniform float timeSeconds;

out vec4 finalColor;

const float Tau = 6.28318530718;

float smoothEnvelope(float value)
{
    float appear = smoothstep(0.0, 0.12, value);
    float disappear = 1.0 - smoothstep(0.62, 1.0, value);
    return appear*disappear;
}

float circularNoise(float angle, float vertical, float time)
{
    vec2 circle = vec2(cos(angle), sin(angle));
    float first = sin(
        circle.x*8.7 + circle.y*5.3 +
        vertical*17.0 - time*5.4);
    float second = sin(
        circle.x*15.1 - circle.y*11.9 -
        vertical*29.0 + time*8.1);
    float third = sin(
        circle.x*31.7 + circle.y*23.3 +
        vertical*51.0 - time*12.7);
    return 0.5 + 0.5*
        (first*0.52 + second*0.31 + third*0.17);
}

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    if (abs(normal.y) > 0.72)
    {
        discard;
    }

    vec3 offset = fragWorldPosition - effectOrigin;
    float vertical =
        clamp(offset.y/max(effectHeight, 0.001), 0.0, 1.0);
    float angle = atan(offset.z, offset.x);
    float envelope = smoothEnvelope(progress);

    float revealHeight =
        smoothstep(0.0, 0.24, progress);
    float reveal =
        1.0 - smoothstep(
            revealHeight - 0.16,
            revealHeight + 0.025, vertical);
    float edgeFade =
        smoothstep(0.0, 0.035, vertical)*
        (1.0 - smoothstep(0.9, 1.0, vertical));

    float noise = circularNoise(
        angle, vertical, timeSeconds);
    float fineNoise = circularNoise(
        angle + 1.73, vertical*1.91,
        timeSeconds*1.37);
    noise = clamp(noise*0.72 + fineNoise*0.28, 0.0, 1.0);

    float ringPhase =
        fract(vertical*7.0 - timeSeconds*1.42);
    float rings =
        exp(-pow((ringPhase - 0.5)*8.5, 2.0));
    rings *= 0.62 + noise*0.38;

    float starRow =
        fract(vertical*11.0 - timeSeconds*1.18);
    float starVertical =
        exp(-pow((starRow - 0.5)*24.0, 2.0));
    float starAngle =
        pow(max(cos(
            angle*9.0 +
            floor(vertical*11.0)*1.91 +
            timeSeconds*2.7), 0.0), 34.0);
    float starPulse =
        0.62 + 0.38*sin(
            timeSeconds*10.0 + vertical*37.0);
    float stars = starVertical*starAngle*starPulse;

    float upwardWisps =
        smoothstep(0.42, 0.92, noise)*
        (0.45 + 0.55*sin(
            vertical*23.0 - timeSeconds*7.4 +
            angle*4.0));
    upwardWisps = max(upwardWisps, 0.0);

    float shimmer =
        0.5 + 0.5*sin(
            angle*2.0 + vertical*9.0 -
            timeSeconds*2.35 + noise*3.2);
    float broadShimmer =
        0.5 + 0.5*sin(
            vertical*4.5 + angle -
            timeSeconds*1.35);
    float body =
        0.19 + noise*0.2 +
        shimmer*0.13 + broadShimmer*0.08;
    float energy =
        body + rings*0.58 + stars*0.95 +
        upwardWisps*0.2;
    float alpha =
        envelope*reveal*edgeFade*energy;
    if (alpha < 0.006)
    {
        discard;
    }

    vec3 deepGold = vec3(1.0, 0.34, 0.025);
    vec3 gold = vec3(1.0, 0.72, 0.12);
    vec3 hotGold = vec3(1.0, 0.97, 0.7);
    vec3 color = mix(
        deepGold, gold,
        clamp(noise*0.58 + shimmer*0.42, 0.0, 1.0));
    color *= 0.84 + broadShimmer*0.28;
    color = mix(
        color, hotGold,
        clamp(rings*0.62 + stars, 0.0, 1.0));
    finalColor = vec4(color, alpha);
}
