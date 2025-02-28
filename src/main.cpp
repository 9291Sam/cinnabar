
#include "gfx/render/renderer.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/vulkan/swapchain.hpp"
#include "gfx/render/window.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cstddef>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

// void comp()
// {
//     shaderc::CompileOptions options {};
//     options.SetSourceLanguage(shaderc_source_language_glsl);
//     options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
//     options.SetTargetSpirv(shaderc_spirv_version_1_6);
//     options.SetOptimizationLevel(shaderc_optimization_level_performance);
//     options.SetGenerateDebugInfo();

//     std::string string = R"(
// #version 460

// #extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
// #extension GL_EXT_nonuniform_qualifier : require
// #extension GL_KHR_shader_subgroup_basic : require
// #extension GL_KHR_shader_subgroup_ballot : require
// #extension GL_KHR_shader_subgroup_quad : require

// layout(local_size_x = 256) in;

// layout(std430, binding = 0) buffer DataBuffer {
//     uint data[];
// };

// void main() {
//     uint idx = gl_GlobalInvocationID.x;
//     data[idx] += 1;
// }

//         )";

//     shaderc::Compiler                compiler {};
//     shaderc::CompilationResult<char> compileResult =
//         compiler.CompileGlslToSpvAssembly(string, shaderc_compute_shader, "", options);

//     if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     {
//         log::error(
//             "compile failed: {} {}",
//             static_cast<int>(compileResult.GetCompilationStatus()),
//             compileResult.GetErrorMessage());
//     }
//     else
//     {
//         std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

//         std::array<char, 16> hexChars {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
//         'F'}; std::string          result {}; result.reserve(data.size() * 6);

//         for (char d : data)
//         {
//             const u32 c = static_cast<u8>(d);

//             result.push_back('0');
//             result.push_back('x');
//             result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
//             result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
//             result.push_back(',');
//             result.push_back(' ');
//         }

//         if (!result.empty())
//         {
//             result.pop_back();
//             result.pop_back();
//         }

//         log::info("{}", result);
//     }
// }

static const char* source = R"(
static const float2 Positions[3] = { { -0.6, 0.6 }, { 0.6, 0.6 }, { 0, -0.6 } };
static const float3 Colors[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

struct VertexOutput
{
    float4 position : SV_Position;
    float3    color;
};

[shader("vertex")]
VertexOutput Vertex(uint id : SV_VertexID)
{
    VertexOutput out = {};
    out.position = float4(Positions[id], 0, 1);
    out.color = Colors[id];
    return out;
}

[shader("fragment")]
float4 Fragment(in VertexOutput vout)
{
    return float4(vout.color, 1);
}
)";

