

struct particle {
    float3 pos;
    float3 vel;
}

RWStructuredBuffer<particle> Ps;


[shader("compute")]
[[numthreads(256, 1, 1)]]
void cpmain(uint3 tid:SV_DispatchThreadID) {

}