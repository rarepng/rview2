static const float PI = 3.14159265359;
static const float MIN_ROUGHNESS = 0.04;



//complete mess idfk

[[vk::binding(0, 0)]] Sampler2D tex[];
[[vk::binding(1, 0)]] ConstantBuffer<cams> cammats;
[[vk::binding(2, 0)]] StructuredBuffer<DualQuatScale> globalJoints[];
[[vk::binding(3, 0)]] Sampler2D envMap;
[[vk::binding(4, 0)]] StructuredBuffer<MaterialData> materials;
[[vk::binding(5, 0)]] ByteAddressBuffer vertexBuffers[];
[[vk::binding(7, 0)]] StructuredBuffer<GPUInstanceData> instanceData[];
[[vk::binding(8, 0)]] RWStructuredBuffer<VkDrawIndexedIndirectCommand> indirectIDXCommands[];

[[vk::binding(12, 0)]] ByteAddressBuffer rawIndexBuffer;
[[vk::binding(13, 0)]] StructuredBuffer<PrimitiveMetadata> primitiveRegistry;
[[vk::binding(14, 0)]] StructuredBuffer<ModelMetadata> modelRegistry;
[[vk::binding(15, 0)]] RWStructuredBuffer<VkDrawIndirectCommand> indirectCommands[];
[[vk::binding(16, 0)]] RWStructuredBuffer<uint> indirectDrawCount[];
[[vk::binding(17, 0)]] ByteAddressBuffer morphDeltas;
[[vk::binding(18, 0)]] StructuredBuffer<float> morphWeights[];

struct PXin {
    float4 sv_pos : SV_Position;
    float3 worldPos;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float2 uv;
    float4 vColor;
    nointerpolation uint materialID;
};

struct DualQuatScale {
  float4 real;
  float4 dual;
  float4 scale;
};

struct MaterialData {
    uint albedoIdx; uint normalIdx; uint ormIdx; uint emissiveIdx;
    uint transmissionIdx; uint sheenIdx; uint clearcoatIdx; uint thicknessIdx;
    float4 baseColorFactor;
    float3 emissiveFactor; float normalScale;
    float roughnessFactor; float metallicFactor; float transmissionFactor; float ior;
    float3 sheenColorFactor; float sheenRoughnessFactor;
    float clearcoatFactor; float clearcoatRoughnessFactor; float thicknessFactor;
    float envMapMaxLod;
};


struct consts {
    int stride;
    float t;
    uint32_t materialID;
    uint32_t modelID;
    uint32_t frameIndex;
    uint32_t posIdx;
    uint32_t normIdx;
    uint32_t uvIdx;
    uint32_t jointIdx;
    uint32_t weightIdx;
    // coloridx x_x
    uint32_t jointFmt;
    uint32_t weightFmt;
};

struct cams {
    float4x4 view;
    float4x4 projection;
    float3 camPos;
    float _pad;
};

[vk_push_constant] ConstantBuffer<consts> pc;


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
struct PrimitiveMetadata {
  uint indexCount;
  uint indexByteOffset;
  uint indexType;
  uint posIdx;
  uint normIdx;
  uint tanIdx;
  uint uvIdx;
  uint jointIdx;
  uint weightIdx;
  uint materialID;
  uint jointFmt;
  uint weightFmt;
  uint targetCount;
  uint morphDeltaIdx;
  uint morphFormat;
  uint vertexCount;
  float aabbMinX; float aabbMinY; float aabbMinZ;
  float aabbMaxX; float aabbMaxY; float aabbMaxZ;
  float3 GetMin() { return float3(aabbMinX, aabbMinY, aabbMinZ); }
  float3 GetMax() { return float3(aabbMaxX, aabbMaxY, aabbMaxZ); }
};

struct ModelMetadata {
  uint firstPrimitiveIndex;
  uint primitiveCount;
  float aabbMinX; float aabbMinY; float aabbMinZ;
  float aabbMaxX; float aabbMaxY; float aabbMaxZ;
  float3 GetMin() { return float3(aabbMinX, aabbMinY, aabbMinZ); }
  float3 GetMax() { return float3(aabbMaxX, aabbMaxY, aabbMaxZ); }
};

struct VkDrawIndirectCommand {
  uint vertexCount;
  uint instanceCount;
  uint firstVertex;
  uint firstInstance;
};

float3x3 getNormalMatrix(float4x4 worldTransform) {
  float3x3 M = (float3x3)worldTransform;
  float3 c0 = cross(M[1], M[2]);
  float3 c1 = cross(M[2], M[0]);
  float3 c2 = cross(M[0], M[1]);
  float det = dot(M[0], c0);
  return float3x3(c0, c1, c2) / det;
}

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

