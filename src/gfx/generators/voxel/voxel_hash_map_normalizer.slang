
#include "shared_data_structures.slang"
#include "voxel_faces.slang"

[shader("compute")]
[numthreads(1024, 1, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    // const uint thisSlot = threadId.x;

    // const uint NormalizedSampleLength = 1024;

    // GpuColorHashMapNode node = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot];
    // // node = GpuColorHashMapNode(~0u, ~0u, ~0u, ~0u, ~0u);

    // const float fadeFactor = bool(global_gpu_data[UBO_GLOBAL_DATA].bool_enable_global_illumination) ? 0.75 : 0.0;

    // // TODO: increase fade on camera move

    // if (node.key != ~0u)
    // {
    //     float3 colors1024 = float3(node.r_1024, node.g_1024, node.b_1024);
    //     colors1024 *= fadeFactor;

    //     node.r_1024 = uint(colors1024.x);
    //     node.g_1024 = uint(colors1024.y);
    //     node.b_1024 = uint(colors1024.z);
    //     node.samples = uint(float(node.samples) * fadeFactor);

    //     in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot] = node;
    // }


}