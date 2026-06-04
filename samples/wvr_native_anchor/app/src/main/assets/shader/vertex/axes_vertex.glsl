#version 300 es

uniform mat4 matrix;
uniform int useColor;
uniform vec4 targetColor;

layout(location = 0) in vec3 v3Position;
layout(location = 4) in vec4 v4Color;
out vec4 v4fColor;

void main() {
    v4fColor = v4Color;
    gl_Position = matrix * vec4(v3Position.xyz, 1.0f);
}
