#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;

uniform mat4 mvp;
uniform float timeSeconds;
uniform float waveSpeed;

out vec3 fragWorldPosition;
out float fragWaterDepth;
out float fragShoreDistance;

void main()
{
    float time = timeSeconds*waveSpeed;
    float broad = sin(
        vertexPosition.x*0.092 + vertexPosition.z*0.027 + time*0.12) +
        sin(vertexPosition.z*0.118 - vertexPosition.x*0.021 - time*0.09);
    float detail = sin(
        vertexPosition.x*0.19 + vertexPosition.z*0.14 + time*0.15);
    float depthFade = smoothstep(0.04, 0.42, vertexTexCoord.x);
    float waveHeight = (broad*0.004 + detail*0.0015)*depthFade;
    vec3 position = vertexPosition;
    position.y += waveHeight;

    fragWorldPosition = position;
    fragWaterDepth = vertexTexCoord.x;
    fragShoreDistance = vertexTexCoord.y;
    gl_Position = mvp*vec4(position, 1.0);
}
