#include "imgui_renderer.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/frame_manager.hpp"
#include "gfx/core/vulkan/instance.hpp"
#include "gfx/core/window.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <atomic>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <cstdio>
#include <fstream>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <iterator>
#include <misc/freetype/imgui_freetype.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::generators::imgui
{
    ImguiRenderer::ImguiRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , font {nullptr}
        , menu_transfer_pipeline {
              this->renderer->getPipelineManager()->createGraphicsPipeline(core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/imgui/menu_color_transfer.vert"},
                  .fragment_shader_path {"src/gfx/generators/imgui/menu_color_transfer.frag"},
                  .topology {vk::PrimitiveTopology::eTriangleList}, // remove
                  .polygon_mode {vk::PolygonMode::eFill},           // replace with dynamic state
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eCounterClockwise}, // remove
                  .depth_test_enable {vk::False},
                  .depth_write_enable {vk::False},
                  .depth_compare_op {}, // remove
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat}, // remove lmao?
                  .blend_enable {vk::True},
                  .name {"Menu Color Transfer"},
              })}
    {
        const std::array availableDescriptors {
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eSampler}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eCombinedImageSampler}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eSampledImage}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eStorageImage}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eUniformTexelBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eStorageTexelBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eUniformBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eStorageBuffer}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eUniformBufferDynamic}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eStorageBufferDynamic}, .descriptorCount {1024}},
            vk::DescriptorPoolSize {.type {vk::DescriptorType::eInputAttachment}, .descriptorCount {1024}}};

        const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
            .sType {vk::StructureType::eDescriptorPoolCreateInfo},
            .pNext {nullptr},
            .flags {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet},
            .maxSets {4096},
            .poolSizeCount {static_cast<u32>(availableDescriptors.size())},
            .pPoolSizes {availableDescriptors.data()},
        };

        this->imgui_descriptor_pool =
            this->renderer->getDevice()->getDevice().createDescriptorPoolUnique(descriptorPoolCreateInfo);

        ImGui::CreateContext();

        // Thank you karnage for this absolute nastiness
        auto getFn = [this](const char* name)
        {
            return this->renderer->getInstance()->getProcAddress<PFN_vkVoidFunction>(name);
        };
        auto lambda = +[](const char* name, void* ud)
        {
            const auto* gf = reinterpret_cast<decltype(getFn)*>(ud); // NOLINT

            void (*ptr)() = nullptr;

            ptr = (*gf)(name);

            // HACK: sometimes function ending in KHR are removed if they exist in core, ignore
            if (std::string_view nameView {name}; ptr == nullptr && nameView.ends_with("KHR"))
            {
                nameView.remove_suffix(3);

                std::string apiString {nameView};

                ptr = (*gf)(apiString.c_str());
            }

            assert::critical(ptr != nullptr, "Failed to load {}", name);

            return ptr;
        };
        ImGui_ImplVulkan_LoadFunctions(lambda, &getFn);

        this->renderer->getWindow()->initializeImgui();

        this->renderer->getDevice()->acquireQueue(
            gfx::core::vulkan::Device::QueueType::Graphics,
            [&](vk::Queue q)
            {
                const VkFormat colorFormat = static_cast<VkFormat>(vk::Format::eB8G8R8A8Unorm);

                ImGui_ImplVulkan_InitInfo initInfo {
                    .Instance {**this->renderer->getInstance()},
                    .PhysicalDevice {this->renderer->getDevice()->getPhysicalDevice()},
                    .Device {this->renderer->getDevice()->getDevice()},
                    .QueueFamily {this->renderer->getDevice()
                                      ->getFamilyOfQueueType(gfx::core::vulkan::Device::QueueType::Graphics)
                                      .value()},
                    .Queue {std::bit_cast<VkQueue>(q)},
                    .DescriptorPool {*this->imgui_descriptor_pool},
                    .RenderPass {nullptr},
                    .MinImageCount {gfx::core::vulkan::FramesInFlight},
                    .ImageCount {gfx::core::vulkan::FramesInFlight},
                    .MSAASamples {VK_SAMPLE_COUNT_1_BIT},
                    .PipelineCache {nullptr},
                    .Subpass {0},
                    .DescriptorPoolSize {0},
                    .UseDynamicRendering {true},
                    .PipelineRenderingCreateInfo {
                        .sType {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO},
                        .pNext {nullptr},
                        .viewMask {0},
                        .colorAttachmentCount {1},
                        .pColorAttachmentFormats {&colorFormat},
                        .depthAttachmentFormat {},
                        .stencilAttachmentFormat {},
                    },
                    .Allocator {nullptr},
                    .CheckVkResultFn {
                        [](VkResult err)
                        {
                            assert::warn(
                                err == VK_SUCCESS, "Imgui ResultCheckFailed {}", vk::to_string(vk::Result {err}));
                        }},
                    .MinAllocationSize {UINT64_C(1024) * 1024}};

                ImGui_ImplVulkan_Init(&initInfo);

                ImGuiIO& io = ImGui::GetIO();

                io.IniFilename = nullptr;
                io.LogFilename = nullptr;

                {
                    static std::array<ImWchar, 3> unifontRanges {0x0001, 0xFFFF, 0};
                    ImFontConfig                  fontConfigUnifont;
                    fontConfigUnifont.OversampleH          = 2;
                    fontConfigUnifont.OversampleV          = 2;
                    fontConfigUnifont.RasterizerDensity    = 2.0f;
                    fontConfigUnifont.MergeMode            = false;
                    fontConfigUnifont.SizePixels           = 64;
                    fontConfigUnifont.FontDataOwnedByAtlas = false;

                    const std::filesystem::path unifontPath =
                        util::getCanonicalPathOfShaderFile("res/unifont-16.0.01.otf");

                    std::vector<std::byte> buffer = util::loadEntireFileFromPath(unifontPath);

                    log::trace("Loaded Text font. Size: {}", buffer.size());

                    this->font = io.Fonts->AddFontFromMemoryTTF(
                        buffer.data(), // NOLINT
                        static_cast<int>(buffer.size()),
                        16,
                        &fontConfigUnifont,
                        unifontRanges.data());
                }

                {
                    static std::array<ImWchar, 3> ranges {0x00001, 0x1FFFF, 0};
                    static ImFontConfig           cfg;

                    cfg.OversampleH       = 1;
                    cfg.OversampleV       = 1;
                    cfg.RasterizerDensity = 2.0f;
                    cfg.MergeMode         = true;
                    cfg.FontBuilderFlags |=
                        (ImGuiFreeTypeBuilderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap); // NOLINT
                    cfg.SizePixels           = 64;
                    cfg.FontDataOwnedByAtlas = false;
                    // cfg.GlyphOffset          = ImVec2(0.0, -2.0);

                    const std::filesystem::path emojiPath =
                        util::getCanonicalPathOfShaderFile("res/OpenMoji-color-colr1_svg.ttf");

                    std::vector<std::byte> buffer = util::loadEntireFileFromPath(emojiPath);

                    log::trace("Loaded Emojis. Size: {}", buffer.size());

                    this->font = io.Fonts->AddFontFromMemoryTTF(
                        buffer.data(), // NOLINT
                        static_cast<int>(buffer.size()),
                        16.0f,
                        &cfg,
                        ranges.data());
                }

                io.Fonts->Build();

                ImGui_ImplVulkan_CreateFontsTexture();
            });
    }

    ImguiRenderer::~ImguiRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->menu_transfer_pipeline));
        ImGui_ImplVulkan_Shutdown();
    }

    void ImguiRenderer::renderIntoCommandBuffer(
        vk::CommandBuffer commandBuffer,
        const Camera&     camera,
        core::vulkan::DescriptorHandle<vk::DescriptorType::eUniformBuffer>)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* const viewport = ImGui::GetMainViewport();

        const auto [x, y] = viewport->Size;

        const ImVec2 desiredConsoleSize {2 * x / 9, y}; // 2 / 9 is normal

        ImGui::SetNextWindowPos(ImVec2 {std::ceil(viewport->Size.x - desiredConsoleSize.x), 0});
        ImGui::SetNextWindowSize(desiredConsoleSize);

        const float deltaTime = renderer->getWindow()->getDeltaTimeSeconds();

        auto getDeltaTimeEmoji = [](const float dt) -> std::string_view
        {
            using namespace std::literals;

            const float fps = 1.0f / dt;

            if (fps >= 60)
            {
                return "🚀"sv;
            }
            if (fps >= 30)
            {
                return "⏳"sv;
            }
            if (fps >= 15)
            {
                return "❄️"sv;
            }
            return "💀"sv;
        };

        if (ImGui::Begin(
                "Console",
                nullptr,
                ImGuiWindowFlags_NoResize |            // NOLINT
                    ImGuiWindowFlags_NoSavedSettings | // NOLINT
                    ImGuiWindowFlags_NoMove |          // NOLINT
                    ImGuiWindowFlags_NoDecoration))    // NOLINT
        {
            static constexpr float WindowPadding = 5.0f;

            // assert::critical(this->font != nullptr, "oopers");

            ImGui::PushFont(this->font);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(WindowPadding, WindowPadding));

            // auto facesVisible = numberOfFacesVisible.load();
            // auto facesOnGpu     = numberOfFacesOnGpu.load();
            // auto facesPossible  = numberOfFacesPossible.load();
            // auto facesAllocated = numberOfFacesAllocated.load();

            // auto chunks         = numberOfChunksAllocated.load();
            // auto chunksPossible = numberOfChunksPossible.load();

            // auto bricks         = numberOfBricksAllocated.load();
            // auto bricksPossible = numberOfBricksPossible.load();

            // auto fly = flySpeed.load();

            // f32 tickDeltaTime = std::bit_cast<f32>(f32TickDeltaTime.load());

            // const std::string menuText = std::format(
            //     "ん✨ち🍋😍🐶🖨🖨🐱🦊🐼🐻🐘🦒🦋🌲🌸🌞🌈\n"
            //     "Camera: {{{:.3f}, {:.3f}, {:.3f}}} {:.3f} {:.3f}\n"
            //     "FPS: {:.3f} / {:.3f}ms\n"
            //     "TPS: {} / {:.3f}ms\n"
            //     "Ram: {}\n"
            //     "Vram: {}\n"
            //     "Staging Usage: {}\n"
            //     "Chunks {} / {} | {:.3f}%\n"
            //     "Bricks {} / {} | {:.3f}%\n"
            //     "Faces {} / {} / {} / {} | {:.3f}%\n"
            //     "Fly Speed {}",
            //     camera.getPosition().x,
            //     camera.getPosition().y,
            //     camera.getPosition().z,
            //     camera.getPitch(),
            //     camera.getYaw(),
            //     1.0f / deltaTime,
            //     1000.0f * deltaTime,
            //     1.0f / tickDeltaTime,
            //     1000.0f * tickDeltaTime,
            //     util::bytesAsSiNamed(util::getMemoryUsage()),
            //     util::bytesAsSiNamed(gfx::core::vulkan::bufferBytesAllocated.load(std::memory_order_relaxed)),
            //     util::bytesAsSiNamed(renderer->getStager().getUsage().first),

            //     chunks,
            //     chunksPossible,
            //     100.0f * static_cast<float>(chunks) / static_cast<float>(chunksPossible),
            //     bricks,
            //     bricksPossible,
            //     100.0f * static_cast<float>(bricks) / static_cast<float>(bricksPossible),
            //     facesVisible,
            //     facesOnGpu,
            //     facesAllocated,
            //     facesPossible,
            //     100.0f * static_cast<float>(facesVisible) / static_cast<float>(facesPossible),
            //     fly);

            std::map<vk::DescriptorType, std::vector<core::vulkan::DescriptorManager::DescriptorReport>>
                allDescriptors = this->renderer->getDescriptorManager()->getAllDescriptorsDebugInfo();

            std::string allDescriptorsRepresentation {};

            for (const auto& [descriptorType, descriptors] : allDescriptors)
            {
                allDescriptorsRepresentation += std::format(
                    "{} ({})\n",
                    vk::to_string(descriptorType),
                    gfx::core::vulkan::getShaderBindingLocation(descriptorType));

                for (auto dr : descriptors)
                {
                    allDescriptorsRepresentation += std::format("    {} @ {}\n", dr.name, dr.offset);
                }
            }

            if (!allDescriptorsRepresentation.empty())
            {
                // removes trailing '\n'
                allDescriptorsRepresentation.pop_back();
            }

            const std::string menuText = std::format(
                "ん✨ち🍋😍🐶🖨🖨🐱🦊🐼🐻🐘🦒🦋🌲🌸🌞🌈\nRAM Usage: {} | VRAM Usage: {}\nFPS {}: {}\n{}\n{}",
                util::bytesAsSiNamed(util::getMemoryUsage()),
                util::bytesAsSiNamed(gfx::core::vulkan::bufferBytesAllocated.load(std::memory_order_acquire)),
                getDeltaTimeEmoji(deltaTime),
                1.0f / deltaTime,
                glm::to_string(camera.getPosition()),
                allDescriptorsRepresentation);

            ImGui::TextWrapped("%s", menuText.c_str()); // NOLINT

            // this->tracing_graph->loadFrameData(tasks);
            // this->tracing_graph->renderTimings(
            //     static_cast<std::size_t>(desiredConsoleSize.x - (2 * WindowPadding)), 0, 256, 0);

            if (ImGui::Button("Attach Cursor"))
            {
                renderer->getWindow()->attachCursor();
            }

            ImGui::PopStyleVar();
            ImGui::PopFont();

            ImGui::End();
        }

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
    }

    void ImguiRenderer::renderImageCopyIntoCommandBuffer(
        vk::CommandBuffer                                                 commandBuffer,
        core::vulkan::DescriptorHandle<vk::DescriptorType::eStorageImage> menuTransferFromImage)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->renderer->getPipelineManager()->getPipeline(this->menu_transfer_pipeline));

        commandBuffer.pushConstants<u32>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
            vk::ShaderStageFlagBits::eAll,
            0,
            static_cast<u32>(menuTransferFromImage.getOffset()));

        commandBuffer.draw(3, 1, 0, 0);
    }
} // namespace gfx::generators::imgui