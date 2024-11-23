#version 460

layout (location = 0) flat in double t2;
layout (location = 1) flat in double l2;
layout (location = 2) flat in double life;
layout (location = 3) in vec2 texpos;

layout (location = 0) out vec4 f;

layout (set = 0, binding = 0) uniform sampler2D tex[];

layout (std140,set=1,binding=1) uniform texidx{
	 uint texid;
};

void main(){
	vec4 col=texture(tex[0],vec2(0.5+(texpos.x*0.5)-(life*0.5),texpos.y));
	f=vec4(col.x,col.y,col.z,0.8);
}
