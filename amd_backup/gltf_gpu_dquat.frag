#version 460 core
#extension GL_EXT_nonuniform_qualifier:enable
layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 newcolor;
layout (location = 3) flat in uint txidx;

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform sampler2D tex[];

vec3 lightPos = vec3(4.0, 3.0, 6.0);
vec3 lightColor = vec3(1.0, 1.0, 1.0);

layout (std140,set=1,binding=1) uniform texidx{
	 uint texid;
};

void main() {
  float lightAngle = max(dot(normalize(normal), normalize(lightPos)), 0.0);
  FragColor = texture(tex[txidx], texCoord) * vec4((0.3 + 0.7 * lightAngle) * lightColor, 1.0);
  //FragColor = vec4(newcolor.xyz,1.0);
}
