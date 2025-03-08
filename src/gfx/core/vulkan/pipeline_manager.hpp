#pragma once

#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/threads.hpp"
#include <filesystem>
#include <shaderc/shaderc.hpp>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    class Device;

    struct GraphicsPipelineDescriptor
    {
        std::string           vertex_shader_path;
        std::string           fragment_shader_path;
        vk::PrimitiveTopology topology {};
        vk::PolygonMode       polygon_mode {};
        vk::CullModeFlags     cull_mode;
        vk::FrontFace         front_face {};
        b32                   depth_test_enable {};
        b32                   depth_write_enable {};
        vk::CompareOp         depth_compare_op {};
        vk::Format            color_format {};
        vk::Format            depth_format {};
        b32                   blend_enable {};
        std::string           name;

        bool operator== (const GraphicsPipelineDescriptor&) const = default;
    };

    class PipelineManager
    {
    public:
        using GraphicsPipeline = util::OpaqueHandle<"Graphics Pipeline", u16>;
    public:
        explicit PipelineManager(const Device&, vk::PipelineLayout bindlessPipelineLayout);
        ~PipelineManager();

        PipelineManager(const PipelineManager&)             = delete;
        PipelineManager(PipelineManager&&)                  = delete;
        PipelineManager& operator= (const PipelineManager&) = delete;
        PipelineManager& operator= (PipelineManager&&)      = delete;

        [[nodiscard]] GraphicsPipeline createGraphicsPipeline(GraphicsPipelineDescriptor) const;
        void                           destroyGraphicsPipeline(GraphicsPipeline) const;

        bool couldAnyShadersReload() const;
        void reloadShaders() const;

        /// The value returned is valid until the next call to reloadShaders
        [[nodiscard]] vk::Pipeline getPipeline(const GraphicsPipeline&) const;

    private:

        struct GraphicsPipelineInternalStorage
        {
            GraphicsPipelineDescriptor      descriptor;
            vk::UniquePipeline              pipeline;
            std::filesystem::path           vertex_path;
            std::filesystem::file_time_type vertex_modify_time;
            std::filesystem::path           fragment_path;
            std::filesystem::file_time_type fragment_modify_time;
        };

        /// Returns the new shader module and the time the file's info
        std::pair<vk::UniqueShaderModule, std::filesystem::path>
        createShaderModuleFromShaderPath(const std::string&, vk::ShaderStageFlags) const;

        GraphicsPipelineInternalStorage createGraphicsPipelineFromDescriptor(GraphicsPipelineDescriptor) const;

        vk::Device              device;
        vk::UniquePipelineCache pipeline_cache;

        shaderc::Compiler shader_compiler;

        vk::PipelineLayout bindless_pipeline_layout;

        struct CriticalSection
        {
            util::OpaqueHandleAllocator<GraphicsPipeline> graphics_pipeline_handle_allocator;
            std::vector<GraphicsPipelineInternalStorage>  graphics_pipeline_storage;
        };

        util::RwLock<CriticalSection> critical_section;
    };
} // namespace gfx::core::vulkan