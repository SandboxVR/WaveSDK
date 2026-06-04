#version 300 es

uniform mat4 matrix;
uniform vec3 scale;

layout(location = 0) in vec3 v3Position;
layout(location = 1) in vec2 v2Coord;

out vec2 v2fCoord;

void main()
{
    v2fCoord = v2Coord;
    gl_Position = matrix * vec4(v3Position.x*scale.x, v3Position.y*scale.y, v3Position.z*scale.z, 1);
}
