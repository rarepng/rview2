#version 460
layout (location = 0) in vec3 aPos;

layout (location = 0) flat out double t2;
layout (location = 1) flat out double l2;
layout (location = 2) out vec2 pos;

layout (push_constant) uniform Constants {
  double t;
  double t22;
  double l;
};

void main() {
  gl_Position = vec4(aPos, 1.0);
  t2=t;
  pos=aPos.xy;
  l2=l;
}

