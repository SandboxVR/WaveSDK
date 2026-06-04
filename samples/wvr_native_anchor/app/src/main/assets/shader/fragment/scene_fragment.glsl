#version 300 es

precision mediump float;

flat in vec3 fiColor;

out vec4 oColor;

void main()
{
    oColor = vec4(fiColor.x, fiColor.y, fiColor.z, 0.6);
}
