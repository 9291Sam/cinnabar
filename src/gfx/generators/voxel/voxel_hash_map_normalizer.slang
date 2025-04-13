
#include "shared_data_structures.slang"
#include "voxel_faces.slang"

[shader("compute")]
[numthreads(1024, 1, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    const uint thisSlot = threadId.x;

    const uint NormalizedSampleLength = 1024;

    GpuColorHashMapNode node = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot];

    if (node.key != ~0u)
    {
        const float3 colors1024 = float3(node.r_1024, node.g_1024, node.b_1024);
        const float samples = node.samples;

        const float3 normalizedColors = colors1024 * (NormalizedSampleLength / node.samples);

        node.r_1024 = uint(normalizedColors.x);
        node.g_1024 = uint(normalizedColors.y);
        node.b_1024 = uint(normalizedColors.z);
        node.samples = NormalizedSampleLength;

        in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot] = node;
    }

}