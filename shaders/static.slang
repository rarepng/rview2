
layout(set = 0, binding = 0) uniform Sampler2D tex[];

struct texidx {
	uint texid;
}

struct VXin {
	float3 pos : POSITION;
	float3 aNormal : NORMAL;
	float2 tex0 : TEXCOORD_0;
	// float2 tex1 : TEXCOORD_1;
	// float2 tex2 : TEXCOORD_2;
	// float2 tex3 : TEXCOORD_3;
	// float2 tex4 : TEXCOORD_4;
	// float2 tex5 : TEXCOORD_5;
}
struct Particle {
	float2 position;
	float2 velocity;
	float4 color;
};

StructuredBuffer<Particle> pin;
RWStructuredBuffer<Particle> pout;

struct PXin {
	float3 normal;
	float2 texCoord;
	float4 newcolor;
	uint32_t txidx;
	float32_t t2;
	float32_t dec;
}
struct CXin {
	uint3 tid : SV_DispatchThreadID;
	Particle p;

}
struct PXout {
	float4 color;
}

struct VXout {
	PXin pxin0;
	float4 sv_position : SV_Position;
}

struct consts {
	int aModelStride;
	uint txid;
	float t;
}
struct cams {
	float4x4 view;
	float4x4 projection;
}
struct jmats {
	float4x4 jointmats;
}

struct anim {
	float3 trans;
	float3 scale;
	float4 rot;
}

RWStructuredBuffer<anim> Ps;

[vk_push_constant]
ConstantBuffer<consts> Constants;

[[vk_binding(0, 1)]]
ConstantBuffer<cams> cammats;

[[vk_binding(0, 2)]]
RWStructuredBuffer<jmats> transmat;

[[vk_binding(1, 1)]]
ConstantBuffer<texidx> texid2;

//stumped
[shader("vertex")]
VXout vxmain(VXin vxin, uint id: SV_VulkanInstanceID) {
	VXout out;
	// float4x4 x = mul(cammats.projection, cammats.view);
	// float4 xx = float4(vxin.pos, 1.0f);
	// float4x4 xxx = mul(x, transmat[id].jointmats);
	// float4 xxxx = mul(xx, xxx);
	// out.sv_position = xxxx;
	float4x4 x = mul(cammats.projection, cammats.view);
	float4 xx = float4(vxin.pos, 1.0f);
	float4 xxx = mul(transmat[id].jointmats, xx);
	float4 xxxx = mul(x, xxx);
	out.sv_position = xxxx;
	out.pxin0.normal = vxin.aNormal;
	out.pxin0.texCoord = vxin.tex0;
	out.pxin0.txidx = Constants.txid;
	out.pxin0.t2 = Constants.t;
	out.pxin0.dec = 0.0;
	return out;
}


[shader("pixel")]
PXout pxmain(PXin pxin) {
	PXout out;
	out.color = tex[pxin.txidx].Sample(float2(pxin.texCoord.x, pxin.texCoord.y));

	return out;
}