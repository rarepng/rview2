#version 460

layout (location = 0) flat in float t2;
layout (location = 1) in vec2 pos;

layout (location = 0) out vec4 f;

float rand(vec2 n) {
	return fract(cos(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 n) {
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
	return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

float fbm(vec2 n) {
	float total = 0.0, amplitude = 1.0;
	for (int i = 0; i < 3; i++) {
		total += noise(n) * amplitude;
		n += n;
		amplitude *= 0.5;
	}
	return total;
}

float plot(vec2 st, float pct){
  return  smoothstep( pct-0.02, pct, st.y) -
          smoothstep( pct, pct+0.02, st.y);
}

void main(){
    
    const vec4 c1 = vec4(0.002);
	const vec4 c2 = vec4(0.3);
	const vec4 c3 = vec4(0.005);
	const vec4 c4 = vec4(0.7);
	const vec4 c5 = vec4(0.004);
	const vec4 c6 = vec4(0.07);
    
	vec2 speed = vec2(0.001, 0.52);
	vec2 p = pos.xy * 0.02;
	float q = fbm(p - t2 * 0.1);
	vec2 r = vec2(fbm(p + q + t2 * speed.x - p.x - p.y), fbm(p + q - t2 * speed.y));
	vec4 c = mix(c1, c2, fbm(p + r)) + mix(c3, c4, r.x) - mix(c5, c6, r.y);
	
    c = c * vec4(1.0 - smoothstep(0.1, 0.8, pos.y));

    
    f = c/2.8;

}