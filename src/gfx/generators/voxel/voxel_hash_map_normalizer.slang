
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
    // in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot] = GpuColorHashMapNode(~0u, ~0u, ~0u, ~0u, ~0u);

    const float fadeFactorDiffuse = 0.998;
    const float fadeFactorReflective = 0.4533;

    const uint samplesStatic = 256;
    const uint samplesDynamic = 64;

    float fadeFactor = (node.isReflective() && bool(global_gpu_data[UBO_GLOBAL_DATA].bool_moved_this_frame)) ? fadeFactorReflective : fadeFactorDiffuse;
    const uint samplesForNormalization =  bool(global_gpu_data[UBO_GLOBAL_DATA].bool_moved_this_frame) ? samplesDynamic : samplesStatic;

    if (node.key != ~0u && node.getSamples() > samplesForNormalization)
    {
        float3 colors1024 = float3(node.r_1024, node.g_1024, node.b_1024);
        float samples = float(node.getSamples());
    
        float lenBefore = length(colors1024);

        const float maxVectorLength = float(1u << 24u);
        
        // emissives blinking from overflow
        if (lenBefore > maxVectorLength)
        {
            float scale = maxVectorLength / lenBefore;
            colors1024 *= scale;
            samples *= scale;
        }
        else
        {
            colors1024 *= fadeFactor;
            samples *= fadeFactor;
        }
    
    
        node.r_1024 = uint(colors1024.x);
        node.g_1024 = uint(colors1024.y);
        node.b_1024 = uint(colors1024.z);
        node.setSamplesNonAtomic(uint(samples));
        
        in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot] = node;
    }


}