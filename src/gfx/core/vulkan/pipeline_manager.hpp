#pragma once

#include "slang_compiler.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/threads.hpp"
#include <filesystem>
#include <variant>
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    class Device;

    struct GraphicsPipelineDescriptor
    {
        std::string           vertex_shader_path;
        std::string           fragment_shader_path;
        vk::PrimitiveTopology topology;
        vk::PolygonMode       polygon_mode;
        vk::CullModeFlags     cull_mode;
        vk::FrontFace         front_face;
        b32                   depth_test_enable;
        b32                   depth_write_enable;
        vk::CompareOp         depth_compare_op;
        vk::Format            color_format;
        vk::Format            depth_format;
        b32                   blend_enable;
        std::string           name;

        bool operator== (const GraphicsPipelineDescriptor&) const = default;
    };

    struct ComputePipelineDescriptor
    {
        std::string compute_shader_path;
        std::string name;
    };

    class PipelineManager
    {
    public:
        using Pipeline = util::OpaqueHandle<"Vulkan Pipeline", u16>;
    public:
        explicit PipelineManager(const Device&, vk::PipelineLayout bindlessPipelineLayout);
        ~PipelineManager();

        PipelineManager(const PipelineManager&)             = delete;
        PipelineManager(PipelineManager&&)                  = delete;
        PipelineManager& operator= (const PipelineManager&) = delete;
        PipelineManager& operator= (PipelineManager&&)      = delete;

        [[nodiscard]] Pipeline createPipeline(GraphicsPipelineDescriptor) const;
        [[nodiscard]] Pipeline createPipeline(ComputePipelineDescriptor) const;

        void destroyPipeline(Pipeline) const;

        /// The value returned is valid until the next call to reloadShaders or till the pipeline is destroyed,
        /// whichever is sooner
        [[nodiscard]] vk::Pipeline getPipeline(const Pipeline&) const;
        [[nodiscard]] bool         couldAnyShadersReload() const;
        void                       reloadShaders() const;


    private:

        vk::Device                          device;
        vk::UniquePipelineCache             pipeline_cache;
        util::Mutex<cfi::SaneSlangCompiler> sane_slang_compiler;

        vk::PipelineLayout bindless_pipeline_layout;

        struct PipelineInternalStorage
        {
            std::variant<GraphicsPipelineDescriptor, ComputePipelineDescriptor>            descriptor;
            vk::UniquePipeline                                                             pipeline;
            std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> dependent_files;
        };

        struct CriticalSection
        {
            util::OpaqueHandleAllocator<Pipeline> pipeline_handle_allocator;
            std::vector<PipelineInternalStorage>  pipeline_storage;
        };

        util::Mutex<CriticalSection> critical_section;

        /// Moves from the variable on success
        std::expected<PipelineInternalStorage, std::string> tryCompilePipeline(GraphicsPipelineDescriptor&) const;
        /// Moves from the variable on success
        std::expected<PipelineInternalStorage, std::string> tryCompilePipeline(ComputePipelineDescriptor&) const;

        std::pair<PipelineInternalStorage, std::optional<std::string>>
            tryRecompilePipeline(PipelineInternalStorage) const;

        struct FormShaderModuleFromShaderSourceResult
        {
            vk::UniqueShaderModule                                                         module;
            std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> dependent_files;
        };
    };
} // namespace gfx::core::vulkan