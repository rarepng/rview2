
layout(set = 0, binding = 0) uniform Sampler2D tex[];

struct texidx {
	uint texid;
}

[[vk_binding(1, 1)]]
ConstantBuffer<texidx> texid2;
// layout (std140,set=1,binding=1) uniform texidx{
// 	 uint texid;
// };

[shader("pixel")]
float4 main(float3 normal: NORMAL, float2 texCoord: TEXCOORD0, float4 newcolor, uint txidx, float t2, float dec) : SV_Target0 {
	float4 col;
	col = tex[txidx].Sample(float2(texCoord.x, texCoord.y - fract(t2 * txidx / 4.0)));

	return float4(col.xyz, col.w * 2.0);
}
