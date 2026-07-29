#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform int outlineRadius;

out vec4 finalColor;

void main()
{
    float centerMask = texture(texture0, fragTexCoord).r;
    if (centerMask > 0.08)
    {
        discard;
    }

    float outsideMask = 0.0;
    const int maximumRadius = 6;
    for (int offsetY = -maximumRadius;
         offsetY <= maximumRadius; ++offsetY)
    {
        for (int offsetX = -maximumRadius;
             offsetX <= maximumRadius; ++offsetX)
        {
            if (offsetX*offsetX + offsetY*offsetY >
                outlineRadius*outlineRadius)
            {
                continue;
            }
            vec2 offset = vec2(float(offsetX), float(offsetY))*texelSize;
            vec2 sampleUv = fragTexCoord + offset;
            if (any(lessThan(sampleUv, vec2(0.0))) ||
                any(greaterThan(sampleUv, vec2(1.0))))
            {
                continue;
            }
            outsideMask = max(
                outsideMask,
                texture(texture0, sampleUv).r);
        }
    }

    float alpha = smoothstep(0.02, 0.22, outsideMask);
    finalColor = vec4(1.0, 1.0, 1.0, alpha)*fragColor;
}