std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> loadShaders()
{
    Slang::ComPtr<slang::IGlobalSession> slang_global_session;

    if (SLANG_FAILED(slang::createGlobalSession(slang_global_session.writeRef())))
    {
        panic("slang failed creating global session");
    }

    slang::TargetDesc target_desc {
        .format                      = SLANG_SPIRV,
        .profile                     = slang_global_session->findProfile("glsl460"),
        .forceGLSLScalarBufferLayout = true,
    };

    std::vector<slang::CompilerOptionEntry> compiler_options;
    compiler_options.push_back(slang::CompilerOptionEntry {
        slang::CompilerOptionName::EmitSpirvDirectly,
        slang::CompilerOptionValue {slang::CompilerOptionValueKind::Int, 1, 1, "", ""}});
    compiler_options.push_back(slang::CompilerOptionEntry {
        slang::CompilerOptionName::VulkanUseEntryPointName,
        slang::CompilerOptionValue {slang::CompilerOptionValueKind::Int, 1, 1, "", ""}});

    slang::SessionDesc session_desc {
        .targets                  = &target_desc,
        .targetCount              = 1,
        .defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .searchPaths              = nullptr,
        .searchPathCount          = 0,
        .fileSystem               = nullptr,
        .compilerOptionEntries    = compiler_options.data(),
        .compilerOptionEntryCount = uint32_t(compiler_options.size()),
    };

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef())))
    {
        panic("slang failed created local session");
    }

    slang::IModule* slang_module = {};
    {
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        slang_module = session->loadModuleFromSourceString("triangle", "triangle", source, diagnostics_blob.writeRef());

        if (diagnostics_blob)
        {
            log::info("{}", reinterpret_cast<const char*>(diagnostics_blob->getBufferPointer()));
        }

        if (!slang_module)
        {
            panic("Slang module could not be created");
        }
    }

    // Find all entry points

    std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points;

    auto entry_point_count = u32(slang_module->getDefinedEntryPointCount());
    for (u32 i = 0; i < entry_point_count; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(slang_module->getDefinedEntryPoint(i, entry_point.writeRef())))
        {
            panic("slang failed to read defined entry point");
        }

        entry_points.push_back(std::move(entry_point));
    }

    // Create composed program including all entry points

    std::vector<slang::IComponentType*> component_types;
    component_types.push_back(slang_module);
    for (auto& entry_point : entry_points)
    {
        component_types.push_back(entry_point);
    }

    Slang::ComPtr<slang::IComponentType> composed_program;
    {
        Slang::ComPtr<slang::IBlob> diagnostics_blob;
        auto                        result = session->createCompositeComponentType(
            component_types.data(), component_types.size(), composed_program.writeRef(), diagnostics_blob.writeRef());

        if (diagnostics_blob)
        {
            log::info("{}", (const char*)diagnostics_blob->getBufferPointer());
        }

        if (SLANG_FAILED(result))
        {
            panic("Slang failed composing program");
        }
    }

    // Reflect on module entry points

    auto                                                         layout = composed_program->getLayout();
    std::vector<std::pair<std::string, vk::ShaderStageFlagBits>> entry_point_names;
    {
        entry_point_count = u32(layout->getEntryPointCount());
        for (u32 i = 0; i < entry_point_count; ++i)
        {
            slang::EntryPointReflection* entry_point = layout->getEntryPointByIndex(i);

            vk::ShaderStageFlagBits stage {};

            switch (entry_point->getStage())
            {
            case SLANG_STAGE_VERTEX:
                stage = vk::ShaderStageFlagBits::eVertex;
                break;
            case SLANG_STAGE_FRAGMENT:
                stage = vk::ShaderStageFlagBits::eFragment;
                break;
            default:
                panic("invalid stage");
            }

            entry_point_names.emplace_back(std::pair {entry_point->getName(), stage});

            log::trace("found entrypoint {}", entry_point->getName());
        }
    }

    std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> spirvMap {};
    int                                                           idx = 0;

    for (auto [entry_point_name, stage] : entry_point_names)
    {
        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(slang_module->findEntryPointByName(entry_point_name.c_str(), entry_point.writeRef())))
        {
            panic("slang failed to find entry point: {}", entry_point_name);
        }

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto                        result = session->createCompositeComponentType(
                component_types.data(),
                component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());

            if (diagnostics_blob)
            {
                log::info("{}", (const char*)diagnostics_blob->getBufferPointer());
            }

            if (SLANG_FAILED(result))
            {
                panic("slang failed composing shader");
            }
        }

        Slang::ComPtr<slang::IBlob> spirv_code;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto                        result =
                composed_program->getEntryPointCode(idx, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());
            if (diagnostics_blob)
            {
                log::info("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (SLANG_FAILED(result))
            {
                panic("slang failed compiling shader");
            }
        }

        idx += 1; // HACK
        log::warn("awful!");

        std::vector<u32> output(spirv_code->getBufferSize() / 4);
        std::memcpy(output.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

        spirvMap[stage] = std::move(output);
    }

    return spirvMap;
}

