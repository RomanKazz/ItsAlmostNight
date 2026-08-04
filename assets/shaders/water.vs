#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;

uniform mat4 mvp;
uniform float timeSeconds;
uniform float waveSpeed;

out vec3 fragWorldPosition;
out float fragWaterDepth;
out float fragShoreDistance;
out float fragWaveHeight;

void main()
{
    float time = timeSeconds*waveSpeed;
    float broad = sin(vertexPosition.x*0.105 + time*1.7) +
                  sin(vertexPosition.z*0.137 - time*1.25);
    float detail = sin(
        vertexPosition.x*0.061 + vertexPosition.z*0.083 + time*0.82);
    float depthFade = smoothstep(0.04, 0.42, vertexTexCoord.x);
    float waveHeight = (broad*0.018 + detail*0.012)*depthFade;
    vec3 position = vertexPosition;
    position.y += waveHeight;

    fragWorldPosition = position;
    fragWaterDepth = vertexTexCoord.x;
    fragShoreDistance = vertexTexCoord.y;
    fragWaveHeight = waveHeight;
    gl_Position = mvp*vec4(position, 1.0);
}
