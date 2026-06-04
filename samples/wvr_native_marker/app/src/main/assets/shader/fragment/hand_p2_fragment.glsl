#version 300 es

precision mediump float;

uniform vec4 graColorA;
uniform vec4 graColorB;
uniform float opacity;
uniform sampler2D alphaTex;

in vec2 v2fCoord1;
in vec2 v2fCoord2;

out vec4 oColor; 

void main()
{
    float smoothStepResult99 = smoothstep(0.7, 0.38, 1.0 - v2fCoord2.y);
    vec4 lerpResult100 = mix(graColorA, graColorB, smoothStepResult99);
    float color104 = 0.0;
    float color102 = 1.0;
    float smoothstepResult103 = smoothstep(-0.05, 1.0, 1.0 - v2fCoord2.y);
    float lerpResult105 = mix(color104, color102, smoothstepResult103);
    float tex2DNode92 = texture(alphaTex, v2fCoord1).r;
    oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, lerpResult105 * opacity * tex2DNode92);
}