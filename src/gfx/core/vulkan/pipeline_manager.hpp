#pragma once

#include "gfx/slang_compiler.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <filesystem>
#include <variant>
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    class Device;

    struct GraphicsPipelineDescriptor
    {
        std::string           shader_path;
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

        void destroyPipeline(Pipeline) const;

        using UniquePipeline = util::UniqueOpaqueHandle<Pipeline, &PipelineManager::destroyPipeline>;

        [[nodiscard]] UniquePipeline createPipelineUnique(GraphicsPipelineDescriptor) const;
        [[nodiscard]] UniquePipeline createPipelineUnique(ComputePipelineDescriptor) const;
        [[nodiscard]] Pipeline       createPipeline(GraphicsPipelineDescriptor) const;
        [[nodiscard]] Pipeline       createPipeline(ComputePipelineDescriptor) const;
        /// The value returned is valid until the next call to reloadShaders or till the pipeline is destroyed,
        /// whichever is sooner
        [[nodiscard]] vk::Pipeline   getPipeline(const Pipeline&) const;
        [[nodiscard]] bool           couldAnyShadersReload() const;
        void                         reloadShaders() const;


    private:

        vk::Device                     device;
        vk::UniquePipelineCache        pipeline_cache;
        util::Mutex<SaneSlangCompiler> sane_slang_compiler;

        vk::PipelineLayout bindless_pipeline_layout;

        using DependentFileStorage = std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>>;

        struct PipelineCompilationResult
        {
            std::optional<vk::UniquePipeline> maybe_new_pipeline;
            std::string                       warnings_and_errors;
            DependentFileStorage              dependent_files;
        };

        struct PipelineInternalStorage
        {
            std::variant<GraphicsPipelineDescriptor, ComputePipelineDescriptor> descriptor;
            DependentFileStorage                                                dependent_files;

            std::optional<vk::UniquePipeline>                                       current_pipeline;
            std::optional<util::MaybeAsynchronousObject<PipelineCompilationResult>> maybe_new_pipeline;
        };

        struct CriticalSection
        {
            util::OpaqueHandleAllocator<Pipeline> pipeline_handle_allocator;
            std::vector<PipelineInternalStorage>  pipeline_storage;
        };

        util::Mutex<CriticalSection> critical_section;

        util::MaybeAsynchronousObject<PipelineCompilationResult>
            startPipelineCompilation(GraphicsPipelineDescriptor) const;
        util::MaybeAsynchronousObject<PipelineCompilationResult>
            startPipelineCompilation(ComputePipelineDescriptor) const;
    };
} // namespace gfx::core::vulkan