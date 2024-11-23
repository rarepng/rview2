#version 460 core
#extension GL_KHR_vulkan_glsl:enable
layout (location = 0) in vec3 n;
layout (location = 1) in vec2 t;

layout (location = 0) out vec4 f;

layout(set=0,binding=0)uniform sampler2D texs;
vec3 lp=vec3(4.0,5.0,-3.0);
vec3 lc=vec3(1.0,1.0,1.0);
void main(){
	float la = max(dot(normalize(n),normalize(lp)),0.0);
	f=texture(texs,t)*vec4((0.3+0.7*la)*lc,1.0);
}