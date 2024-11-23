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
	switch(texid){
		case 0:
		vec4 ftemp=texture(image,otex);
		f=vec4(ftemp.x*colorz.x,ftemp.y*colorz.y,ftemp.z*colorz.z,1.0f);
		//f=texture(image,otex);
		break;
		case 1:
		f=texture(image2,otex);
		break;
		case 2:
		f=texture(image3,otex);
		break;
		case 3:
		f=texture(image4,otex);
		break;
	}
}