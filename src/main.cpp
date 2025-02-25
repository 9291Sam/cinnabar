
// #include "gfx/render/vulkan/device.hpp"
// #include "gfx/render/vulkan/instance.hpp"
// #include "gfx/render/window.hpp"
// #include "util/logger.hpp"
// #include <cpptrace/cpptrace.hpp>
// #include <cpptrace/from_current.hpp>
// #include <fstream>
// #include <vuk/Config.hpp>
// #include <vuk/Executor.hpp>
// #include <vuk/RenderGraph.hpp>
// #include <vuk/Types.hpp>
// #include <vuk/runtime/CommandBuffer.hpp>
// #include <vuk/runtime/ThisThreadExecutor.hpp>
// #include <vuk/runtime/vk/VkRuntime.hpp>
// #include <vulkan/vulkan_handles.hpp>

// inline std::string read_entire_file(const std::string& path)
// {
//     std::ostringstream buf;
//     std::ifstream      input(path.c_str());
//     assert(input);
//     buf << input.rdbuf();
//     return buf.str();
// }

// int main()
// {
//     util::GlobalLoggerContext loggerContext {};

//     CPPTRACE_TRYZ
//     {
//         log::info(
//             "Cinnabar v{}.{}.{}.{}{}",
//             CINNABAR_VERSION_MAJOR,
//             CINNABAR_VERSION_MINOR,
//             CINNABAR_VERSION_PATCH,
//             CINNABAR_VERSION_TWEAK,
//             CINNABAR_DEBUG_BUILD ? " Debug Build" : "");

//         gfx::render::Window           w {{}, {.width = 1920, .height = 1080}, "Cinnabar"};
//         gfx::render::vulkan::Instance instance {};
//         vk::UniqueSurfaceKHR          surface {w.createSurface(*instance)};
//         gfx::render::vulkan::Device   device {*instance, *surface};

//         vuk::FunctionPointers fps {};
//         fps.vkGetInstanceProcAddr =
//             instance.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
//         fps.vkGetDeviceProcAddr =
//             instance.getProcAddress<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
//         fps.load_pfns(*instance, device.getDevice(), true);

//         vk::Queue globalQueue {}; // TODO: fix this shit

//         device.acquireQueue(
//             gfx::render::vulkan::Device::QueueType::Graphics,
//             [&](vk::Queue q)
//             {
//                 globalQueue = q;
//             });

//         std::vector<std::unique_ptr<vuk::Executor>> executors {};
//         executors.push_back(vuk::create_vkqueue_executor(
//             fps,
//             device.getDevice(),
//             static_cast<VkQueue>(globalQueue),
//             0,
//             vuk::DomainFlagBits::eAny));
//         executors.push_back(std::make_unique<vuk::ThisThreadExecutor>());

//         vuk::Runtime vukRuntime {vuk::RuntimeCreateParameters {
//             .instance {*instance},
//             .device {device.getDevice()},
//             .physical_device {device.getPhysicalDevice()},
//             .executors {std::move(executors)},
//             .pointers {fps},
//         }};

//         vukRuntime.set_shader_target_version(vk::ApiVersion12);

//         vuk::PipelineBaseCreateInfo pci;
//         pci.add_glsl(read_entire_file("../src/shaders/triangle.vert"), "triangle.vert");
//         pci.add_glsl(read_entire_file("../src/shaders/triangle.frag"), "triangle.frag");

//         vukRuntime.create_named_pipeline("triangle", pci);

//         int iters = 0;

//         while (!w.shouldClose())
//         {
//             break;
//             log::info("FPS: {}", 1.0f / w.getDeltaTimeSeconds());

//             vuk::Arg<vuk::ImageAttachment, vuk::eColorWrite, vuk::tag_type<4>> color_rt;

//             auto p = vuk::make_pass(
//                 "clearColor",
//                 [](vuk::CommandBuffer& cb, decltype(color_rt) c)
//                 {
//                     cb.set_viewport(0, vuk::Rect2D::framebuffer());
//                     // Set the scissor area to cover the entire framebuffer
//                     cb.set_scissor(0, vuk::Rect2D::framebuffer());
//                     cb.set_rasterization({})    // Set the default rasterization state
//                         .set_color_blend(c, {}) // Set the default color blend state
//                         .bind_graphics_pipeline(
//                             "triangle")    // Recall pipeline for "triangle" and bind
//                         .draw(3, 1, 0, 0); // Draw 3 vertices

