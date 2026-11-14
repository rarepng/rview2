struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
};

struct PXin {
    float3 normal;
    float4 color;
}

struct PXout {
    float4 color;
}

struct VXout {
    PXin pxin;
    float4 sv_position : SV_Position;
    float psize : SV_PointSize;
}

struct consts {
    int stride;
    float t;
    uint materialID;
    uint modelID;
    uint frameIndex;
    uint posIdx;
    uint normIdx;
    uint uvIdx;
    uint jointIdx;
    uint weightIdx;
    uint jointFmt;
    uint weightFmt;
};
[vk_push_constant] ConstantBuffer<consts> pc;

[[vk::binding(10, 0)]] RWStructuredBuffer<Particle> particleBuffers[];

[shader("vertex")]
VXout vxmain(uint vertexID : SV_VertexID) {
    VXout out;
    Particle p = particleBuffers[pc.posIdx][vertexID];
    
    out.sv_position = float4(p.position, 1.0f, 1.0f);
    out.pxin.color = p.color;
    out.pxin.normal = float3(0,0,1);
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
[numthreads(256, 1, 1)]
void cxmain(uint3 tid : SV_DispatchThreadID) {
    particleBuffers[pc.posIdx][tid.x].position = particleBuffers[pc.posIdx][tid.x].position + particleBuffers[pc.posIdx][tid.x].velocity.xy * 0.1;
    particleBuffers[pc.posIdx][tid.x].velocity = particleBuffers[pc.posIdx][tid.x].velocity;
}