#version 330

uniform vec4 maskColor;

out vec4 finalColor;

void main()
{
    // The RGB channels carry the selected object's outline color. Alpha
    // carries inverse clip depth for both selected geometry and black
    // occluders. Inverse depth gives useful precision in the close range
    // where chest loot is viewed even with an RGBA8 render target.
    float inverseDepth = clamp(gl_FragCoord.w*0.5, 1.0/255.0, 1.0);
    finalColor = vec4(maskColor.rgb, inverseDepth);
}
