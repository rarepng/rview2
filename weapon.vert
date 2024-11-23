#version 460 core
layout (location=0) in vec3 pos;
out vec2 xx;
uniform float time;
uniform vec3 mousepos;

void main(){
	xx=pos.xy;
	gl_Position=vec4(mousepos+(pos.xyz/2.0),1.0);
}