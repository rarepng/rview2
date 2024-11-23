#version 460
#extension GL_EXT_shader_explicit_arithmetic_types:enable
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in u8vec4 aJointNum;
//layout (location = 3) in u16vec4 aJointNum;
//layout (location = 3) in uvec4 aJointNum;
layout (location = 4) in vec4 aJointWeight;


layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec4 newcolor;
layout (location = 3) out uint txidx;
layout (location = 4) flat out float t2;
layout (location = 5) flat out uint instID;

layout (push_constant) uniform Constants {
  int aModelStride;
  uint txid;
  float t;
  bool decaying;
};

layout (set = 1, binding = 0) uniform Matrices {
    mat4 view;
    mat4 projection;
};

layout (std430, set = 4, binding = 0) readonly buffer JointMatrices {
    mat4 jointMat[];
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
  mat4 skinMat =
    aJointWeight.x * jointMat[aJointNum.x ] +
    aJointWeight.y * jointMat[aJointNum.y ] +
    aJointWeight.z * jointMat[aJointNum.z ] +
    aJointWeight.w * jointMat[aJointNum.w ];
    vec4 glpos;
    float newx;
  switch(gl_InstanceIndex){
  case 0:
    newx=noise(vec2(skinMat[3][0],skinMat[3][2]));
    skinMat[3][1] =max(skinMat[3][1]-(t*10.0),-100.0);
    skinMat[3][0] -=newx*t*100.0;
    skinMat[3][2] -=newx*t*100.0;
    skinMat[2][2] /=(t*1000.0);
    glpos=projection * view * skinMat * vec4(aPos.x,aPos.y,aPos.z, 1.0);
    gl_Position= vec4(glpos.x,glpos.y,glpos.z,glpos.w);
    break;
  default:
    newx=noise(vec2(skinMat[3][0],skinMat[3][2]));
    skinMat[3][1] =max(skinMat[3][1]-(t*gl_InstanceIndex*10.0),-100.0);
    skinMat[3][0] -=newx*t*100.0*(pow(-1,gl_InstanceIndex));
    skinMat[3][2] -=newx*t*100.0*(pow(-1,gl_InstanceIndex));
    skinMat[2][2] /=(t*1000.0);
    glpos=projection * view * skinMat * vec4(aPos.x,aPos.y,aPos.z, 1.0);
    newx=noise(vec2(glpos.x,glpos.y))/10.0;
    gl_Position= vec4(glpos.x,glpos.y,glpos.z,glpos.w);
    break;
  }
  normal = aNormal;
  texCoord = aTexCoord;
  newcolor = aJointNum;
  txidx=txid;
  t2=t;
  instID=gl_InstanceIndex;
}

