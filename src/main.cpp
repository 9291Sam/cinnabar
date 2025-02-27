
#include "gfx/render/renderer.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/vulkan/swapchain.hpp"
#include "gfx/render/window.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

int main()
{
    util::GlobalLoggerContext loggerContext {};

    CPPTRACE_TRYZ
    {
        log::info(
            "Cinnabar v{}.{}.{}.{}{}",
            CINNABAR_VERSION_MAJOR,
            CINNABAR_VERSION_MINOR,
            CINNABAR_VERSION_PATCH,
            CINNABAR_VERSION_TWEAK,
            CINNABAR_DEBUG_BUILD ? " Debug Build" : "");

        gfx::render::Renderer renderer {};

        while (!renderer.shouldWindowClose())
        {
            auto rec = [](vk::CommandBuffer               commandBuffer,
                          u32                             swapchainImage,
                          gfx::render::vulkan::Swapchain& swapchain,
                          std::size_t                     frameIndex)
            {
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
                        .srcQueueFamilyIndex {0},
                        .dstQueueFamilyIndex {0},
                        .image {swapchain.getImages()[swapchainImage]},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});
            };

            renderer.recordOnThread(rec);
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

// // /*

// //         shaderc::CompileOptions options {};
// //         options.SetSourceLanguage(shaderc_source_language_glsl);
// //         options.SetTargetEnvironment(shaderc_target_env_vulkan,
// shaderc_env_version_vulkan_1_3);
// //         options.SetTargetSpirv(shaderc_spirv_version_1_6);
// //         options.SetOptimizationLevel(shaderc_optimization_level_performance);

// //         std::string string = R"(
// // #version 460

// // #extension GL_EXT_shader_explicit_arithmetic_types : require
// // #extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
// // #extension GL_EXT_nonuniform_qualifier : require
// // #extension GL_KHR_shader_subgroup_basic : require
// // #extension GL_KHR_shader_subgroup_ballot : require
// // #extension GL_KHR_shader_subgroup_quad : require

// // layout(local_size_x = 256) in;

// // layout(std430, binding = 0) buffer DataBuffer {
// //     uint data[];
// // };

// // void main() {
// //     uint idx = gl_GlobalInvocationID.x;
// //     data[idx] += 1;
// // }

// //         )";

// //         shaderc::Compiler                compiler {};
// //         shaderc::CompilationResult<char> compileResult =
// //             compiler.CompileGlslToSpvAssembly(string, shaderc_compute_shader, "", options);

// //         if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
// //         {
// //             log::error(
// //                 "compile failed: {} {}",
// //                 static_cast<int>(compileResult.GetCompilationStatus()),
// //                 compileResult.GetErrorMessage());
// //         }
// //         else
// //         {
// //             std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

// //             std::array<char, 16> hexChars {
// //                 '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
// 'F'};
// //             std::string result {};
// //             result.reserve(data.size() * 6);

// //             for (char d : data)
// //             {
// //                 const u32 c = static_cast<u8>(d);

// //                 result.push_back('0');
// //                 result.push_back('x');
// //                 result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
// //                 result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
// //                 result.push_back(',');
// //                 result.push_back(' ');
// //             }

// //             if (!result.empty())
// //             {
// //                 result.pop_back();
// //                 result.pop_back();
// //             }

// //             log::info("{}", result);
// //         }

// //         */