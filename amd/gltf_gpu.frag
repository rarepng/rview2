#version 460
//#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 newcolor;
layout (location = 3) flat in uint txidx;

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform sampler2D tex[];

layout (std140,set=1,binding=1) uniform texidx{
	 uint texid;
};


void main() {
  
  FragColor = texture(tex[txidx], texCoord);
}
