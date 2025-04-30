
#include "shared_data_structures.slang"
#include "voxel_faces.slang"
#include "globals.slang" 

[shader("compute")]
[numthreads(1024, 1, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    const uint thisSlot = threadId.x;

    const uint NormalizedSampleLength = 1024;

    GpuColorHashMapNode node = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot];
    // node = GpuColorHashMapNode(~0u, ~0u, ~0u, ~0u, ~0u);

    const float fadeFactorDiffuse = 0.99;
    const float fadeFactorReflective = 0.4533;

    const uint samplesStatic = 256;
    const uint samplesDynamic = 64;

    const float fadeFactor = (node.isReflective() && bool(global_gpu_data[UBO_GLOBAL_DATA].bool_moved_this_frame)) ? fadeFactorReflective : fadeFactorDiffuse;
    const uint samplesForNormalization =  bool(global_gpu_data[UBO_GLOBAL_DATA].bool_moved_this_frame) ? samplesDynamic : samplesStatic;

    if (node.key != ~0u && node.getSamples() > samplesForNormalization)
    {
        float3 colors1024 = float3(node.r_1024, node.g_1024, node.b_1024);
        colors1024 *= fadeFactor;

        node.r_1024 = uint(colors1024.x);
        node.g_1024 = uint(colors1024.y);
        node.b_1024 = uint(colors1024.z);
        node.setSamplesNonAtomic(uint(float(node.getSamples()) * fadeFactor));
        
        in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot] = node;
    }


}