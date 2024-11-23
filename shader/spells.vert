#version 460 core
layout (location=0) in vec3 pos;

uniform float time;

mat4 rotfunc(float t){
 return mat4(cos(t),-sin(t),0,0,
			sin(t),cos(t),0,0,
			0,0,1,0,
			0,0,0,1);
}

void main(){

	gl_Position=rotfunc(time)*vec4(pos*0.4f,1.0f);
}