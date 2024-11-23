#version 460 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 nor;
layout (location = 2) in vec2 tex;

layout (location = 0) out vec3 n;
layout (location = 1) out vec2 t;

layout (set=1,binding=0) uniform mats{
	mat4 v;
	mat4 p;
};
void main(){
	gl_Position = p*v*vec4(pos,1.0);
	n=nor;
	t=tex;
}