float3 getIBLContribution(PBRInfo pbrInputs, float3 n, float3 reflection, float envMapMaxLod) {
    float lod = (pbrInputs.perceptualRoughness * envMapMaxLod);

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
struct GPUInstanceData {
    float4x4 worldTransform;
    uint modelID;
    uint jointOffset;
    uint isSkinned;
    uint indexCount;
    uint firstIndex;
    int vertexOffset;
    uint morphWeightOffset;
};

// [[EXPERIMENTAL]]
struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

[shader("vertex")]
PXin vxmain(uint vertexID: SV_VertexID, uint id: SV_VulkanInstanceID) {
  PXin out;
  out.sv_pos = float4(0, 0, 0, 0);
  out.worldPos = float3(0);
  out.uv = float2(0);
  out.normal = float3(0, 1, 0);
  out.tangent = float3(0);
  out.bitangent = float3(0);
  out.vColor = float4(1);

  uint16_t2 unpacked = bit_cast<uint16_t2>(id);
  uint globalPrimID = (uint)unpacked.x;
  uint instanceID = (uint)unpacked.y;

  GPUInstanceData inst = instanceData[pc.frameIndex][instanceID];
  PrimitiveMetadata prim = primitiveRegistry[globalPrimID];

  if (prim.posIdx == 0xFFFFFFFF) return out;

  uint vertexIndex = vertexID;

  if (prim.indexType < 3) {
    uint byteOffset = prim.indexByteOffset;
    if (prim.indexType == 0) {
      uint offset = byteOffset + vertexID;
      uint word = rawIndexBuffer.Load(offset & ~3);
      vertexIndex = (word >> ((offset & 3) * 8)) & 0xFF;
    } else if (prim.indexType == 1) {
      uint offset = byteOffset + (vertexID * 2);
      uint word = rawIndexBuffer.Load(offset & ~3);
      vertexIndex = (word >> ((offset & 2) * 8)) & 0xFFFF;
    } else if (prim.indexType == 2) {
      uint offset = byteOffset + (vertexID * 4);
      uint alignedOffset = offset & ~3;
      uint word0 = rawIndexBuffer.Load(alignedOffset);

      if ((offset & 3) == 0) {
        vertexIndex = word0;
      } else {
        uint word1 = rawIndexBuffer.Load(alignedOffset + 4);
        uint shift = (offset & 3) * 8;
        vertexIndex = (word0 >> shift) | (word1 << (32 - shift));
      }
    }
  }

  uint posSize = 0;
  vertexBuffers[prim.posIdx].GetDimensions(posSize);
  if ((vertexIndex * 12) + 12 > posSize) return out;

  float3 inPos = vertexBuffers[prim.posIdx].Load<float3>(vertexIndex * 12);

  float3 inNormal = float3(0, 1, 0);
  if (prim.normIdx < 0xFFFFFFFF) {
    inNormal = vertexBuffers[prim.normIdx].Load<float3>(vertexIndex * 12);
  }

  float2 inUv = float2(0, 0);
  if (prim.uvIdx < 0xFFFFFFFF) {
    inUv = vertexBuffers[prim.uvIdx].Load<float2>(vertexIndex * 8);
  }

  uint4 inJoint = uint4(0);
  float4 inWeight = float4(0.0);

  if (prim.jointIdx != 0xFFFFFFFF) {
    if (prim.jointFmt == 1) {
      uint2 words = vertexBuffers[prim.jointIdx].Load2(vertexIndex * 8);
      inJoint = uint4(words.x & 0xFFFF, words.x >> 16, words.y & 0xFFFF, words.y >> 16);
    } else {
      uint word = vertexBuffers[prim.jointIdx].Load(vertexIndex * 4);
      inJoint = uint4(word & 0xFF, (word >> 8) & 0xFF, (word >> 16) & 0xFF, word >> 24);
    }
  }

  if (prim.weightIdx != 0xFFFFFFFF) {
    if (prim.weightFmt == 2) {
      inWeight = vertexBuffers[prim.weightIdx].Load<float4>(vertexIndex * 16);
    } else if (prim.weightFmt == 1) {
      uint2 words = vertexBuffers[prim.weightIdx].Load2(vertexIndex * 8);
      inWeight = float4(words.x & 0xFFFF, words.x >> 16, words.y & 0xFFFF, words.y >> 16) / 65535.0;
    } else {
      uint word = vertexBuffers[prim.weightIdx].Load(vertexIndex * 4);
      inWeight = float4(word & 0xFF, (word >> 8) & 0xFF, (word >> 16) & 0xFF, word >> 24) / 255.0;
    }
  }
  if (prim.targetCount > 0 && prim.morphDeltaIdx != 0xFFFFFFFF) {
    uint bytesPerComponent = (prim.morphFormat == 2) ? 1 : ((prim.morphFormat == 1) ? 2 : 4);
    uint targetByteSize = prim.vertexCount * bytesPerComponent * 3;

    float3 morphPosDelta = float3(0.0);
    float3 morphNormDelta = float3(0.0);

    for (uint t = 0; t < prim.targetCount; t++) {
      float weight = morphWeights[pc.frameIndex][inst.morphWeightOffset + t];

      if (abs(weight) > 0.001) {
        uint targetBaseOffset = prim.morphDeltaIdx + (t * targetByteSize * 2);
        uint posOffset = targetBaseOffset + (vertexIndex * bytesPerComponent * 3);
        uint normOffset = targetBaseOffset + targetByteSize + (vertexIndex * bytesPerComponent * 3);

        if (prim.morphFormat == 2) {
          morphPosDelta += LoadInt8Vec3(posOffset) * weight;
          morphNormDelta += LoadInt8Vec3(normOffset) * weight;
        } else if (prim.morphFormat == 1) {
          morphPosDelta += LoadInt16Vec3(posOffset) * weight;
          morphNormDelta += LoadInt16Vec3(normOffset) * weight;
        } else {
          morphPosDelta += LoadFloatVec3(posOffset) * weight;
          morphNormDelta += LoadFloatVec3(normOffset) * weight;
        }
      }
    }
    inPos += morphPosDelta;
    inNormal += morphNormDelta;
  }

  float4 locPos = float4(inPos, 1.0);
  float4 worldPos;
  float3x3 normalMat = getNormalMatrix(inst.worldTransform);

  if (inst.isSkinned == 1) {
    DualQuatScale dq0 = globalJoints[pc.frameIndex][inst.jointOffset + inJoint.x];
    DualQuatScale dq1 = globalJoints[pc.frameIndex][inst.jointOffset + inJoint.y];
    DualQuatScale dq2 = globalJoints[pc.frameIndex][inst.jointOffset + inJoint.z];
    DualQuatScale dq3 = globalJoints[pc.frameIndex][inst.jointOffset + inJoint.w];

    if (dot(dq0.real, dq1.real) < 0.0) { dq1.real *= -1.0; dq1.dual *= -1.0; }
    if (dot(dq0.real, dq2.real) < 0.0) { dq2.real *= -1.0; dq2.dual *= -1.0; }
    if (dot(dq0.real, dq3.real) < 0.0) { dq3.real *= -1.0; dq3.dual *= -1.0; }

    float4 blendReal = dq0.real * inWeight.x + dq1.real * inWeight.y + dq2.real * inWeight.z + dq3.real * inWeight.w;
    float4 blendDual = dq0.dual * inWeight.x + dq1.dual * inWeight.y + dq2.dual * inWeight.z + dq3.dual * inWeight.w;
    float3 blendScale = dq0.scale.xyz * inWeight.x + dq1.scale.xyz * inWeight.y + dq2.scale.xyz * inWeight.z + dq3.scale.xyz * inWeight.w;

    float invLen = 1.0 / length(blendReal);
    blendReal *= invLen;
    blendDual *= invLen;

    float3 scaledPos = inPos * blendScale;

    float3 rCrossV = cross(blendReal.xyz, scaledPos) + blendReal.w * scaledPos;
    float3 rotatedPos = scaledPos + 2.0 * cross(blendReal.xyz, rCrossV);

    float3 translation = 2.0 * (blendReal.w * blendDual.xyz - blendDual.w * blendReal.xyz + cross(blendReal.xyz, blendDual.xyz));

    float4 modelSpacePos = float4(rotatedPos + translation, 1.0);
    worldPos = mul(inst.worldTransform, modelSpacePos);

    float3 rCrossN = cross(blendReal.xyz, inNormal) + blendReal.w * inNormal;
    float3 finalNormal = inNormal + 2.0 * cross(blendReal.xyz, rCrossN);

    inNormal = finalNormal;
  } else {
    worldPos = mul(inst.worldTransform, locPos);
  }

  out.worldPos = worldPos.xyz;
  out.sv_pos = mul(mul(cammats.projection, cammats.view), worldPos);
  out.uv = inUv;

  out.materialID = prim.materialID;
  out.normal = normalize(mul(normalMat, inNormal));

  return out;
}

[shader("pixel")]
float4 pxmain(PXin in) : SV_Target {

    uint matID = in.materialID;
    MaterialData mat = materials[matID];

    float3 N;
    if (mat.normalIdx < 0xAAAAAAAA) {
        // temporarily bypassing tangent until i unify float4 n float3 ones
        N = getNormalFromMap(in.worldPos, in.normal, in.uv, mat.normalIdx, mat.normalScale);
    } else {
        // Fallback if no normal map
        N = normalize(in.normal);
    }

    float3 V = normalize(cammats.camPos - in.worldPos);

    float perceptualRoughness = mat.roughnessFactor;
    float metallic = mat.metallicFactor;
    float3 baseColor = mat.baseColorFactor.rgb * in.vColor.rgb;
    float alpha = mat.baseColorFactor.a * in.vColor.a;

    if (mat.albedoIdx < 0xAAAAAAAA) {
        float4 texColor = tex[mat.albedoIdx].Sample(in.uv);
        baseColor *= texColor.rgb;
        alpha *= texColor.a;
    }

    if (alpha < 0.01) discard;

    float occlusion = 1.0;
    if (mat.ormIdx < 0xAAAAAAAA) {
        float4 orm = tex[mat.ormIdx].Sample(in.uv);
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

    float3 color = getIBLContribution(pbrInputs, N, reflection, mat.envMapMaxLod);

    // ig occlusion gotta be done some other way idk about
    color = lerp(color, color * occlusion, 1.0);

    float3 emissive = mat.emissiveFactor;
    if (mat.emissiveIdx < 0xAAAAAAAA) {
        emissive *= tex[mat.emissiveIdx].Sample(in.uv).rgb;
    }
    color += emissive;

    color *= 0.6;

    return float4(color, alpha);
}
bool IsVisible(float3 aabbMin, float3 aabbMax, float4x4 mvp) {
  float3 center = (aabbMin + aabbMax) * 0.5f;
  float3 extents = (aabbMax - aabbMin) * 0.5f;
  float4 clipCenter = mul(mvp, float4(center, 1.0f));
  float4x4 absMvp;
  absMvp[0] = abs(mvp[0]);
  absMvp[1] = abs(mvp[1]);
  absMvp[2] = abs(mvp[2]);
  absMvp[3] = abs(mvp[3]);
  float4 clipExtent = mul(absMvp, float4(extents, 0.0f));
  float minX = clipCenter.x - clipExtent.x;
  float maxX = clipCenter.x + clipExtent.x;
  float minY = clipCenter.y - clipExtent.y;
  float maxY = clipCenter.y + clipExtent.y;
  float minZ = clipCenter.z - clipExtent.z;
  float maxZ = clipCenter.z + clipExtent.z;
  float maxW = clipCenter.w + clipExtent.w;

  if (maxW <= 0.01f) return false;
  if (maxX < -maxW || minX > maxW) return false;
  if (maxY < -maxW || minY > maxW) return false;
  if (maxZ < 0.0f || minZ > maxW) return false;

  return true;
}

bool IsVisibleTESTING(float3 aabbMin, float3 aabbMax, float4x4 mvp) {
  float3 center = (aabbMin + aabbMax) * 0.5f;
  float3 extents = (aabbMax - aabbMin) * 0.5f;
  float4 clipCenter = mul(mvp, float4(center, 1.0f));
  float4x4 absMvp;
  absMvp[0] = abs(mvp[0]);
  absMvp[1] = abs(mvp[1]);
  absMvp[2] = abs(mvp[2]);
  absMvp[3] = abs(mvp[3]);
  float4 clipExtent = mul(absMvp, float4(extents, 0.0f));
  float minX = clipCenter.x - clipExtent.x;
  float maxX = clipCenter.x + clipExtent.x;
  float minY = clipCenter.y - clipExtent.y;
  float maxY = clipCenter.y + clipExtent.y;
  float minZ = clipCenter.z - clipExtent.z;
  float maxZ = clipCenter.z + clipExtent.z;
  float maxW = clipCenter.w + clipExtent.w;

  if (maxW <= 0.01f) return false;
  if (minZ > maxW) return false;
  if (minX > 0.0f || maxX < -maxW) return false;
  if (minY > 0.0f || maxY < -maxW) return false;

  return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cxmain(uint3 threadID: SV_DispatchThreadID) {
  uint instanceID = threadID.x;

  if (instanceID >= pc.stride) return;

  GPUInstanceData inst = instanceData[pc.frameIndex][instanceID];
  if (inst.modelID == 0xFFFFFFFF) return;

  float4x4 vp = mul(cammats.projection, cammats.view);
  float4x4 mvp = mul(vp, inst.worldTransform);

  ModelMetadata model = modelRegistry[inst.modelID];

  // if (!IsVisibleTESTING(model.GetMin(), model.GetMax(), mvp)) return;
  if (!IsVisible(model.GetMin(), model.GetMax(), mvp)) return;

  for (uint i = 0; i < model.primitiveCount; i++) {
    if (i > 100) break;

    uint globalPrimID = model.firstPrimitiveIndex + i;
    PrimitiveMetadata prim = primitiveRegistry[globalPrimID];

    float3 minB = prim.GetMin();
    float3 maxB = prim.GetMax();

    if (inst.isSkinned == 1) {
      float3 center = (minB + maxB) * 0.8f;
      float3 size = (maxB - minB) * 1.25f;
      minB = center - size;
      maxB = center + size;
    }

    // if (!IsVisibleTESTING(minB, maxB, mvp)) continue;
    bool isVisible = IsVisible(minB, maxB, mvp);

    if (isVisible) {
      uint waveCount = WaveActiveCountBits(true);
      uint waveOffset = WavePrefixCountBits(true);
      uint baseIndex = 0;

      if (waveOffset == 0) {
        InterlockedAdd(indirectDrawCount[pc.frameIndex][0], waveCount, baseIndex);
      }

      baseIndex = WaveReadLaneFirst(baseIndex);
      uint cmdIndex = baseIndex + waveOffset;

      if (cmdIndex < 100000) {
        uint packedID = (instanceID << 16) | (globalPrimID & 0xFFFF);
        VkDrawIndirectCommand cmd;
        cmd.vertexCount = prim.indexCount;
        cmd.instanceCount = 1;
        cmd.firstVertex = 0;
        cmd.firstInstance = packedID;

        indirectCommands[pc.frameIndex][cmdIndex] = cmd;
      }
    }
  }
}

// decoding helpers
float3 LoadInt8Vec3(uint offset) {
  uint aligned = offset & ~3;
  uint shift = (offset & 3) * 8;
  uint2 w = morphDeltas.Load2(aligned);

  int x, y, z;
  if (shift == 0) {
    x = int(w.x << 24) >> 24; y = int((w.x >> 8) << 24) >> 24; z = int((w.x >> 16) << 24) >> 24;
  } else if (shift == 8) {
    x = int((w.x >> 8) << 24) >> 24; y = int((w.x >> 16) << 24) >> 24; z = int((w.x >> 24) << 24) >> 24;
  } else if (shift == 16) {
    x = int((w.x >> 16) << 24) >> 24; y = int((w.x >> 24) << 24) >> 24; z = int(w.y << 24) >> 24;
  } else {
    x = int((w.x >> 24) << 24) >> 24; y = int(w.y << 24) >> 24; z = int((w.y >> 8) << 24) >> 24;
  }
  return max(float3(x, y, z) / 127.0, -1.0);
}
int Read16(uint3 w, uint byteOffset) {
  uint bitOffset = byteOffset * 8;
  uint wordIdx = bitOffset / 32;
  uint bitInWord = bitOffset % 32;

  uint val = 0;
  if (wordIdx == 0) {
    val = (bitInWord <= 16) ? ((w.x >> bitInWord) & 0xFFFF) : (((w.x >> bitInWord) | (w.y << (32 - bitInWord))) & 0xFFFF);
  } else if (wordIdx == 1) {
    val = (bitInWord <= 16) ? ((w.y >> bitInWord) & 0xFFFF) : (((w.y >> bitInWord) | (w.z << (32 - bitInWord))) & 0xFFFF);
  } else {
    val = (w.z >> bitInWord) & 0xFFFF;
  }
  return (int(val) << 16) >> 16;
}
float3 LoadInt16Vec3(uint offset) {
  uint aligned = offset & ~3;
  uint3 w = morphDeltas.Load3(aligned);
  uint localOffset = offset & 3;

  int x = Read16(w, localOffset);
  int y = Read16(w, localOffset + 2);
  int z = Read16(w, localOffset + 4);

  return max(float3(x, y, z) / 32767.0, -1.0);
}
float3 LoadFloatVec3(uint offset) {
  uint aligned = offset & ~3;
  uint localOffset = offset & 3;
  if (localOffset == 0) return asfloat(morphDeltas.Load3(aligned));

  uint4 w = morphDeltas.Load4(aligned);
  uint shift = localOffset * 8;
  uint invShift = 32 - shift;
  uint xBits = (w.x >> shift) | (w.y << invShift);
  uint yBits = (w.y >> shift) | (w.z << invShift);
  uint zBits = (w.z >> shift) | (w.w << invShift);
  return asfloat(uint3(xBits, yBits, zBits));
}