int main()
{
    util::GlobalLoggerContext loggerContext {};

    CPPTRACE_TRYZ
    {
        log::info(
            "Cinnabar has started v{}.{}.{}.{}{}",
            CINNABAR_VERSION_MAJOR,
            CINNABAR_VERSION_MINOR,
            CINNABAR_VERSION_PATCH,
            CINNABAR_VERSION_TWEAK,
            CINNABAR_DEBUG_BUILD ? " Debug Build" : "");

        gfx::render::Renderer renderer {};
        bool                  hasResizeOccurred = true;
        const u32             graphicsQueueIndex =
            renderer.getDevice()->getFamilyOfQueueType(gfx::render::vulkan::Device::QueueType::Graphics).value();

        std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> shadersSPirv = loadShaders();

        struct CacheablePipelineShaderStageCreateInfo
        {
            vk::ShaderStageFlagBits                 stage;
            std::shared_ptr<vk::UniqueShaderModule> shader;
            std::string                             entry_point;

            // bool operator== (const CacheablePipelineShaderStageCreateInfo&) const = default;
        };

        std::vector<CacheablePipelineShaderStageCreateInfo> stages {};

        for (auto [flags, code] : shadersSPirv)
        {
            log::trace("{} {}", vk::to_string(flags), code.size());

            stages.push_back(CacheablePipelineShaderStageCreateInfo {
                .stage {flags},
                .shader {std::make_shared<vk::UniqueShaderModule>(
                    renderer.getDevice()->getDevice().createShaderModuleUnique({
                        .sType {vk::StructureType::eShaderModuleCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .codeSize {code.size() * sizeof(u32)},
                        .pCode {code.data()},
                    }))},
                .entry_point {flags & vk::ShaderStageFlagBits::eVertex ? "Vertex" : "Fragment"},
            });
        }

        struct CacheableGraphicsPipelineCreateInfo
        {
            std::vector<CacheablePipelineShaderStageCreateInfo> stages;
            std::vector<vk::VertexInputAttributeDescription>    vertex_attributes;
            std::vector<vk::VertexInputBindingDescription>      vertex_bindings;
            vk::PrimitiveTopology                               topology;
            bool                                                discard_enable;
            vk::PolygonMode                                     polygon_mode;
            vk::CullModeFlags                                   cull_mode;
            vk::FrontFace                                       front_face;
            bool                                                depth_test_enable;
            bool                                                depth_write_enable;
            vk::CompareOp                                       depth_compare_op;
            vk::Format                                          color_format;
            vk::Format                                          depth_format;
            bool                                                blend_enable;
            std::shared_ptr<vk::UniquePipelineLayout>           layout;
            std::string                                         name;

            bool operator== (const CacheableGraphicsPipelineCreateInfo&) const = default;
        };

        const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
            .sType {vk::StructureType::ePipelineLayoutCreateInfo},
            .pNext {nullptr},
            .flags {},
            .setLayoutCount {0},
            .pSetLayouts {nullptr},
            .pushConstantRangeCount {0},
            .pPushConstantRanges {nullptr},
        };

        std::shared_ptr<vk::UniquePipelineLayout> layout = std::make_shared<vk::UniquePipelineLayout>(
            renderer.getDevice()->getDevice().createPipelineLayoutUnique(pipelineLayoutCreateInfo));

        CacheableGraphicsPipelineCreateInfo info {
            .stages {std::move(stages)},
            .vertex_attributes {},
            .vertex_bindings {},
            .topology {vk::PrimitiveTopology::eTriangleList},
            .discard_enable {false},
            .polygon_mode {vk::PolygonMode::eFill},
            .cull_mode {vk::CullModeFlagBits::eNone},
            .front_face {vk::FrontFace::eClockwise},
            .depth_test_enable {false},
            .depth_write_enable {false},
            .depth_compare_op {},
            .color_format {gfx::render::Renderer::ColorFormat.format},
            .depth_format {},
            .blend_enable {true},
            .layout {std::move(layout)},
            .name {"idk bro"}};

        std::vector<vk::PipelineShaderStageCreateInfo> denseStages {};
        denseStages.reserve(info.stages.size());

        for (const CacheablePipelineShaderStageCreateInfo& s : info.stages)
        {
            denseStages.push_back(vk::PipelineShaderStageCreateInfo {

                .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                .pNext {nullptr},
                .flags {},
                .stage {static_cast<vk::ShaderStageFlagBits>(s.stage)},
                .module {**s.shader},
                .pName {s.entry_point.c_str()},
                .pSpecializationInfo {},
            });
        }

        const vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {
            .sType {vk::StructureType::ePipelineVertexInputStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .vertexBindingDescriptionCount {static_cast<u32>(info.vertex_bindings.size())},
            .pVertexBindingDescriptions {info.vertex_bindings.data()},
            .vertexAttributeDescriptionCount {static_cast<u32>(info.vertex_attributes.size())},
            .pVertexAttributeDescriptions {info.vertex_attributes.data()},
        };

        const vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {
            .sType {vk::StructureType::ePipelineInputAssemblyStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .topology {info.topology},
            .primitiveRestartEnable {false},
        };

        const vk::Viewport nullDynamicViewport {};
        const vk::Rect2D   nullDynamicScissor {};

        const vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {
            .sType {vk::StructureType::ePipelineViewportStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .viewportCount {1},
            .pViewports {&nullDynamicViewport},
            .scissorCount {1},
            .pScissors {&nullDynamicScissor},
        };

        const vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo {
            .sType {vk::StructureType::ePipelineRasterizationStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .depthClampEnable {vk::False},
            .rasterizerDiscardEnable {info.discard_enable},
            .polygonMode {info.polygon_mode},
            .cullMode {info.cull_mode},
            .frontFace {info.front_face},
            .depthBiasEnable {vk::False},
            .depthBiasConstantFactor {0.0},
            .depthBiasClamp {0.0},
            .depthBiasSlopeFactor {0.0},
            .lineWidth {1.0},
        };

        const vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo {
            .sType {vk::StructureType::ePipelineMultisampleStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .rasterizationSamples {vk::SampleCountFlagBits::e1},
            .sampleShadingEnable {vk::False},
            .minSampleShading {1.0},
            .pSampleMask {nullptr},
            .alphaToCoverageEnable {vk::False},
            .alphaToOneEnable {vk::False},
        };

        const vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {
            .sType {vk::StructureType::ePipelineDepthStencilStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .depthTestEnable {info.depth_test_enable},
            .depthWriteEnable {info.depth_write_enable},
            .depthCompareOp {info.depth_compare_op},
            .depthBoundsTestEnable {vk::False}, // TODO: expose?
            .stencilTestEnable {vk::False},
            .front {},
            .back {},
            .minDepthBounds {0.0}, // TODO: expose?
            .maxDepthBounds {1.0}, // TODO: expose?
        };

        const vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {
            .blendEnable {info.blend_enable},
            .srcColorBlendFactor {vk::BlendFactor::eSrcAlpha},
            .dstColorBlendFactor {vk::BlendFactor::eOneMinusSrcAlpha},
            .colorBlendOp {vk::BlendOp::eAdd},
            .srcAlphaBlendFactor {vk::BlendFactor::eOne},
            .dstAlphaBlendFactor {vk::BlendFactor::eZero},
            .alphaBlendOp {vk::BlendOp::eAdd},
            .colorWriteMask {
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA},
        };

        const vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {
            .sType {vk::StructureType::ePipelineColorBlendStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .logicOpEnable {vk::False},
            .logicOp {vk::LogicOp::eCopy},
            .attachmentCount {1},
            .pAttachments {&pipelineColorBlendAttachmentState},
            .blendConstants {{0.0, 0.0, 0.0, 0.0}},
        };

        const std::array pipelineDynamicStates {
            vk::DynamicState::eScissor,
            vk::DynamicState::eViewport,
        };

        const vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {
            .sType {vk::StructureType::ePipelineDynamicStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .dynamicStateCount {static_cast<u32>(pipelineDynamicStates.size())},
            .pDynamicStates {pipelineDynamicStates.data()},
        };

        const vk::PipelineRenderingCreateInfo renderingInfo {
            .sType {vk::StructureType::ePipelineRenderingCreateInfo},
            .pNext {nullptr},
            .viewMask {0},
            .colorAttachmentCount {info.color_format == vk::Format::eUndefined ? 0u : 1u},
            .pColorAttachmentFormats {&info.color_format},
            .depthAttachmentFormat {info.depth_format},
            .stencilAttachmentFormat {},
        };

        const vk::GraphicsPipelineCreateInfo pipelineCreateInfo {
            .sType {vk::StructureType::eGraphicsPipelineCreateInfo},
            .pNext {&renderingInfo},
            .flags {},
            .stageCount {static_cast<u32>(denseStages.size())},
            .pStages {denseStages.data()},
            .pVertexInputState {&pipelineVertexInputStateCreateInfo},
            .pInputAssemblyState {&pipelineInputAssemblyStateCreateInfo},
            .pTessellationState {nullptr},
            .pViewportState {&pipelineViewportStateCreateInfo},
            .pRasterizationState {&pipelineRasterizationStateCreateInfo},
            .pMultisampleState {&pipelineMultisampleStateCreateInfo},
            .pDepthStencilState {&pipelineDepthStencilStateCreateInfo},
            .pColorBlendState {&pipelineColorBlendStateCreateInfo},
            .pDynamicState {&pipelineDynamicStateCreateInfo},
            .layout {**info.layout},
            .renderPass {nullptr},
            .subpass {0},
            .basePipelineHandle {nullptr},
            .basePipelineIndex {0},
        };

        auto [result, pipeline] =
            renderer.getDevice()->getDevice().createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);

        assert::critical(result == vk::Result::eSuccess, "oops {}", vk::to_string(result));

        while (!renderer.shouldWindowClose())
        {
            auto rec = [&](vk::CommandBuffer               commandBuffer,
                           u32                             swapchainImageIdx,
                           gfx::render::vulkan::Swapchain& swapchain,
                           std::size_t                     frameIndex)
            {
                const vk::Image     thisSwapchainImage     = swapchain.getImages()[swapchainImageIdx];
                const vk::ImageView thisSwapchainImageView = swapchain.getViews()[swapchainImageIdx];

                vk::Extent2D renderExtent = swapchain.getExtent();

                const vk::Rect2D scissor {.offset {vk::Offset2D {.x {0}, .y {0}}}, .extent {renderExtent}};

                const vk::Viewport renderViewport {
                    .x {0.0},
                    .y {static_cast<f32>(renderExtent.height)},
                    .width {static_cast<f32>(renderExtent.width)},
                    .height {static_cast<f32>(renderExtent.height) * -1.0f},
                    .minDepth {0.0},
                    .maxDepth {1.0},
                };

                if (hasResizeOccurred)
                {
                    const std::span<const vk::Image> swapchainImages = swapchain.getImages();

                    std::vector<vk::ImageMemoryBarrier> swapchainMemoryBarriers {};
                    swapchainMemoryBarriers.reserve(swapchainImages.size());

                    for (const vk::Image i : swapchainImages)
                    {
                        swapchainMemoryBarriers.push_back(vk::ImageMemoryBarrier {
                            .sType {vk::StructureType::eImageMemoryBarrier},
                            .pNext {nullptr},
                            .srcAccessMask {vk::AccessFlagBits::eNone},
                            .dstAccessMask {vk::AccessFlagBits::eNone},
                            .oldLayout {vk::ImageLayout::eUndefined},
                            .newLayout {vk::ImageLayout::ePresentSrcKHR},
                            .srcQueueFamilyIndex {graphicsQueueIndex},
                            .dstQueueFamilyIndex {graphicsQueueIndex},
                            .image {i},
                            .subresourceRange {vk::ImageSubresourceRange {
                                .aspectMask {vk::ImageAspectFlagBits::eColor},
                                .baseMipLevel {0},
                                .levelCount {1},
                                .baseArrayLayer {0},
                                .layerCount {1}}},
                        });
                    }

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {},
                        {},
                        {},
                        swapchainMemoryBarriers);
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eNone},
                        .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .oldLayout {vk::ImageLayout::ePresentSrcKHR},
                        .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {thisSwapchainImage},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                // Simple Color
                {
                    const vk::RenderingAttachmentInfo colorAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {thisSwapchainImageView},
                        .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            vk::ClearColorValue {.float32 {{192.0f / 255.0f, 57.0f / 255.0f, 43.0f / 255.0f, 1.0f}}}},
                    };

                    const vk::RenderingInfo simpleColorRenderingInfo {
                        .sType {vk::StructureType::eRenderingInfo},
                        .pNext {nullptr},
                        .flags {},
                        .renderArea {scissor},
                        .layerCount {1},
                        .viewMask {0},
                        .colorAttachmentCount {1},
                        .pColorAttachments {&colorAttachmentInfo},
                        .pDepthAttachment {nullptr},
                        .pStencilAttachment {nullptr},
                    };

                    commandBuffer.setViewport(0, {renderViewport});
                    commandBuffer.setScissor(0, {scissor});

                    commandBuffer.beginRendering(simpleColorRenderingInfo);

                    // commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
                    // commandBuffer.draw(3, 1, 0, 0);

                    commandBuffer.endRendering();
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .dstAccessMask {vk::AccessFlagBits::eColorAttachmentRead},
                        .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .newLayout {vk::ImageLayout::ePresentSrcKHR},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {thisSwapchainImage},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});
            };

            if (renderer.getWindow()->isActionActive(gfx::render::Window::Action::ToggleCursorAttachment))
            {
                renderer.getWindow()->toggleCursor();
            }

            if (renderer.getWindow()->isActionActive(gfx::render::Window::Action::CloseWindow))
            {
                break;
            }

            hasResizeOccurred = renderer.recordOnThread(rec);
        }
    }
    CPPTRACE_CATCHZ(const std::exception& e)
    {
        log::critical(
            "Cinnabar has crashed! | {} {}\n{}",
            e.what(),
            typeid(e).name(),
            cpptrace::from_current_exception().to_string(true));
    }
    CPPTRACE_CATCH_ALT(...)
    {
        log::critical(
            "Cinnabar has crashed! | Unknown Exception type thrown!\n{}",
            cpptrace::from_current_exception().to_string(true));
    }

    log::info("Cinnabar has shutdown successfully!");
}
