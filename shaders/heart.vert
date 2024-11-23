#version 460
layout (location = 0) in vec3 aPos;

layout (location = 0) flat out double time;
layout (location = 1) flat out double time2;
layout (location = 2) flat out double life;
layout (location = 3) out vec2 xx;

layout (push_constant) uniform Constants {
  double t;
  double t2;
  double l;
};

void main() {
  gl_Position = vec4(aPos, 1.0);
  time=t;
  time2=t2;
  life=l;
  xx=aPos.xy;
}

