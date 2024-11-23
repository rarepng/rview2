#version 460

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 newcolor;
layout (location = 3) flat in uint txidx;
layout (location = 4) flat in float t2;
layout (location = 5) flat in uint instID;


layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 0) uniform sampler2D tex[];

layout (std140,set=1,binding=1) uniform texidx{
	 uint texid;
};

float rand(vec2 n) {
	return fract(cos(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 n) {
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
	return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

void main() {
	float opa;
	float col = noise(gl_FragCoord.xy+t2*10.0)/(t2*10.0);
  switch(instID){
  case 0:
  opa = 0.8/(t2*10.0f);
	break;
  default:
	opa = 0.8/(t2*25.0f*instID);
	break;
  }
	FragColor = vec4(opa,opa,opa,col);
}
