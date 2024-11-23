#version 460 core
layout (location=0) out vec4 f;
layout (location=0) in vec4 colorz;
layout (location=1) in vec2 otex;
layout (binding=0) uniform sampler2D image;
layout (binding=1) uniform sampler2D image2;
layout (binding=2) uniform sampler2D image3;
layout (binding=3) uniform sampler2D image4;

struct uniformz{
	float moverx;
	float movery;
	float time;
	uint texid;
};
void main(){
	f=vec4(1.0f,0.22f,0.78f,1.0f);
}