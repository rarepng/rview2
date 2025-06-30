
// struct jmats {
// 	float4x4 jointmats;
// }

// [[vk_binding(0, 0)]]
// StructuredBuffer<jmats> jmatbuffer;
// [[vk_binding(0, 1)]]
// StructuredBuffer<uint4> jois;
// [[vk_binding(0, 2)]]
// StructuredBuffer<float4> weis;

// RWStructuredBuffer<float4> verts;

// [shader("compute")]
// [[numthreads(256, 1, 1)]]
// void cxmain(uint3 tid: SV_DispatchThreadID) {
// 	verts[tid.x] = jmatbuffer[jois[tid.x].x].jointmats * weis[tid.x].x +
// 				   jmatbuffer[jois[tid.x].y].jointmats * weis[tid.x].y +
// 				   jmatbuffer[jois[tid.x].z].jointmats * weis[tid.x].z +
// 				   jmatbuffer[jois[tid.x].w].jointmats * weis[tid.x].w;
// }
