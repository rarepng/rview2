#version 460 core
out vec4 f;
in vec4 colorz;
in vec2 otex;
uniform sampler2D image;
uniform sampler2D image2;
uniform sampler2D image3;
uniform sampler2D image4;
uniform float moverx;
uniform float movery;
uniform float time;
uniform uint texid;
void main(){
	f=vec4(1.0f,0.22f,0.78f,1.0f);
}