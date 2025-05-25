
layout(set = 0, binding = 0) uniform Sampler2D tex[];

struct texidx {
	uint texid;
}

struct VXin {
	float3 pos:POSITION;
	float3 aNormal:NORMAL;
	float2 aTexCoord:TEXCOORD;
	uint4 aJointNum:JOINTS;
	float4 aJointWeight:WEIGHTS;
}

struct PXin {
	float3 normal;
	float2 texCoord;
	float4 newcolor;
	uint32_t txidx;
	float32_t t2;
	float32_t dec;
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

[vk_push_constant]
ConstantBuffer<consts> Constants;

[[vk_binding(0, 1)]]
ConstantBuffer<cams> cammats;

[[vk_binding(0, 2)]]
StructuredBuffer<jmats> jmatbuffer;

[[vk_binding(1, 1)]]
ConstantBuffer<texidx> texid2;

//stumped
[shader("vertex")]
VXout vxmain(VXin vxin) {
	// float4x4 skinmat = JointMatrices[0].jointMat[vxin.aJointNum.x].scale(vxin.aJointWeight.x.toFloat()).add(JointMatrices[0].jointMat[vxin.aJointNum.y].scale(vxin.aJointWeight.y.toFloat()).add(JointMatrices[0].jointMat[vxin.aJointNum.z].scale(vxin.aJointWeight.z.toFloat())).add(JointMatrices[0].jointMat[vxin.aJointNum.w].scale(vxin.aJointWeight.w.toFloat())));
	float4x4 skinmat = jmatbuffer[vxin.aJointNum.x].jointmats * vxin.aJointWeight.x +
					   jmatbuffer[vxin.aJointNum.y].jointmats * vxin.aJointWeight.y +
					   jmatbuffer[vxin.aJointNum.z].jointmats * vxin.aJointWeight.z +
					   jmatbuffer[vxin.aJointNum.w].jointmats * vxin.aJointWeight.w ;
	VXout out;
	float4x4 x = mul(cammats.projection, cammats.view);
	float4x4 y = mul(x, skinmat);
	float4 xx = float4(vxin.pos, 1.0f);
	float4 z = mul(y, xx);
	out.sv_position = z;
	out.pxin0.normal = vxin.aNormal;
	out.pxin0.texCoord = vxin.aTexCoord;
	out.pxin0.newcolor = vxin.aJointNum;
	out.pxin0.txidx = Constants.txid;
	out.pxin0.t2 = Constants.t;
	out.pxin0.dec = 0.0;
	return out;
}


[shader("pixel")]
PXout pxmain(PXin pxin) {
	PXout out;
	out.color = tex[pxin.txidx].Sample(float2(pxin.texCoord.x, pxin.texCoord.y - fract(pxin.t2 * pxin.txidx / 4.0)));

	return out;
}
