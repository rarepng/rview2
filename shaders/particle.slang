
struct Particle {
	float2 position;
	float2 velocity;
	float4 color;
};

struct VXin {
	float2 pos : POSITION;
	float4 color;
}
struct PXin {
    float3 normal;
	float4 color;
}
struct CXin {
	uint3 tid : SV_DispatchThreadID;
	Particle p;

}
struct PXout {
	float4 color;
}

struct VXout {
	PXin pxin;
    float4 sv_position : SV_Position;
    float psize : SV_PointSize;
}


StructuredBuffer<Particle> pin;
RWStructuredBuffer<Particle> pout;

[shader("vertex")]
VXout vxmain(VXin vxin) {
	VXout out;
	out.sv_position = float4(vxin.pos, 1.0f, 1.0f);
	out.pxin.color = vxin.color;
	out.psize = 14.0f;
	return out;
}

[shader("pixel")]
PXout pxmain(PXin pxin, float2 coord: SV_PointCoord) {
	PXout out;
	out.color = float4(pxin.color.xyz, 0.5 - length(coord - float2(0.5)));
	return out;
}

[shader("compute")]
[[numthreads(256, 1, 1)]]
void cxmain(uint3 tid : SV_DispatchThreadID) {
    pout[tid.x].position = pin[tid.x].position + pin[tid.x].velocity.xy * 0.1;
    pout[tid.x].velocity = pin[tid.x].velocity;
}