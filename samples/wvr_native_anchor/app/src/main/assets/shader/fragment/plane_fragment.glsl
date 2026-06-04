#version 300 es

precision mediump float;

uniform vec4 v2fColor;
out vec4 oColor;

void main()
{
    oColor=v2fColor;
}
