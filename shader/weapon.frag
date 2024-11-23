#version 460 core
in vec2 xx;
out vec4 f;
uniform float time;
vec3 rotz(vec3 p,float a){
    mat3 rotm = mat3(cos(a),-sin(a),0.0,sin(a),cos(a),0.0,0.0,0.0,1.0);
    return rotm*p;
}
vec2 rota2(vec2 p,float a){
    mat2 rotm = mat2(cos(a),-sin(a),sin(a),cos(a));
    return rotm*p;
}
float map(vec3 p){
    vec3 tp = p-vec3(2.0,0.0,-2.0);
    vec3 vtp = rotz(p-vec3(2.0,0.0,0.0),1.57);
    vec3 tpp = vec3(1.5,0.4,1.0);
    float d = 1.0;
    float newtime = time * 2.2;
    int maxi = int(clamp(20.0*time+6.0,0.0,22.0));
    for(int i = 2;i<maxi;i++){
        float timecoff = 2.0*cos((pow(float(i),0.2)/(newtime+0.5)))*1.0/(float(i)/2.20+newtime);
        float horifact0 = 6.0*sin(0.2*float(i)+timecoff);
        float horifact1 = 6.6*cosh((0.2*(float(i-6))+timecoff)/1.8)/float(i);
        float horifact = mix(horifact0,horifact1,clamp(newtime/2.0,0.0,1.0));
        float vertifact0 = 6.0*sin(0.2*float(i)+timecoff);
        float vertifact1 = 6.6*cosh((0.2*(float(i-6))+timecoff)/1.8)/float(i);
        float vertifact = mix(vertifact0,vertifact1,clamp(newtime/2.0,0.0,1.0));
        float hori = length(vec2(length(tp.xz-vec2(0.0,4.2*float(i)))-tpp.x,tp.y-horifact))-tpp.y;
        float verti = length(vec2(length(vtp.xz-vec2(vertifact,4.2*float(i)))-tpp.x,vtp.y))-tpp.y;
        d = min(d,hori);
        d = min(d,verti);
    }
    return d;
}
float raymarch(vec3 ro,vec3 rd){
    float d = 0.0;
    for(int i=0;i<200;i++){
        float dt = map(ro+rd*d);
        d+=dt;
        if(d>100.0||dt<0.001)break;
    }
    return d;
}
vec3 calcnorm(vec3 p){
    float d = map(p);
    vec2 e = vec2(0.01,0.0);
    return normalize(d-vec3(map(p-e.xyy),map(p-e.yxy),map(p-e.yyx)));
}
float diffuse(vec3 p){
    vec3 pos = vec3(0.0,8.0,0.0);
    pos.xz += vec2(sin(time),cos(time))*2.0;
    vec3 n = calcnorm(p);
    vec3 l = normalize(pos-p);
    float d = raymarch(p+n*0.004,l);
    //if(d<length(pos-p)) return clamp(dot(n,l),0.0,1.0)*0.1;
    return clamp(dot(n,l),0.0,1.0);
}
void main(){
    vec3 ro = vec3(-12.0,12.0,-6.0);
    vec3 rd = normalize(vec3(xx.x,xx.y,1.0));
    float d = raymarch(ro,rd);
    vec3 col = vec3(diffuse(ro+rd*d));
    col=pow(col,vec3(0.4545));
    if(d<100.0)col+=0.1;else discard;
    f=vec4(col,1.0);
}