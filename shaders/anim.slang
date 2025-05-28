
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


[shader("compute")]
[[numthreads(256, 1, 1)]]
void cpmain(uint3 tid:SV_DispatchThreadID) {

}