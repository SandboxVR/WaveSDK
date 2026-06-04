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
    float clampResult114 = clamp((0.59 + (0.59 - (1.0 - v2fCoord2.y)) / 0.38), 0.0, 1.0);
    vec4  lerpResult100  = mix(graColorA, graColorB, clampResult114);
    float tex2DNode92    = texture(alphaTex, v2fCoord1).r;
    oColor = vec4(lerpResult100.r, lerpResult100.g, lerpResult100.b, opacity * tex2DNode92);
}