
//complete mess pt2

layout(set = 0, binding = 0) uniform Sampler2D tex[];



struct VXin {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float4 tangent : TANGENT;
    [[vk::location(3)]] float2 uv : TEXCOORD_0;
    [[vk::location(4)]] uint4 joints : JOINTS_0;
    [[vk::location(5)]] float4 weights : WEIGHTS_0;
    // will add back when i add fallback for it first
    // [[vk::location(6)]] float4 color : COLOR_0; 
};

struct PXin {
    float4 sv_pos : SV_Position;
    float3 worldPos;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv;
    float4 vColor;
};

struct PXout {
    float4 outPos : SV_Target0;
    float4 outNormal : SV_Target1;
    float4 outAlbedo : SV_Target2;
};

struct consts {
    int stride; float t;
    
    uint albedoIdx; uint normalIdx; uint ormIdx; uint emissiveIdx;
    uint transmissionIdx; uint sheenIdx; uint clearcoatIdx; uint thicknessIdx;

    float _pad0[2];

    float4 baseColorFactor;
    float3 emissiveFactor; float normalScale;
    float roughnessFactor; float metallicFactor; float transmissionFactor; float ior;
    
    float3 sheenColorFactor; float sheenRoughnessFactor;
    float clearcoatFactor; float clearcoatRoughnessFactor; float thicknessFactor;

    uint envMapMaxLod;
};

struct cams {
    float4x4 view;
    float4x4 projection;
    float3 camPos; 
    float _pad;
}

struct jmats {
    float4x4 jointmats;
}

[vk_push_constant] ConstantBuffer<consts> pc;
[[vk::binding(0, 1)]] ConstantBuffer<cams> cammats;
[[vk::binding(0, 2)]] RWStructuredBuffer<jmats> jmatbuffer;


[shader("vertex")]
PXin vxmain(VXin in, uint id: SV_VulkanInstanceID) {
    PXin out;

    float4x4 skinMat = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    if (pc.stride > 1) {
        uint offset = pc.stride * id; 
        skinMat =
            jmatbuffer[in.joints.x + offset].jointmats * in.weights.x +
            jmatbuffer[in.joints.y + offset].jointmats * in.weights.y +
            jmatbuffer[in.joints.z + offset].jointmats * in.weights.z +
            jmatbuffer[in.joints.w + offset].jointmats * in.weights.w;
    }

    float4 locPos = mul(skinMat, float4(in.pos, 1.0));
    out.worldPos = locPos.xyz;
    out.sv_pos = mul(mul(cammats.projection, cammats.view), locPos);
    out.uv = in.uv;

    //todo:fix!!
    out.vColor = float4(1.0f,1.0f,1.0f,1.0f);//(dot(in.color, in.color) > 0.01) ? in.color : float4(1.0);

    float3x3 normalMat = (float3x3)skinMat; 
    float3 N = normalize(mul(normalMat, in.normal));
    float3 T = normalize(mul(normalMat, in.tangent.xyz));
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T) * in.tangent.w;

    out.normal = N;
    out.tangent = T;
    out.bitangent = B;

    return out;
}

[shader("pixel")]
PXout pxmainmine(PXin in) : SV_Target {


    PXout out;
    
    float3 N = normalize(in.normal);
    float3 T = normalize(in.tangent);
    float3 B = normalize(in.bitangent);
    float3x3 TBN = float3x3(T, B, N);

    if (pc.normalIdx < 0xAAAAAAAA) {
        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm *= float3(pc.normalScale, pc.normalScale, 1.0);
        N = normalize(mul(tNorm, TBN));
    }

    if (pc.albedoIdx < 0xAAAAAAAA) {
        out.outAlbedo = tex[pc.albedoIdx].Sample(in.uv);
    }


    return out;
}
