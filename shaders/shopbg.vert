#version 460
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 tex0;

layout (location = 0) flat out double t2;
layout (location = 1) flat out double l2;
layout (location = 2) out vec2 texpos;
layout (location = 3) flat out uint instidx;

layout (push_constant) uniform Constants {
  double t;
  double t22;
  double l;
};
layout (set = 1, binding = 0) uniform Matrices {
    mat4 view;
    mat4 projection;
};
layout (std430, set = 2, binding = 0) readonly buffer transMats {
    mat4 transmat[];
};
void main() {
  gl_Position = vec4(((aPos.x/(l*1.5))+(gl_InstanceIndex)) - (1.0/l),aPos.y*0.8,0.8, 1.0);
  t2=t;
  texpos=tex0;
  l2=l;
}
