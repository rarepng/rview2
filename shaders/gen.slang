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

struct PBRInfo {
    float NdotL;
    float NdotV;
    float NdotH;
    float LdotH;
    float VdotH;
    float perceptualRoughness;
    float metalness;
    float3 reflectance0;
    float3 reflectance90;
    float alphaRoughness;
    float3 diffuseColor;
    float3 specularColor;
};

float3 getNormalFromMap(float3 worldPos, float3 normal, float2 uv, uint normalIdx, float normalScale)
{
    float3 tangentNormal = tex[normalIdx].Sample(uv).rgb * 2.0 - 1.0;
    tangentNormal.xy *= normalScale;
    
    float3 Q1 = ddx(worldPos);
    float3 Q2 = ddy(worldPos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 N = normalize(normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    if (length(T) == 0) T = cross(N, float3(0, 1, 0));

    float3 B = -normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    return normalize(mul(tangentNormal, TBN));
}
float2 DirToUV(float3 dir) {
    float2 uv = float2(atan2(dir.z, dir.x), asin(dir.y));
    uv *= float2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

float2 EnvBRDFApprox(float Roughness, float NoV) {
    const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
    float4 r = Roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
    return AB;
}

float3 diffuse(PBRInfo pbrInputs) {
    return pbrInputs.diffuseColor / PI;
}

float3 specularReflection(PBRInfo pbrInputs) {
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(PBRInfo pbrInputs) {
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs) {
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

float3 getIBLContribution(PBRInfo pbrInputs, float3 n, float3 reflection) {
    float lod = (pbrInputs.perceptualRoughness * float(pc.envMapMaxLod));

    float2 brdf = EnvBRDFApprox(pbrInputs.perceptualRoughness, pbrInputs.NdotV);

    float3 diffuseLight = envMap.SampleLevel(DirToUV(n), 20.0).rgb;

    float3 specularLight = envMap.SampleLevel(DirToUV(reflection), lod).rgb;

    float3 diff = diffuseLight * pbrInputs.diffuseColor;
    float3 spec = specularLight * (pbrInputs.specularColor * brdf.x + brdf.y);

    return diff + spec;
}


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

    if (pc.normalIdx < 0xAAAAAAAA) {
        float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
        tNorm *= float3(pc.normalScale, pc.normalScale, 1.0);
        N = normalize(mul(tNorm, TBN));
    }

    float4 baseColor = pc.baseColorFactor;
    if (pc.albedoIdx < 0xAAAAAAAA) {
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

[shader("pixel")]
float4 pxpbr(PXin in) : SV_Target {

// float3 N = normalize(in.normal);
// float3 T = normalize(in.tangent);
// float3 B = normalize(in.bitangent);
// float3x3 TBN = float3x3(T, B, N);

// float3 V = normalize(cammats.camPos - in.worldPos);

// if (pc.normalIdx < 0xAAAAAAAA) {
//     float3 tNorm = tex[pc.normalIdx].Sample(in.uv).rgb * 2.0 - 1.0;
//     tNorm *= float3(pc.normalScale, pc.normalScale, 1.0);
//     N = normalize(mul(tNorm, TBN)); // Transform to world space
//     // N.y *= -1.0; // GLSL sometimes flips Y, adjust if normal map looks inverted
// }

float3 N;
if (pc.normalIdx < 0xAAAAAAAA) {
    // temporarily bypassing tangent until i unify float4 n float3 ones
    N = getNormalFromMap(in.worldPos, in.normal, in.uv, pc.normalIdx, pc.normalScale);
} else {
    // Fallback if no normal map
    N = normalize(in.normal);
}

float3 V = normalize(cammats.camPos - in.worldPos);

float perceptualRoughness = pc.roughnessFactor;
float metallic = pc.metallicFactor;
float3 baseColor = pc.baseColorFactor.rgb * in.vColor.rgb;
float alpha = pc.baseColorFactor.a * in.vColor.a;

if (pc.albedoIdx < 0xAAAAAAAA) {
    float4 texColor = tex[pc.albedoIdx].Sample(in.uv);
    baseColor *= texColor.rgb;
    alpha *= texColor.a;
}

if (alpha < 0.01) discard;

float occlusion = 1.0;
if (pc.ormIdx < 0xAAAAAAAA) {
    float4 orm = tex[pc.ormIdx].Sample(in.uv);
    occlusion = orm.r;
    if (occlusion < 0.01) occlusion = 1.0;
    perceptualRoughness = orm.g * perceptualRoughness;
    metallic = orm.b * metallic;
}

perceptualRoughness = clamp(perceptualRoughness, MIN_ROUGHNESS, 1.0);
metallic = clamp(metallic, 0.0, 1.0);
float alphaRoughness = perceptualRoughness * perceptualRoughness;

float3 f0 = float3(0.04);
float3 diffuseColor = baseColor * (float3(1.0) - f0);
diffuseColor *= 1.0 - metallic;

float3 specularColor = lerp(f0, baseColor, metallic);

float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
float3 specularEnvironmentR0 = specularColor;
float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

float3 L = float3(0, 1, 0);
float3 H = normalize(L + V);
float3 reflection = normalize(reflect(-V, N));

float NdotL = clamp(dot(N, L), 0.001, 1.0);
float NdotV = clamp(abs(dot(N, V)), 0.001, 1.0);
float NdotH = clamp(dot(N, H), 0.0, 1.0);
float LdotH = clamp(dot(L, H), 0.0, 1.0);
float VdotH = clamp(dot(V, H), 0.0, 1.0);

PBRInfo pbrInputs;
pbrInputs.NdotL = NdotL;
pbrInputs.NdotV = NdotV;
pbrInputs.NdotH = NdotH;
pbrInputs.LdotH = LdotH;
pbrInputs.VdotH = VdotH;
pbrInputs.perceptualRoughness = perceptualRoughness;
pbrInputs.metalness = metallic;
pbrInputs.reflectance0 = specularEnvironmentR0;
pbrInputs.reflectance90 = specularEnvironmentR90;
pbrInputs.alphaRoughness = alphaRoughness;
pbrInputs.diffuseColor = diffuseColor;
pbrInputs.specularColor = specularColor;


float3 color = getIBLContribution(pbrInputs, N, reflection);

// ig occlusion gotta be done some other way idk about
color = lerp(color, color *occlusion, 1.0);

float3 emissive = pc.emissiveFactor;
if (pc.emissiveIdx < 0xAAAAAAAA) {
    emissive *= tex[pc.emissiveIdx].Sample(in.uv).rgb;
}
color += emissive;

color *= 0.6;

return float4(color, alpha);
}