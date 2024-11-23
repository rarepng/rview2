#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types:require
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in u8vec4 aJointNum;
layout (location = 4) in vec4 aJointWeight;


layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec4 newcolor;
layout (location = 3) out uint txidx;

layout (push_constant) uniform Constants {
  int aModelStride;
  uint txid;
  float t;
};

layout (set = 1, binding = 0) uniform Matrices {
    mat4 view;
    mat4 projection;
};

layout (std430, set = 2, binding = 0) readonly buffer JointMatrices {
    mat4 jointMat[];
};

void main() {
  mat4 skinMat =
    aJointWeight.x * jointMat[aJointNum.x+ gl_InstanceIndex * aModelStride ] +
    aJointWeight.y * jointMat[aJointNum.y+ gl_InstanceIndex * aModelStride ] +
    aJointWeight.z * jointMat[aJointNum.z+ gl_InstanceIndex * aModelStride ] +
    aJointWeight.w * jointMat[aJointNum.w+ gl_InstanceIndex * aModelStride ];
//  mat4 skinMat =
//    aJointWeight.x * jointMat[((aJointNum >> 24) & 0xFF) ] +
//    aJointWeight.y * jointMat[((aJointNum >> 16) & 0xFF) ] +
//    aJointWeight.z * jointMat[((aJointNum >> 8) & 0xFF) ] +
//    aJointWeight.w * jointMat[((aJointNum) & 0xFF) ];
  gl_Position = projection * view * skinMat * vec4(aPos, 1.0);
  //gl_Position = projection * view * vec4(aPos, 1.0);
  //normal = vec3(transpose(inverse(skinMat)) * vec4(aNormal, 1.0));
  normal = aNormal;
  texCoord = aTexCoord;
  newcolor = aJointNum;
  txidx=txid;
  //newcolor = vec4(((aJointNum >> 24) & 0xFF),((aJointNum >> 16) & 0xFF),((aJointNum >> 8) & 0xFF),((aJointNum) & 0xFF));
}

