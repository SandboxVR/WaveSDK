#version 300 es

precision mediump float;

uniform sampler2D atexture;
uniform bool fUseInputColor;
uniform vec4 v2fColor;

in vec2 v2fCoord;

out vec4 oColor;

void main()
{
    if(fUseInputColor==true){
        oColor=v2fColor;
    }else{
        oColor = texture(atexture, v2fCoord);
    }

}
