
#include "gfx/render/renderer.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/vulkan/swapchain.hpp"
#include "gfx/render/window.hpp"
#include "slang-com-ptr.h"
#include "slang-gfx.h"
#include "slang.h"
#include "slang/slang-cpp-types.h"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

using namespace std::string_view_literals;

void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob)
{
    if (diagnosticsBlob != nullptr)
    {
        log::critical("Slang Compile Error:\n{}", static_cast<const char*>(diagnosticsBlob->getBufferPointer()));
    }
}

const std::string shader = R"(
// // hello-world.slang
// StructuredBuffer<float> buffer0;
// StructuredBuffer<float> buffer1;
// RWStructuredBuffer<float> result;

// [shader("compute")]
// [numthreads(1,1,1)]
// void computeMain(uint3 threadId : SV_DispatchThreadID)
// {
//     uint index = threadId.x;
//     result[index] = buffer0[index] + buffer1[index];
// }
// T compute<T:IInteger>(T a, T b)
// {
//     return a * b + T(1);
// }

// RWStructuredBuffer<int> outputBuffer;

// [numthreads(1,1,1)]
// void computeMain()
// {
//     // int a = 2;
//     // int b = 3;
//     // outputBuffer[0] = compute(a, b); // result = 2*3 + 1 = 7

//     // int16_t2 a2 = int16_t2(2, 3);
//     // int16_t2 b2 = int16_t2(4, 5);
//     // // result2 = int16_t2(2*4 + 1, 3*5 + 1) = int16_t2(9, 16)
//     // int16_t2 result2 = compute<int16_t2>(a2, b2);
//     // outputBuffer[1] = result2.x;
// }
// Ensure Slang core library is loaded

RWStructuredBuffer<uint> output : register(u0);

[numthreads(1, 1, 1)]
[shader("compute")]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    Optional<uint> a = 3;
    output[tid.x] = 42; // Simple test to verify the shader compiles and runs
}

)";

void createShaderBlob()
{
    // First we need to create slang global session with work with the Slang API.
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    auto                                 res = slang::createGlobalSession(slangGlobalSession.writeRef());
    // slang::shutdown();
    log::info("global {}", res);

    // Next we create a compilation session to generate SPIRV code from Slang source.
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc  targetDesc  = {};
    targetDesc.format              = SLANG_SPIRV;
    targetDesc.profile             = slangGlobalSession->findProfile("spirv_1_5");
    targetDesc.flags               = 0;

    sessionDesc.targets     = &targetDesc;
    sessionDesc.targetCount = 1;

    std::vector<slang::CompilerOptionEntry> options;
    options.push_back(
        {slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}});
    sessionDesc.compilerOptionEntries    = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    Slang::ComPtr<slang::ISession> session;
    res = slangGlobalSession->createSession(sessionDesc, session.writeRef());
    log::info("session {}", res);

    // Once the session has been obtained, we can start loading code into it.
    //
    // The simplest way to load code is by calling `loadModule` with the name of a Slang
    // module. A call to `loadModule("hello-world")` will behave more or less as if you
    // wrote:
    //
    //      import hello_world;
    //
    // In a Slang shader file. The compiler will use its search paths to try to locate
    // `hello-world.slang`, then compile and load that file. If a matching module had
    // already been loaded previously, that would be used directly.
    slang::IModule* slangModule = nullptr;
    {
        Slang::ComPtr<slang::IBlob> diagnosticBlob;
        slangModule =
            session->loadModuleFromSourceString("hello", "shaders/hello", shader.c_str(), diagnosticBlob.writeRef());
        diagnoseIfNeeded(diagnosticBlob);
        if (!slangModule)
        {
            panic("oops");
        }
    }

    // Loading the `hello-world` module will compile and check all the shader code in it,
    // including the shader entry points we want to use. Now that the module is loaded
    // we can look up those entry points by name.
    //
    // Note: If you are using this `loadModule` approach to load your shader code it is
    // important to tag your entry point functions with the `[shader("...")]` attribute
    // (e.g., `[shader("compute")] void computeMain(...)`). Without that information there
    // is no umambiguous way for the compiler to know which functions represent entry
    // points when it parses your code via `loadModule()`.
    //
    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    res = slangModule->findAndCheckEntryPoint("computeMain", SLANG_STAGE_COMPUTE, entryPoint.writeRef(), nullptr);
    // res = slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());
    log::info("res {}", res);

    // At this point we have a few different Slang API objects that represent
    // pieces of our code: `module`, `vertexEntryPoint`, and `fragmentEntryPoint`.
    //
    // A single Slang module could contain many different entry points (e.g.,
    // four vertex entry points, three fragment entry points, and two compute
    // shaders), and before we try to generate output code for our target API
    // we need to identify which entry points we plan to use together.
    //
    // Modules and entry points are both examples of *component types* in the
    // Slang API. The API also provides a way to build a *composite* out of
    // other pieces, and that is what we are going to do with our module
    // and entry points.
    //
    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.push_back(slangModule);
    componentTypes.push_back(entryPoint);

    // Actually creating the composite component type is a single operation
    // on the Slang session, but the operation could potentially fail if
    // something about the composite was invalid (e.g., you are trying to
    // combine multiple copies of the same module), so we need to deal
    // with the possibility of diagnostic output.
    //
    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult                 result = session->createCompositeComponentType(
            componentTypes.data(), componentTypes.size(), composedProgram.writeRef(), diagnosticsBlob.writeRef());
        diagnoseIfNeeded(diagnosticsBlob);
        log::info("result {}", result);
    }

    // Now we can call `composedProgram->getEntryPointCode()` to retrieve the
    // compiled SPIRV code that we will use to create a vulkan compute pipeline.
    // This will trigger the final Slang compilation and spirv code generation.
    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = composedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(), diagnosticsBlob.writeRef());
        diagnoseIfNeeded(diagnosticsBlob);

        log::info("result {}", result);
    }

    log::trace("{}", spirvCode->getBufferSize());

    std::string out {};

    for (int i = 0; i < spirvCode->getBufferSize(); ++i)
    {
        char c = static_cast<const char*>(spirvCode->getBufferPointer())[i];

        if (c != '\0' && c != '\n')
        {
            out += std::format("{}", c);
        }
    }

    if (!out.empty())
    {
        out.pop_back();
        out.pop_back();
    }

    log::trace("{}", out);

    panic("done");
}

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

        createShaderBlob();

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