//                     return c;
//                 });

//             device->waitIdle();

//             w.endFrame();
//         }
//     }
//     CPPTRACE_CATCHZ(const std::exception& e)
//     {
//         log::critical(
//             "Cinnabar has crashed! | {} {}\n{}",
//             e.what(),
//             typeid(e).name(),
//             cpptrace::from_current_exception().to_string(true));
//     }
//     CPPTRACE_CATCH_ALT(...)
//     {
//         log::critical(
//             "Cinnabar has crashed! | Unknown Exception type thrown!\n{}",
//             cpptrace::from_current_exception().to_string(true));
//     }

//     log::info("Cinnabar has shutdown successfully!");
// }

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
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/window.hpp"
#include <vulkan/vulkan_handles.hpp>
#define GLFW_INCLUDE_VULKAN
#include "glfw/glfw3.h"
#include "vuk/Config.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/runtime/CommandBuffer.hpp"
#include "vuk/runtime/ThisThreadExecutor.hpp"
#include "vuk/runtime/vk/Allocator.hpp"
#include "vuk/runtime/vk/AllocatorHelpers.hpp"
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/vsl/Core.hpp"
#include <VkBootstrap.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <vuk/Types.hpp>
#include <vuk/Value.hpp>
#include <vuk/runtime/vk/VkRuntime.hpp>
#include <vuk/runtime/vk/VkSwapchain.hpp>
#include <vuk/vuk_fwd.hpp>

inline GLFWwindow* create_window_glfw(const char* title, bool resize = true)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (!resize)
    {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    return glfwCreateWindow(1024, 1024, title, NULL, NULL);
}

inline void destroy_window_glfw(GLFWwindow* window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

inline VkSurfaceKHR create_surface_glfw(VkInstance instance, GLFWwindow* window)
{
    VkSurfaceKHR surface = nullptr;
    VkResult     err     = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (err)
    {
        const char* error_msg;
        int         ret = glfwGetError(&error_msg);
        if (ret != 0)
        {
            throw error_msg;
        }
        surface = nullptr;
    }
    return surface;
}

namespace util
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv_coordinates;
    };

    using Mesh = std::pair<std::vector<Vertex>, std::vector<unsigned>>;

    inline vuk::Swapchain make_swapchain(
        vuk::Allocator                allocator,
        vkb::Device                   vkbdevice,
        VkSurfaceKHR                  surface,
        std::optional<vuk::Swapchain> old_swapchain)
    {
        vkb::SwapchainBuilder swb(vkbdevice, surface);
        swb.set_desired_format(
            vuk::SurfaceFormatKHR {vuk::Format::eR8G8B8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear});
        swb.add_fallback_format(
            vuk::SurfaceFormatKHR {vuk::Format::eB8G8R8A8Srgb, vuk::ColorSpaceKHR::eSrgbNonlinear});
        swb.set_desired_present_mode((VkPresentModeKHR)vuk::PresentModeKHR::eImmediate);
        swb.set_image_usage_flags(
            VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        bool                        is_recycle  = false;
        vkb::Result<vkb::Swapchain> vkswapchain = {vkb::Swapchain {}};
        if (!old_swapchain)
        {
            vkswapchain = swb.build();
            old_swapchain.emplace(allocator, vkswapchain->image_count);
        }
        else
        {
            is_recycle = true;
            swb.set_old_swapchain(old_swapchain->swapchain);
            vkswapchain = swb.build();
        }

        if (is_recycle)
        {
            allocator.deallocate(std::span {&old_swapchain->swapchain, 1});
            for (auto& iv : old_swapchain->images)
            {
                allocator.deallocate(std::span {&iv.image_view, 1});
            }
        }

        auto images = *vkswapchain->get_images();
        auto views  = *vkswapchain->get_image_views();

        old_swapchain->images.clear();

        for (auto i = 0; i < images.size(); i++)
        {
            vuk::ImageAttachment ia;
            ia.extent       = {vkswapchain->extent.width, vkswapchain->extent.height, 1};
            ia.format       = (vuk::Format)vkswapchain->image_format;
            ia.image        = vuk::Image {images[i], nullptr};
            ia.image_view   = vuk::ImageView {{0}, views[i]};
            ia.view_type    = vuk::ImageViewType::e2D;
            ia.sample_count = vuk::Samples::e1;
            ia.base_level = ia.base_layer = 0;
            ia.level_count = ia.layer_count = 1;
            old_swapchain->images.push_back(ia);
        }

        old_swapchain->swapchain = vkswapchain->swapchain;
        old_swapchain->surface   = surface;
        return std::move(*old_swapchain);
    }

    inline std::string read_entire_file(const std::string& path)
    {
        std::ostringstream buf;
        std::ifstream      input(path.c_str());
        assert(input);
        buf << input.rdbuf();
        return buf.str();
    }

    inline std::vector<uint32_t> read_spirv(const std::string& path)
    {
        std::ostringstream buf;
        std::ifstream      input(path.c_str(), std::ios::ate | std::ios::binary);
        assert(input);
        size_t                file_size = (size_t)input.tellg();
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
        input.seekg(0);
        input.read(reinterpret_cast<char*>(buffer.data()), file_size);
        return buffer;
    }
} // namespace util

