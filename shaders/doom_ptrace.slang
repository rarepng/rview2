struct GridUniforms
{
    float3 gridOrigin;
    float voxelSize;
    int3 gridDimensions;
    int maxProbeCount;
};

// -- Bindings --

// 1. The Scene (Top Level Acceleration Structure)
[[vk::binding(0)]] RaytracingAccelerationStructure gSceneTLAS;

// 2. The Output Lists
// "Requests": The unique ID of the spatial hash we want to shade
[[vk::binding(1)]] RWStructuredBuffer<uint> gActiveCacheIndices;
// "Counter": An atomic counter to know where to write in the list
[[vk::binding(2)]] RWStructuredBuffer<uint> gIndirectArgs;
// "Data": The geometric info (Pos/Normal) for the hit
[[vk::binding(3)]] RWStructuredBuffer<uint4> gPackedHitData;

// 3. The Settings
[[vk::binding(4)]] ConstantBuffer<GridUniforms> gGrid;

// -- Helper: Spatial Hash Function (Gautron style) --
// Maps a 3D world position to a random-looking 1D index
uint computeSpatialHash(float3 pos, float3 normal)
{
    // Quantize position to the grid size
    int3 iPos = int3(floor(pos / gGrid.voxelSize));

    // Simple XOR hash (Production needs better hashing to avoid collisions)
    uint h = uint(iPos.x * 73856093) ^ uint(iPos.y * 19349663) ^ uint(iPos.z * 83492791);

    // Modulo by cache size (e.g., 65536)
    return h % 65536;
}

// -- Helper: Pack Data --
// Squeezes 3 floats (Pos) and 3 floats (Normal) into 4 uints (128 bits)
uint4 packHitData(float3 pos, float3 normal)
{
    // In a real engine, use half-floats (f16) to save space.
    // For this example, we just cast float bits to uints roughly.
    uint4 data;
    data.x = asuint(pos.x);
    data.y = asuint(pos.y);
    data.z = asuint(pos.z);

    // Pack normal into .w using simple 8-bit quantization
    // 0..1 range -> 0..255 integer
    uint nx = uint((normal.x * 0.5 + 0.5) * 255.0);
    uint ny = uint((normal.y * 0.5 + 0.5) * 255.0);
    uint nz = uint((normal.z * 0.5 + 0.5) * 255.0);
    data.w = (nx << 16) | (ny << 8) | nz;

    return data;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchID: SV_DispatchThreadID)
{
    uint probeIndex = dispatchID.x;
    if (probeIndex >= gGrid.maxProbeCount) return;

    // ---------------------------------------------------------
    // 1. Calculate Probe Position (1D Index -> 3D Coord)
    // ---------------------------------------------------------
    int w = gGrid.gridDimensions.x;
    int h = gGrid.gridDimensions.y;

    int z = probeIndex / (w * h);
    int y = (probeIndex % (w * h)) / w;
    int x = probeIndex % w;

    float3 probePos = gGrid.gridOrigin + (float3(x, y, z) * gGrid.voxelSize);

    // ---------------------------------------------------------
    // 2. Setup the Ray
    // ---------------------------------------------------------
    RayDesc ray;
    ray.Origin = probePos;
    // For MVP, we shoot DOWN. In production, you rotate this every frame.
    ray.Direction = float3(0, -1, 0);
    ray.TMin = 0.1;    // Don't hit yourself
    ray.TMax = 1000.0; // Max draw distance

    // ---------------------------------------------------------
    // 3. Initialize Ray Query (The "Inline" Trace)
    // ---------------------------------------------------------
    // RAY_FLAG_NONE: Standard behavior
    // 0xFF: Visibility Mask (hit everything)
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(gSceneTLAS, RAY_FLAG_NONE, 0xFF, ray);

    // ---------------------------------------------------------
    // 4. Traverse the Acceleration Structure
    // ---------------------------------------------------------
    // The GPU walks down the BVH boxes here.
    while (q.Proceed())
    {
        // If we hit an alpha-tested texture (leaves, fences),
        // we would handle candidate hits here.
        // For Opaque geometry, we just keep going.
    }

    // ---------------------------------------------------------
    // 5. Check Results
    // ---------------------------------------------------------
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        // We hit a triangle!
        float hitT = q.CommittedRayT(); // Distance to hit
        float3 hitPos = ray.Origin + (ray.Direction * hitT);

        // Note: Getting the normal in RayQuery is tricky.
        // Usually, you look up the Vertex Buffer using PrimitiveIndex().
        // For this MVP, let's assume UP normal to keep it compilable.
        float3 normal = float3(0, 1, 0);

        // Calculate the "Address" for this surface point
        uint hash = computeSpatialHash(hitPos, normal);

        // ---------------------------------------------------------
        // 6. Write to Append Buffer (Thread Safe)
        // ---------------------------------------------------------
        uint listIndex;
        // AtomicAdd returns the ORIGINAL value, effectively reserving a slot for us
        InterlockedAdd(gIndirectArgs[0], 1, listIndex);

        // Write the Request
        gActiveCacheIndices[listIndex] = hash;

        // Store the Data needed to shade it later
        gPackedHitData[hash] = packHitData(hitPos, normal);
    }
}