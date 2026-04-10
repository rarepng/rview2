module animation;

struct AnimControl {
    uint animID;        // Index into the Baked Header list
    float localTime;    // Current time in seconds (CPU updates this: t += dt * speed)
    float blendWeight;  // 0.0 - 1.0 (For crossfading, requires 2 passes or a heavier shader)
    uint boneCount;     // Number of bones to process
    
    // IK / Control Features
    uint headBoneIdx;   // -1 to disable
    float3 lookAtTarget;// World space target
    float lookAtStrength;// 0.0 = off, 1.0 = full lookat
    
    uint outputOffset;  // Where to write in Set 1, Binding 2 (EntityTransforms)
    uint skeletonOffset;// Start index in SkeletonDefs
};

// The Header for a single clip in the Big Buffer
struct BakedClipHeader {
    uint startOffset;   // Index in float stream
    uint frameCount;
    float duration;
    float sampleRate;   // e.g., 30.0f
};

// Raw Bone Data (Decomposed for compression)
struct PackedTRS {
    float3 t; float pad0;
    float4 r; // Quat
    float3 s; float pad1;
};