inline std::filesystem::path root;

namespace vuk
{
    struct ExampleRunner;

    struct Example
    {
        std::string_view name;

        std::function<void(ExampleRunner&, vuk::Allocator&)> setup;
        std::function<vuk::Value<vuk::ImageAttachment>(
            ExampleRunner&, vuk::Allocator&, vuk::Value<vuk::ImageAttachment>)>
                                                             render;
        std::function<void(ExampleRunner&, vuk::Allocator&)> cleanup;
    };

    struct ExampleRunner
    {
        gfx::render::Window           new_window;
        gfx::render::vulkan::Instance new_instance;
        vk::UniqueSurfaceKHR          new_surface;
        gfx::render::vulkan::Device   new_device;

        vk::Queue new_queue;

        std::optional<DeviceSuperFrameResource> superframe_resource;
        std::optional<Allocator>                superframe_allocator;

        std::optional<Runtime>                    runtime;
        bool                                      suspend = false;
        std::optional<vuk::Swapchain>             swapchain;
        GLFWwindow*                               window;
        VkSurfaceKHR                              surface;
        std::vector<UntypedValue>                 futures;
        std::mutex                                setup_lock;
        double                                    old_time   = 0;
        uint32_t                                  num_frames = 0;
        bool                                      has_rt;
        vuk::Unique<std::array<VkSemaphore, 3>>   present_ready;
        vuk::Unique<std::array<VkSemaphore, 3>>   render_complete;
        // command buffer and pool for Tracy to do init & collect
        vuk::Unique<vuk::CommandPool>             tracy_cpool;
        vuk::Unique<vuk::CommandBufferAllocation> tracy_cbufai;

        // when called during setup, enqueues a device-side operation to be completed before
        // rendering begins
        void enqueue_setup(UntypedValue&& fut)
        {
            std::scoped_lock _(setup_lock);
            futures.emplace_back(std::move(fut));
        }

        std::vector<Value<SampledImage>> sampled_images;
        std::vector<Example*>            examples;

        ExampleRunner();

        void setup()
        {
            {
                std::vector<std::thread> threads;
                for (auto& ex : examples)
                {
                    threads.emplace_back(std::thread(
                        [&]
                        {
                            ex->setup(*this, *superframe_allocator);
                        }));
                }

                for (std::thread& t : threads)
                {
                    t.join();
                }
            }
            glfwSetWindowSizeCallback(
                window,
                [](GLFWwindow* window, int width, int height)
                {
                    ExampleRunner& runner =
                        *reinterpret_cast<ExampleRunner*>(glfwGetWindowUserPointer(window));
                    if (width == 0 && height == 0)
                    {
                        runner.suspend = true;
                    }
                    else
                    {
                        runner.swapchain = util::make_swapchain(
                            *runner.superframe_allocator,
                            runner.vkbdevice,
                            runner.swapchain->surface,
                            std::move(runner.swapchain));
                        for (auto& iv : runner.swapchain->images)
                        {
                            runner.runtime->set_name(iv.image_view.payload, "Swapchain ImageView");
                        }
                        runner.suspend = false;
                    }
                });
        }

