#version 460
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 tex0;

layout (location = 0) flat out double t2;
layout (location = 1) flat out double l2;
layout (location = 2) flat out double life;
layout (location = 3) out vec2 texpos;

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
layout (std430, set = 3, binding = 0) readonly buffer lifes {
    double lifez[];
};
void main() {
  gl_Position = projection*view*transmat[gl_InstanceIndex]*vec4(aPos, 1.0);
  life = lifez[gl_InstanceIndex];
  t2=t;
  texpos=tex0;
  l2=l;
}
