

#include "util/allocators/opaque_integer_handle_allocator.hpp"
namespace gfx::renderables::voxel
{
    class VoxelRenderer
    {
    public:
        using Chunk = util::OpaqueHandle<"VoxelRenderer::Chunk", u32>;
    public:

        [[nodiscard]] Chunk createChunk();
        void                destroyChunk(Chunk);

        void updateChunk();
    };
} // namespace gfx::renderables::voxel