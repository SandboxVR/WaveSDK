#version 300 es

uniform int useColor;
uniform vec4 targetColor;

in vec4 v4fColor;

out vec4 oColor;

void main() {
    if (useColor == 0) {
        oColor = v4fColor;
    } else {
        oColor = targetColor;
    }
}