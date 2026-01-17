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

//stolen functions from doom hehe
float D_GGX_Anisotropic(float NdotH, float3 H, float3 T, float3 B, float at, float ab) {
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float a2 = at * ab;
    float3 d = float3(ab * TdotH, at * BdotH, a2 * NdotH);
    float d2 = dot(d, d);
    float b2 = a2 / d2;
    return a2 * b2 * b2 / PI;
}
float V_SmithGGXCorrelated_Anisotropic(float at, float ab, float TdotV, float BdotV, float TdotL, float BdotL, float NdotV, float NdotL) {
    float lambdaV = NdotL * length(float3(at * TdotV, ab * BdotV, NdotV));
    float lambdaL = NdotV * length(float3(at * TdotL, ab * BdotL, NdotL));
    return 0.5 / (lambdaV + lambdaL);
}
float2 EnvBRDFApprox(float Roughness, float NoV) {
    const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
    float4 r = Roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
    return AB;
}
float3 ACES_Fitted(float3 color) {
    const float3x3 ACESInputMat = float3x3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    );
    color = mul(ACESInputMat, color);
    float3 a = color * (color + 0.0245786) - 0.000090537;
    float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    return saturate(a / b);
}
float3 ACESFilm(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}
float3 LinearToSRGB(float3 color) {
    return pow(color, 1.0 / 2.2);
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
float4 pxmainmine(PXin in) : SV_Target {

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
        // texColor.rgb = pow(texColor.rgb, 2.2);
        baseColor *= texColor;
    }
    baseColor *= in.vColor;
    if (baseColor.a < 0.01) discard;

    float occlusion = 1.0;
    // float roughness = 1.0;
    // float metallic = 0.0;

    float roughness = pc.roughnessFactor;
    float metallic = pc.metallicFactor;

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
    float NdotV = max(dot(N, V), 0.0001);

    float diffuseIntensity = 0.5;
    float3 diffuseIBL = envMap.SampleLevel(DirToUV(N), 5.0).rgb * diffuseColor * diffuseIntensity;

    float3 R = reflect(-V, N);

    float lod = roughness * pc.envMapMaxLod;

    float3 prefilteredColor = envMap.SampleLevel(DirToUV(R), lod).rgb;

    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 specularIBL = prefilteredColor * (specularColor * envBRDF.x + envBRDF.y);

    float3 ambient = (diffuseIBL + specularIBL) * occlusion;

    float3 finalColor = ambient;

    return float4(finalColor, baseColor.a);
}

// i forgot what this was for
// finalColor = ACESToneMapping(finalColor);
// finalColor = pow(finalColor, 1.0 / 2.2);

// UNCOMMENT ONE LINE AT A TIME TO DEBUG
// return baseColor;
// return float4(N * 0.5 + 0.5, 1.0);
// return float4(roughness, roughness, roughness, 1.0);
// return float4(diffuseIBL, 1.0);
// return float4(specularIBL, 1.0);
// float3 debugMips = envMap.SampleLevel(in.uv, pc.envMapMaxLod).rgb;
// debugMips = debugMips / (debugMips + 1.0);
// return float4(debugMips, 1.0);

