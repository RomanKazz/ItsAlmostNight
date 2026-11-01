#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform vec2 damageStrength;
out vec4 finalColor;

void main()
{
    // Rounded rectangular falloff reaches every screen edge, while leaving
    // aiming and the central view completely clear. UVs also work in HiDPI
    // screenshots and intermediate UI render targets without resolution state.
    vec2 p = abs(fragTexCoord * 2.0 - 1.0);
    float edge = pow(pow(p.x, 6.0) + pow(p.y, 6.0), 1.0 / 6.0);
    float playerAlpha = smoothstep(0.52, 1.10, edge) * damageStrength.x * 0.62;
    float coreAlpha = smoothstep(0.65, 1.10, edge) * damageStrength.y * 0.59;
    float glow = exp(-pow((edge - 0.96) / 0.17, 2.0));
    playerAlpha += glow * damageStrength.x * 0.045;
    coreAlpha += glow * damageStrength.y * 0.045;
    float alpha = playerAlpha + coreAlpha * (1.0 - playerAlpha);
    vec3 color = (vec3(0.94, 0.065, 0.10) * playerAlpha +
                  vec3(0.64, 0.15, 0.97) * coreAlpha * (1.0 - playerAlpha)) / max(alpha, 0.0001);
    finalColor = vec4(color, min(alpha, 0.72)) * fragColor;
}
