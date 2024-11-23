#version 460 core
layout (location=0) in vec3 pos;
out vec2 xx;
uniform float time;

void main(){
	xx=pos.xy;
	gl_Position=vec4(pos.xyz,1.0);
}