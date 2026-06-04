#version 310 es

layout(location = 0) in vec3 v3Position;

uniform float startYPosition;
uniform float scale;

void main()
{
    gl_Position = vec4(v3Position.x, v3Position.y*scale+startYPosition, v3Position.z, 1);
}