[shader("pixel")]
float4 pxmainllm(PXin in) : SV_Target {
    float3 N = normalize(in.normal);
    float3 V = normalize(cammats.camPos - in.worldPos);

    if (pc.normalIdx > 0xAAAAAAAA) {
        float3 T = normalize(in.tangent);
        float3 B = normalize(in.bitangent);
        float3x3 TBN = float3x3(T, B, N);

        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm.xy *= pc.normalScale;
        N = normalize(mul(tNorm, TBN));
    }

    float NdotV = max(dot(N, V), 1e-4);

    float4 albedo = pc.baseColorFactor;
    float roughness = pc.roughnessFactor;
    float metallic = pc.metallicFactor;
    float occlusion = 1.0;

    // Sample Textures
    if (pc.albedoIdx != 0xAAAAAAAA) {
        float4 texColor = tex[pc.albedoIdx].Sample(in.uv);
        albedo *= texColor;
    }

    if (pc.ormIdx != 0xFFFFFFFF) {
        float4 orm = tex[pc.ormIdx].Sample(in.uv);
        occlusion = orm.r;
        roughness *= orm.g;
        metallic *= orm.b;
    }

    roughness = clamp(roughness, 0.04, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    float3 f0 = float3(0.04);
    float3 diffuseColor = albedo.rgb * (1.0 - metallic);
    float3 specularColor = lerp(f0, albedo.rgb, metallic);
    float alpha = roughness * roughness;

    float3 color = float3(0.0);

    float3 R = reflect(-V, N);

    float maxReflectionLod = 8.0;
    float lod = roughness * maxReflectionLod;
    float3 prefilteredColor = envMap.SampleLevel(DirToUV(R), lod).rgb;

    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 specularIBL = prefilteredColor * (specularColor * envBRDF.x + envBRDF.y);

    float diffuseLod = 2.0;
    float3 irradiance = envMap.SampleLevel(DirToUV(N), diffuseLod).rgb;
    float3 diffuseIBL = irradiance * diffuseColor;

    float aperture = 2.0 * alpha * alpha;
    float microShadow = saturate(NdotV + aperture) - aperture;
    // specularIBL *= microShadow;
    diffuseIBL *= microShadow;

    color += (diffuseIBL + specularIBL) * occlusion;

    color = ACES_Fitted(color);

    // Gamma Correction
    // ONLY do this if your Swapchain is UNORM. If it is SRGB, delete this line.
    // color = pow(color, 1.0 / 2.2);
    // return float4(in.normal * 0.5 + 0.5, 1.0);
    return float4(color, albedo.a);
}
[shader("pixel")]
float4 pxmaingem(PXin in) : SV_Target {
    float3 N = normalize(in.normal);
    float3 V = normalize(cammats.camPos - in.worldPos);
    float3 R = reflect(-V, N);

    if (pc.normalIdx > 0xAAAAAAAA) {
        float3 T = normalize(in.tangent);
        float3 B = normalize(in.bitangent);
        float3x3 TBN = float3x3(T, B, N);
        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm.xy *= pc.normalScale;
        N = normalize(mul(tNorm, TBN));
    }

    float NdotV = max(dot(N, V), 1e-4);

    float4 baseColor = pc.baseColorFactor;
    if (pc.albedoIdx != 0xAAAAAAAA) {
        float4 texColor = tex[pc.albedoIdx].Sample(in.uv);
        baseColor *= texColor;
    }

    float roughness = pc.roughnessFactor;
    float metallic = pc.metallicFactor;
    float occlusion = 1.0;

    if (pc.ormIdx != 0xFFFFFFFF) {
        float4 orm = tex[pc.ormIdx].Sample(in.uv);
        occlusion = orm.r;
        roughness *= orm.g;
        metallic *= orm.b;
    }

    roughness = clamp(roughness, MIN_ROUGHNESS, 1.0);
    metallic = saturate(metallic);
    float alphaRoughness = roughness * roughness;

    float3 f0 = float3(0.04);
    float3 diffuseColor = baseColor.rgb * (1.0 - metallic);
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);


    float lod = roughness * float(pc.envMapMaxLod);
    float3 prefilteredColor = envMap.SampleLevel(DirToUV(R), lod).rgb;
    float2 envBRDF = EnvBRDFApprox(roughness, NdotV);
    float3 specularIBL = prefilteredColor * (specularColor * envBRDF.x + envBRDF.y);

    float3 irradiance = envMap.SampleLevel(DirToUV(N), float(pc.envMapMaxLod)).rgb;
    float3 diffuseIBL = irradiance * diffuseColor;

    float horizonOcclusion = 1.0 + occlusion;
    specularIBL *= occlusion;
    diffuseIBL *= occlusion;

    float3 finalColor = diffuseIBL + specularIBL;

    finalColor = ACESFilm(finalColor);

    // 5. Gamma Correction (MANDATORY if Swapchain is UNORM)
    // finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, baseColor.a);
}