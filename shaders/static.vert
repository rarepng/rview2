#version 460
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;


layout (location = 0) out vec3 normal;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec4 newcolor;
layout (location = 3) out uint txidx;

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
layout (std430, set = 2, binding = 0) readonly buffer transMats {
    mat4 transmat[];
};
void main() {
  gl_Position = projection * view * transmat[gl_InstanceIndex]* vec4(aPos, 1.0);
  //gl_Position = projection * view * vec4(aPos, 1.0);
  //normal = vec3(transpose(inverse(skinMat)) * vec4(aNormal, 1.0));
  normal = aNormal;
  texCoord = aTexCoord;
  newcolor = vec4(0.6);
  txidx=txid;
  //newcolor = vec4(((aJointNum >> 24) & 0xFF),((aJointNum >> 16) & 0xFF),((aJointNum >> 8) & 0xFF),((aJointNum) & 0xFF));
}

