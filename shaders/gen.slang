static const float PI = 3.14159265359;
static const float MIN_ROUGHNESS = 0.04;

layout(set = 0, binding = 0) uniform Sampler2D tex[];

//complete mess idfk

//hdri
[[vk::binding(0, 3)]] Sampler2D envMap; 

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

    float _pad1[1];
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


float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float V_SmithGGXCorrelated(float NdotL, float NdotV, float roughness) {
    float a2 = roughness * roughness;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

float3 F_Schlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float2 DirToUV(float3 dir) {
    float2 uv = float2(atan2(dir.z, dir.x), asin(dir.y));
    uv *= float2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

float3 ACESToneMapping(float3 color) {
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    return saturate((color * (A * color + B)) / (color * (C * color + D) + E));
}

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
    out.vColor = float4(1.0);//(dot(in.color, in.color) > 0.01) ? in.color : float4(1.0);

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
float4 pxmain(PXin in) : SV_Target {

    //flat backup
    // float3 N = normalize(cross(ddx(in.worldPos), ddy(in.worldPos)));
    
    float3 N = normalize(in.normal);
    float3 T = normalize(in.tangent);
    float3 B = normalize(in.bitangent);
    float3x3 TBN = float3x3(T, B, N);

    if (pc.normalIdx > 0xAAAAAAAA) {
        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm *= float3(pc.normalScale, pc.normalScale, 1.0);
        N = normalize(mul(tNorm, TBN));
    }

    float4 baseColor = pc.baseColorFactor;
    if (pc.albedoIdx != 0xAAAAAAAA) {
        float4 texColor = tex[pc.albedoIdx].Sample(in.uv);
        texColor.rgb = pow(texColor.rgb, 2.2);
        baseColor *= texColor;
    }
    baseColor *= in.vColor;
    if (baseColor.a < 0.01) discard;


    float occlusion = 1.0;
    float roughness = 1.0;
    float metallic = 0.0;

    roughness *= pc.roughnessFactor;
    metallic *= pc.metallicFactor;

    if (pc.ormIdx != 0xFFFFFFFF) {
        float4 orm = tex[pc.ormIdx].Sample(in.uv);
        occlusion = orm.r;
        roughness = orm.g * pc.roughnessFactor;
        metallic = orm.b * pc.metallicFactor;
    } else {
        // i wanted to implement flat shading here
    }
    roughness = clamp(roughness, MIN_ROUGHNESS, 1.0);

    float3 f0 = float3(0.04); 
    float3 diffuseColor = baseColor.rgb * (1.0 - metallic);
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);
    float alphaRoughness = roughness * roughness;

    float3 V = normalize(cammats.camPos - in.worldPos);
    float3 Lo = float3(0.0);

    float3 lightDir = normalize(float3(0.5, 1.0, 0.5)); 
    float3 lightColor = float3(3.0); 
    
    float3 L = lightDir;
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(abs(dot(N, V)), 0.001); 
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0) {
        float D = D_GGX(NdotH, alphaRoughness);
        float Vis = V_SmithGGXCorrelated(NdotL, NdotV, alphaRoughness);
        float3 F = F_Schlick(VdotH, specularColor);
        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);
        float3 diffuse = kD * diffuseColor / PI;
        float3 specular = (D * Vis) * F; 
        Lo += (diffuse + specular) * lightColor * NdotL;
    }

    float3 diffuseIBL = envMap.SampleLevel(DirToUV(N), 5.0).rgb * diffuseColor;

    float3 R = reflect(-V, N);
    float lod = roughness * 10.0; 
    float3 prefilteredColor = envMap.SampleLevel(DirToUV(R), lod).rgb;
    
    float3 F_IBL = F_Schlick(NdotV, specularColor); 
    float3 specularIBL = prefilteredColor * F_IBL;

    float3 ambient = (diffuseIBL + specularIBL) * occlusion;

    float3 finalColor = ambient + Lo;

    finalColor = ACESToneMapping(finalColor);

    // i forgot what this was for
    finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, baseColor.a);
}