#pragma once

#include "util/threads.hpp"
#include "util/util.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hash.hpp>
#include <vulkan/vulkan_structs.hpp>

VK_DEFINE_HANDLE(VmaAllocator)

//     struct CacheablePipelineShaderStageCreateInfo
//     {
//         vk::ShaderStageFlagBits                 stage;
//         std::shared_ptr<vk::UniqueShaderModule> shader;
//         std::string                             entry_point;

//         bool operator== (const CacheablePipelineShaderStageCreateInfo&) const = default;
//     };

//     struct CacheableGraphicsPipelineCreateInfo
//     {
//         std::shared_ptr<vk::UniqueShaderModule> vertex_shader;
//         std::shared_ptr<vk::UniqueShaderModule> fragment_shader;
//         vk::PrimitiveTopology                   topology;
//         bool                                    discard_enable;
//         vk::PolygonMode                         polygon_mode;
//         vk::CullModeFlags                       cull_mode;
//         vk::FrontFace                           front_face;
//         bool                                    depth_test_enable;
//         bool                                    depth_write_enable;
//         vk::CompareOp                           depth_compare_op;
//         vk::Format                              color_format;
//         vk::Format                              depth_format;
//         bool                                    blend_enable;
//         std::string                             name;

//         bool operator== (const CacheableGraphicsPipelineCreateInfo&) const = default;
//     };

//     // struct CacheableComputePipelineCreateInfo
//     // {
//     //     std::string                               entry_point;
//     //     std::shared_ptr<vk::UniqueShaderModule>   shader;
//     //     std::shared_ptr<vk::UniquePipelineLayout> layout;
//     //     std::string                               name;

//     //     bool operator== (const CacheableComputePipelineCreateInfo&) const = default;
//     // };
// } // namespace gfx::render::vulkan

// template<>
// struct std::hash<gfx::render::vulkan::CacheablePipelineShaderStageCreateInfo>
// {
//     std::size_t operator() (const gfx::render::vulkan::CacheablePipelineShaderStageCreateInfo& i) const noexcept
//     {
//         std::size_t result = 5783547893548971;

//         util::hashCombine(result, std::hash<std::string> {}(i.entry_point));

//         util::hashCombine(result, static_cast<std::size_t>(std::bit_cast<u64>(**i.shader)));

//         util::hashCombine(result, std::hash<vk::ShaderStageFlagBits> {}(i.stage));

//         return result;
//     }
// };

// template<>
// struct std::hash<gfx::render::vulkan::CacheableGraphicsPipelineCreateInfo>
// {
//     std::size_t operator() (const gfx::render::vulkan::CacheableGraphicsPipelineCreateInfo& i) const noexcept
//     {
//         std::size_t result = 5783547893548971;

//         for (const gfx::render::vulkan::CacheablePipelineShaderStageCreateInfo& d : i.stages)
//         {
//             util::hashCombine(result, std::hash<gfx::render::vulkan::CacheablePipelineShaderStageCreateInfo> {}(d));
//         }

//         for (const vk::VertexInputAttributeDescription& d : i.vertex_attributes)
//         {
//             util::hashCombine(result, std::hash<vk::VertexInputAttributeDescription> {}(d));
//         }

//         for (const vk::VertexInputBindingDescription& d : i.vertex_bindings)
//         {
//             util::hashCombine(result, std::hash<vk::VertexInputBindingDescription> {}(d));
//         }

//         util::hashCombine(result, std::hash<vk::PrimitiveTopology> {}(i.topology));
//         util::hashCombine(result, std::hash<bool> {}(i.discard_enable));
//         util::hashCombine(result, std::hash<vk::PolygonMode> {}(i.polygon_mode));
//         util::hashCombine(result, std::hash<vk::CullModeFlags> {}(i.cull_mode));
//         util::hashCombine(result, std::hash<vk::FrontFace> {}(i.front_face));
//         util::hashCombine(result, std::hash<bool> {}(i.depth_test_enable));
//         util::hashCombine(result, std::hash<bool> {}(i.depth_write_enable));
//         util::hashCombine(result, std::hash<vk::CompareOp> {}(i.depth_compare_op));
//         util::hashCombine(result, std::hash<vk::Format> {}(i.color_format));
//         util::hashCombine(result, std::hash<vk::Format> {}(i.depth_format));
//         util::hashCombine(result, std::hash<bool> {}(i.blend_enable));
//         util::hashCombine(result, static_cast<std::size_t>(std::bit_cast<u64>(**i.layout)));

//         util::hashCombine(result, std::hash<std::string> {}(i.name));

//         return result;
//     }
// };

// template<>
// struct std::hash<gfx::render::vulkan::CacheableComputePipelineCreateInfo>
// {
//     std::size_t operator() (const gfx::render::vulkan::CacheableComputePipelineCreateInfo& i) const noexcept
//     {
//         std::size_t result = 5783547893548971;

//         util::hashCombine(result, std::hash<std::string> {}(i.entry_point));

//         util::hashCombine(result, static_cast<std::size_t>(std::bit_cast<u64>(**i.layout)));

//         util::hashCombine(result, static_cast<std::size_t>(std::bit_cast<u64>(**i.shader)));

//         util::hashCombine(result, std::hash<std::string> {}(i.name));

//         return result;
//     }
// };

namespace gfx::render::vulkan
{
    class Instance;
    class Device;
    class DescriptorPool;

    class Allocator
    {
    public:

        Allocator(const Instance&, const Device*);
        ~Allocator();

        Allocator(const Allocator&)             = delete;
        Allocator(Allocator&&)                  = delete;
        Allocator& operator= (const Allocator&) = delete;
        Allocator& operator= (Allocator&&)      = delete;

        [[nodiscard]] VmaAllocator operator* () const;

        const Device*      getDevice() const;
        vk::DescriptorPool getRawPool() const;
        vk::PipelineCache  getRawCache() const;

    private:
        const Device* device;
        VmaAllocator  allocator;
        // vk::UniqueDescriptorPool descriptor_pool;
        // vk::UniquePipelineCache pipeline_cache;
        // util::Mutex<
        //     std::unordered_map<CacheableDescriptorSetLayoutCreateInfo,
        //     std::shared_ptr<vk::UniqueDescriptorSetLayout>>> descriptor_set_layout_cache;

        // util::Mutex<std::unordered_map<CacheablePipelineLayoutCreateInfo, std::shared_ptr<vk::UniquePipelineLayout>>>
        //     pipeline_layout_cache;

        // util::Mutex<
        //     std::unordered_map<vk::Pipeline, std::pair<std::weak_ptr<vk::UniquePipelineLayout>,
        //     vk::PipelineBindPoint>>> pipeline_layout_and_bind_lookup;

        // util::Mutex<std::unordered_map<CacheableGraphicsPipelineCreateInfo, std::shared_ptr<vk::UniquePipeline>>>
        //     graphics_pipeline_cache;

        // util::Mutex<std::unordered_map<CacheableComputePipelineCreateInfo, std::shared_ptr<vk::UniquePipeline>>>
        //     compute_pipeline_cache;

        // util::Mutex<std::unordered_map<std::string, std::shared_ptr<vk::UniqueShaderModule>>> shader_module_cache;
    };

} // namespace gfx::render::vulkan
