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
    float broad = sin(vertexPosition.x*0.092 + vertexPosition.z*0.027 + time*1.35) +
                  sin(vertexPosition.z*0.118 - vertexPosition.x*0.021 - time*1.05);
    float detail = sin(
        vertexPosition.x*0.19 + vertexPosition.z*0.14 + time*1.9);
    float depthFade = smoothstep(0.04, 0.42, vertexTexCoord.x);
    float waveHeight = (broad*0.022 + detail*0.006)*depthFade;
    vec3 position = vertexPosition;
    position.y += waveHeight;

    fragWorldPosition = position;
    fragWaterDepth = vertexTexCoord.x;
    fragShoreDistance = vertexTexCoord.y;
    fragWaveHeight = waveHeight;
    gl_Position = mvp*vec4(position, 1.0);
}
