#version 460
#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 newcolor;
layout (location = 3) flat in uint txidx;

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform sampler2D tex[];

layout (std140,set=1,binding=1) uniform texidx{
	 uint texid;
};

//vec3 lightPos = vec3(400.0, 300.0, 600.0);
//vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main() {
  //float lightAngle = max(dot(normalize(normal), normalize(lightPos)), 0.0);
  
  FragColor = vec4(texture(tex[txidx], texCoord).xyz,1.0f);
  // * vec4((0.3 + 0.7 * lightAngle) * lightColor, 1.0);
  //FragColor = vec4(newcolor.xyz,1.0);
  //FragColor = texture(tex, texCoord);
  //FragColor = vec4(1.0,1.0,0.0,1.0);
}
