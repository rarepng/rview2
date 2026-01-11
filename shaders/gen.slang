
layout(set = 0, binding = 0) uniform Sampler2D tex[];

struct texidx {
	uint texid;
}
struct VXin {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float4 tangent : TANGENT;
    [[vk::location(3)]] float2 uv : TEXCOORD_0;
    [[vk::location(4)]] uint4 joints : JOINTS_0;
    [[vk::location(5)]] float4 weights : WEIGHTS_0;
};

struct PXin {
    float4 sv_pos : SV_Position;
    float3 worldPos;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv;
};
struct Particle {
	float2 position;
	float2 velocity;
	float4 color;
};

StructuredBuffer<Particle> pin;
RWStructuredBuffer<Particle> pout;

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
    int stride; float t;
    uint albedoIdx; uint normalIdx; uint ormIdx; uint emissiveIdx;
    uint transmissionIdx; uint sheenIdx; uint clearcoatIdx; uint thicknessIdx;

	//the matching fu padding
    float _pad0[2];

    float4 baseColorFactor;
    float3 emissiveFactor; float normalScale;
    float roughnessFactor; float metallicFactor; float transmissionFactor; float ior;
    float3 sheenColorFactor; float sheenRoughnessFactor;
    float clearcoatFactor; float clearcoatRoughnessFactor; float thicknessFactor;

	//the more padding
    float _pad1;
};

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
ConstantBuffer<consts> pc;

[[vk_binding(0, 1)]]
ConstantBuffer<cams> cammats;

[[vk_binding(0, 2)]]
RWStructuredBuffer<jmats> jmatbuffer;

// stumped
[shader("vertex")]
PXin vxmain(VXin in, uint id: SV_VulkanInstanceID) {
    PXin out;

    float4x4 skinMat = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    // not sure about this chief
    if (pc.stride > 1) {
        skinMat =
            jmatbuffer[in.joints.x + pc.stride * id].jointmats * in.weights.x +
            jmatbuffer[in.joints.y + pc.stride * id].jointmats * in.weights.y +
            jmatbuffer[in.joints.z + pc.stride * id].jointmats * in.weights.z +
            jmatbuffer[in.joints.w + pc.stride * id].jointmats * in.weights.w;
    }

    // need to add instance matrix/ces
    float4 locPos = mul(skinMat, float4(in.pos, 1.0));
    out.worldPos = locPos.xyz;
    out.sv_pos = mul(mul(cammats.projection, cammats.view), locPos);
    out.uv = in.uv;

    float3x3 normalMat = (float3x3)skinMat;

    float3 inNorm = in.normal;
    float3 inTan = in.tangent.xyz;
    float hand = in.tangent.w;

    float normLenSq = dot(inNorm, inNorm);
    if (normLenSq < 0.01) {
        out.normal = float3(0, 0, 0);
    } else {
        out.normal = normalize(mul(normalMat, inNorm));
    }

    bool badTan = (pc.stride == 0) || (dot(inTan, inTan) < 0.01);

    if (badTan) {
        out.tangent = float3(0, 0, 0);
        out.bitangent = float3(0, 0, 0);
    }
    else {
        out.tangent = normalize(mul(normalMat, inTan));

        float validHand = (abs(hand) < 0.1) ? 1.0 : sign(hand);

        out.bitangent = cross(out.normal, out.tangent) * validHand;
    }

    return out;
}
[shader("pixel")]
float4 pxmain(PXin in) : SV_Target {

    float3 N = in.normal;
    float3 T = in.tangent;
    float3 B = in.bitangent;
    if (dot(N, N) < 0.001) {
        float3 dX = ddx(in.worldPos);
        float3 dY = ddy(in.worldPos);
        N = normalize(cross(dX, dY));
    }

    bool useNormalMap = (dot(T, T) > 0.1);

    float4 albedo = tex[pc.albedoIdx].Sample(in.uv) * pc.baseColorFactor;
    if (albedo.a < 0.01) discard;

    float4 orm = tex[pc.ormIdx].Sample(in.uv);
    float roughness = orm.g * pc.roughnessFactor;
    float metallic = orm.b * pc.metallicFactor;

    if (useNormalMap) {
        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm *= float3(pc.normalScale, pc.normalScale, 1.0);

        float3x3 TBN = float3x3(normalize(T), normalize(B), normalize(N));
        N = normalize(mul(tNorm, TBN));
    }
	
    float3 emissive = tex[pc.emissiveIdx].Sample(in.uv).rgb * pc.emissiveFactor;

    float3 L = normalize(float3(0.5, 1.0, 0.5));
    float NdotL = max(dot(N, L), 0.1);
    float3 diffuse = albedo.rgb * NdotL;

    return float4((albedo.rgb + emissive) * NdotL, albedo.a);
}
//first step to indirect rendering soon(tm)
struct drawcmd {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};
