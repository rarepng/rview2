#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aCol;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec4 texColor;
layout (location = 1) out vec2 texCoord;

//layout (push_constant) uniform constants{
//	float xxx;
//}pushc;

layout (set = 1, binding=0)uniform mats{
	mat4 v;
	mat4 p;
};

void main(){
	gl_Position = p*v*vec4(aPos,1.0f);
	texColor = vec4(aCol,1.0);
	texCoord = aTexCoord;
}