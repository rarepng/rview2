layout(location = 0) in float3 aPos;
layout(location = 1) in float3 aNormal;
layout(location = 2) in float2 aTexCoord;
layout(location = 3) in uint8_t4 aJointNum;
layout(location = 4) in float4 aJointWeight;

layout(location = 0) out float3 normal;
layout(location = 1) out float2 texCoord;
layout(location = 2) out float4 newcolor;
layout(location = 3) out uint32_t txidx;
layout(location = 4) out float32_t t2;
layout(location = 5) out float32_t dec;

struct consts {
	int aModelStride;
	uint txid;
	float t;
	bool decaying;
}
struct cams {
	float4x4 view;
	float4x4 projection;
}
struct jmats {
	float4x4 jointMat[];
}

[vk_push_constant]
layout(push_constant) uniform consts Constants;

layout(set = 1, binding = 0) ConstantBuffer<cams> Matrices;
// layout (set = 1, binding = 0) uniform cams Matrices;

[[vk_binding(0, 2)]]
StructuredBuffer<jmats> JointMatrices;
// layout(std430, set = 2, binding = 0) readonly buffer JointMatrices {
//     mat4 jointMat[];
// };

[shader("vertex")]
float4 main(uint vid: SV_VertexID) : SV_Position {
	float4x4 skinMat =
		aJointWeight.x * JointMatrices[0].jointMat[aJointNum.x] +
		aJointWeight.y * JointMatrices[0].jointMat[aJointNum.y] +
		aJointWeight.z * JointMatrices[0].jointMat[aJointNum.z] +
		aJointWeight.w * JointMatrices[0].jointMat[aJointNum.w];
	normal = aNormal;
	texCoord = aTexCoord;
	newcolor = aJointNum;
	txidx = Constants.txid;
	t2 = Constants.t;
	if (Constants.decaying)
		dec = 1.0;
	else
		dec = 0.0;
	return mul(mul(mul(Matrices.projection, Matrices.view), skinMat), float4(aPos, 1.0));
}

