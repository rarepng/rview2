#version 460 core
in vec2 xx;
out vec4 f;
uniform float time;
float fk(float x,float y){
	return pow(max(1.0-abs(x-y),0.0),3.0)/(1.0*pow(2.0,2.0));
}
float map(vec3 p){
	vec4 s = vec4(0.0,2.0*sin(time),6.0,1.0);
	float d = length(p-s.xyz)-s.w;
	//return min(p.y,d);
	return max(d,p.y)-fk(d,p.y);
}
float raymarch(vec3 ro,vec3 rd){
	float d = 0.0;
	for(int i=0;i<100;i++){
		float dtemp = map(ro+rd*d);
		d+=dtemp;
		if(d<0.01)break;
		if(d>100.0)discard;
	}
	return d;
}
vec3 calcnorm(vec3 p){
	float d = map(p);
	vec2 e = vec2(0.01,0.0);
	return normalize(d-vec3(map(p-e.xyy),map(p-e.yxy),map(p-e.yyx)));
}
float diffuse(vec3 p){
	vec3 pos=vec3(0.0,5.0,0.0);
	vec3 l = normalize(pos-p);
	vec3 n = calcnorm(p);
	return 0.5+0.5*sin(dot(n,l));
}
vec4 render(vec2 p){
	vec3 ro = vec3(0.0,2.0,0.0);
	vec3 rd = normalize(vec3(p.x,p.y,1.0));
	float d = raymarch(ro,rd);
	vec3 dd = ro+rd*d;
	float diff = diffuse(dd);
	return vec4(diff,0.0,diff,0.9);
}
void main(){
	vec2 x=xx/1080.0;
	f=render(xx*6.0);
}