        void render();

        void cleanup()
        {
            runtime->wait_idle();
            for (auto& ex : examples)
            {
                if (ex->cleanup)
                {
                    ex->cleanup(*this, *superframe_allocator);
                }
            }
        }

        void set_window_title(std::string title)
        {
            glfwSetWindowTitle(window, title.c_str());
        }

        double get_time()
        {
            return glfwGetTime();
        }

        ~ExampleRunner()
        {
            tracy_cbufai.reset();
            tracy_cpool.reset();
            present_ready.reset();
            render_complete.reset();
            swapchain.reset();
            superframe_resource.reset();
            runtime.reset();
            auto vkDestroySurfaceKHR =
                (PFN_vkDestroySurfaceKHR)vkbinstance.fp_vkGetInstanceProcAddr(
                    vkbinstance.instance, "vkDestroySurfaceKHR");
            vkDestroySurfaceKHR(vkbinstance.instance, surface, nullptr);
            destroy_window_glfw(window);
            vkb::destroy_device(vkbdevice);
            vkb::destroy_instance(vkbinstance);
        }

        static ExampleRunner& get_runner()
        {
            static ExampleRunner runner;
            return runner;
        }
    };

    inline vuk::ExampleRunner::ExampleRunner()
    {
        vkb::InstanceBuilder builder;
        builder.request_validation_layers()
            .set_debug_callback(
                [](VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT             messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                   void*                                       pUserData) -> VkBool32
                {
                    auto ms = vkb::to_string_message_severity(messageSeverity);
                    auto mt = vkb::to_string_message_type(messageType);
                    printf("[%s: %s](user defined)\n%s\n", ms, mt, pCallbackData->pMessage);
                    return VK_FALSE;
                })
            .set_app_name("vuk_example")
            .set_engine_name("vuk")
            .require_api_version(1, 2, 0)
            .set_app_version(0, 1, 0);
        auto inst_ret = builder.build();
        if (!inst_ret)
        {
            throw std::runtime_error("Couldn't initialise instance");
        }

        has_rt = true;

        vkbinstance                          = inst_ret.value();
        auto                        instance = vkbinstance.instance;
        vkb::PhysicalDeviceSelector selector {vkbinstance};
        window = create_window_glfw("Vuk example", true);
        glfwSetWindowUserPointer(window, this);
        surface = create_surface_glfw(vkbinstance.instance, window);
        selector.set_surface(surface)
            .set_minimum_version(1, 0)
            .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
            .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
            .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
            .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
            .add_desired_extension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
#ifdef __APPLE__
            .add_desired_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
#endif // __APPLE__
            .add_desired_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

        auto                phys_ret = selector.select();
        vkb::PhysicalDevice vkbphysical_device;
        if (!phys_ret)
        {
            has_rt = false;
            vkb::PhysicalDeviceSelector selector2 {vkbinstance};
            selector2.set_surface(surface)
                .set_minimum_version(1, 0)
                .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
                .add_desired_extension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
#ifdef __APPLE__
                .add_desired_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
#endif // __APPLE__
                .add_desired_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
            auto phys_ret2 = selector2.select();
            if (!phys_ret2)
            {
                throw std::runtime_error("Couldn't create physical device");
            }
            else
            {
                vkbphysical_device = phys_ret2.value();
            }
        }
        else
        {
            vkbphysical_device = phys_ret.value();
        }

        physical_device = vkbphysical_device.physical_device;
        vkb::DeviceBuilder               device_builder {vkbphysical_device};
        VkPhysicalDeviceVulkan12Features vk12features {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        vk12features.timelineSemaphore                         = true;
        vk12features.descriptorBindingPartiallyBound           = true;
        vk12features.descriptorBindingUpdateUnusedWhilePending = true;
        vk12features.shaderSampledImageArrayNonUniformIndexing = true;
        vk12features.runtimeDescriptorArray                    = true;
        vk12features.descriptorBindingVariableDescriptorCount  = true;
        vk12features.hostQueryReset                            = true;
        vk12features.bufferDeviceAddress                       = true;
        vk12features.shaderOutputLayer                         = true;
        VkPhysicalDeviceVulkan11Features vk11features {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        vk11features.shaderDrawParameters = true;
        VkPhysicalDeviceFeatures2 vk10features {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
        vk10features.features.shaderInt64 = true;
        VkPhysicalDeviceSynchronization2FeaturesKHR sync_feat {
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
            .synchronization2 = true};

        device_builder = device_builder.add_pNext(&vk12features)
                             .add_pNext(&vk11features)
                             .add_pNext(&sync_feat)
                             .add_pNext(&vk10features);

        auto dev_ret = device_builder.build();
        if (!dev_ret)
        {
            throw std::runtime_error("Couldn't create device");
        }
        vkbdevice      = dev_ret.value();
        graphics_queue = vkbdevice.get_queue(vkb::QueueType::graphics).value();
        auto graphics_queue_family_index =
            vkbdevice.get_queue_index(vkb::QueueType::graphics).value();
#ifndef __APPLE__
        transfer_queue = vkbdevice.get_queue(vkb::QueueType::transfer).value();
        auto transfer_queue_family_index =
            vkbdevice.get_queue_index(vkb::QueueType::transfer).value();
#endif // __APPLE__
        device = vkbdevice.device;
        vuk::FunctionPointers fps;
        fps.vkGetInstanceProcAddr = vkbinstance.fp_vkGetInstanceProcAddr;
        fps.load_pfns(instance, device, true);
        std::vector<std::unique_ptr<Executor>> executors;

        executors.push_back(vuk::create_vkqueue_executor(
            fps,
            device,
            graphics_queue,
            graphics_queue_family_index,
            DomainFlagBits::eGraphicsQueue));
#ifndef __APPLE__
        executors.push_back(vuk::create_vkqueue_executor(
            fps,
            device,
            transfer_queue,
            transfer_queue_family_index,
            DomainFlagBits::eTransferQueue));
#endif // __APPLE__
        executors.push_back(std::make_unique<ThisThreadExecutor>());

        runtime.emplace(
            RuntimeCreateParameters {instance, device, physical_device, std::move(executors), fps});
        const unsigned num_inflight_frames = 3;
        superframe_resource.emplace(*runtime, num_inflight_frames);
        superframe_allocator.emplace(*superframe_resource);
        swapchain       = util::make_swapchain(*superframe_allocator, vkbdevice, surface, {});
        present_ready   = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);
        render_complete = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);

        // match shader compilation target version to the vk version we request
        runtime->set_shader_target_version(VK_API_VERSION_1_2);

        superframe_allocator->allocate_semaphores(*present_ready);
        superframe_allocator->allocate_semaphores(*render_complete);
    }
} // namespace vuk

namespace util
{
    struct Register
    {
        Register(vuk::Example& x)
        {
            vuk::ExampleRunner::get_runner().examples.push_back(&x);
        }
    };
} // namespace util

#define CONCAT_IMPL(x, y)   x##y
#define MACRO_CONCAT(x, y)  CONCAT_IMPL(x, y)
#define REGISTER_EXAMPLE(x) util::Register MACRO_CONCAT(_reg_, __LINE__)(x)

void vuk::ExampleRunner::render()
{
    Compiler compiler;

    vuk::wait_for_values_explicit(*superframe_allocator, compiler, futures);
    futures.clear();

    // our main loop
    while (!glfwWindowShouldClose(window))
    {
        // pump the message loop
        glfwPollEvents();
        while (suspend)
        {
            glfwWaitEvents();
        }
        // advance the frame for the allocators and caches used by vuk
        auto& frame_resource = superframe_resource->get_next_frame();
        runtime->next_frame();

        Allocator frame_allocator(frame_resource);
        auto      imported_swapchain = acquire_swapchain(*swapchain);
        // acquire an image on the swapchain
        auto      swapchain_image    = acquire_next_image("swp_img", std::move(imported_swapchain));

        // clear the swapchain image
        Value<ImageAttachment> cleared_image_to_render_into =
            clear_image(std::move(swapchain_image), vuk::ClearColor {0.3f, 0.5f, 0.3f, 1.0f});
        // invoke the render method of the example with the cleared image
        Value<ImageAttachment> example_result =
            examples[0]->render(*this, frame_allocator, std::move(cleared_image_to_render_into));

        // set up some profiling callbacks for our example Tracy integration
        vuk::ProfilingCallbacks cbs;
        cbs.user_data               = &get_runner();
        cbs.on_begin_command_buffer = [](void* user_data, ExecutorTag tag, VkCommandBuffer cbuf)
        {
            ExampleRunner& runner = *reinterpret_cast<vuk::ExampleRunner*>(user_data);

            return (void*)nullptr;
        };
        // runs whenever entering a new vuk::Pass
        // we start a GPU zone and then keep it open
        cbs.on_begin_pass = [](void*, Name, CommandBuffer&, DomainFlagBits)
        {
            return (void*)nullptr;
        };
        // runs whenever a pass has ended, we end the GPU zone we started
        cbs.on_end_pass = [](void*, void*) {};

        // compile the IRModule that contains all the rendering of the example
        // submit and present the results to the swapchain we imported previously
        auto entire_thing = enqueue_presentation(std::move(example_result));

        entire_thing.submit(frame_allocator, compiler, {.callbacks = cbs});

        // update window title with FPS
        if (++num_frames == 16)
        {
            auto new_time       = get_time();
            auto delta          = new_time - old_time;
            auto per_frame_time = delta / 16 * 1000;
            old_time            = new_time;
            num_frames          = 0;
            set_window_title(
                std::string("Vuk example [") + std::to_string(per_frame_time) + " ms / "
                + std::to_string(1000 / per_frame_time) + " FPS]");
        }
    }
}

namespace
{
    vuk::Example x {
        // The display name of this example
        .name = "01_triangle",
        // Setup code, ran once in the beginning
        .setup =
            [](vuk::ExampleRunner& runner, vuk::Allocator& allocator)
        {
            // Pipelines are created by filling out a vuk::PipelineCreateInfo
            // In this case, we only need the shaders, we don't care about the rest of the state
            vuk::PipelineBaseCreateInfo pci;
            pci.add_glsl(util::read_entire_file("../src/shaders/triangle.vert"), "triangle.vert");
            pci.add_glsl(util::read_entire_file("../src/shaders/triangle.frag"), "triangle.frag");
            // The pipeline is stored with a user give name for simplicity
            runner.runtime->create_named_pipeline("triangle", pci);
        },
        // Code ran every frame
        .render =
            [](vuk::ExampleRunner&              runner,
               vuk::Allocator&                  frame_allocator,
               vuk::Value<vuk::ImageAttachment> target)
        {
            // The framework provides us with an image to render to in "target"
            // We attach this to the rendergraph named as "01_triangle"
            // The rendergraph is composed of passes (vuk::Pass)
            // Each pass declares which resources are used
            // And it provides a callback which is executed when this pass is being ran
            auto pass = vuk::make_pass(
                "01_triangle",
                [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eColorWrite) color_rt)
                {
                    command_buffer.set_viewport(0, vuk::Rect2D::framebuffer());
                    // Set the scissor area to cover the entire framebuffer
                    command_buffer.set_scissor(0, vuk::Rect2D::framebuffer());
                    command_buffer
                        .set_rasterization({})         // Set the default rasterization state
                        .set_color_blend(color_rt, {}) // Set the default color blend state
                        .bind_graphics_pipeline(
                            "triangle")    // Recall pipeline for "triangle" and bind
                        .draw(3, 1, 0, 0); // Draw 3 vertices
                    return color_rt;
                });

            auto drawn = pass(std::move(target));

            return drawn;
        }};

    REGISTER_EXAMPLE(x);
} // namespace

int main()
{
    vuk::ExampleRunner::get_runner().setup();
    vuk::ExampleRunner::get_runner().render();
    vuk::ExampleRunner::get_runner().cleanup();
}