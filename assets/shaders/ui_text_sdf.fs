#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec2 fragStyle;
uniform sampler2D texture0;
uniform float framebufferScale;
out vec4 finalColor;
void main()
{
    float distance = texture(texture0, fragTexCoord).a;
    float distancePerFragment = max(length(vec2(
        dFdx(distance), dFdy(distance))), 0.00001);
    float antialiasWidth = distancePerFragment*fragStyle.y;
    float threshold = 0.5 - fragStyle.x*
        max(framebufferScale, 1.0)*distancePerFragment;
    float coverage = smoothstep(
        threshold - antialiasWidth, threshold + antialiasWidth, distance);
    finalColor = vec4(fragColor.rgb, fragColor.a*coverage);
}
