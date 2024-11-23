#version 460
#extension GL_EXT_shader_explicit_arithmetic_types:enable
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in uvec4 aJointNum;
//layout (location = 3) in u16vec4 aJointNum;
//layout (location = 3) in uvec4 aJointNum;
layout (location = 4) in vec4 aJointWeight;


layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec4 newcolor;
layout (location = 3) out uint txidx;
layout (location = 4) flat out float t2;
layout (location = 5) flat out float dec;

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

layout (std430, set = 5, binding = 0) readonly buffer JointMatrices {
    mat4 jointMat[];
};

void main() {
  mat4 skinMat =
    aJointWeight.x * jointMat[aJointNum.x] +
    aJointWeight.y * jointMat[aJointNum.y] +
    aJointWeight.z * jointMat[aJointNum.z] +
    aJointWeight.w * jointMat[aJointNum.w];
  gl_Position = projection * view * skinMat * vec4(aPos, 1.0);
  normal = aNormal;
  texCoord = aTexCoord;
  newcolor = aJointNum;
  txidx=txid;
  t2=t;
  if(decaying)
    dec = 1.0;
  else
    dec = 0.0;
}

