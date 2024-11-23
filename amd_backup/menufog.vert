#version 460
layout (location = 0) in vec3 aPos;

layout (location = 0) flat out float t2;
layout (location = 1) out vec2 pos;

layout (push_constant) uniform Constants {
  float t;
};

void main() {
  gl_Position = vec4(aPos, 1.0);
  t2=t;
  pos=aPos.xy;
}

