#version 300 es

uniform mat4 matrix;

layout(location = 0) in vec3 v3Position;
layout(location = 1) in vec3 v3Color;

flat out vec3 fiColor;

void main()
{
    gl_Position = matrix * vec4(v3Position.x, v3Position.y, v3Position.z, 1);
    fiColor=vec3(v3Color.x,v3Color.y,v3Color.z);
}