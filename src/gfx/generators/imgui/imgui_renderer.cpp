#include "imgui_renderer.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/frame_manager.hpp"
#include "gfx/core/vulkan/instance.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <atomic>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <functional>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::generators::imgui
{
    ImguiRenderer::ImguiRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , font {nullptr}
        , menu_transfer_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .shader_path {"src/gfx/generators/imgui/menu_color_transfer.slang"},
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
        , light {
              .position_and_half_intensity_distance {17.5, -39.2, 32.4, 128.0}, .color_and_power {1.0, 1.0, 1.0, 156.0}}
    {
        util::send<voxel::GpuRaytracedLight>("UpdateLight", voxel::GpuRaytracedLight {light});

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

                    log::debug("Loaded Text font. Size: {}", buffer.size());

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
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->menu_transfer_pipeline));
        ImGui_ImplVulkan_Shutdown();
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void ImguiRenderer::renderIntoCommandBuffer(
        vk::CommandBuffer commandBuffer, const Camera& camera, const core::vulkan::Swapchain& swapchain)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();

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
                    allDescriptorsRepresentation += std::format(
                        "    {} @ {}{}\n",
                        dr.name,
                        dr.offset,
                        dr.maybe_size_bytes.has_value() ? std::format(" {}", util::bytesAsSiNamed(*dr.maybe_size_bytes))
                                                        : std::string {});
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

            // if (this->raw_animation_name_strings.empty())
            // {
            //     // TODO: have a version that warns on error
            //     std::optional v = util::receive<std::vector<std::string>>("AllAnimationNames");

            //     assert::critical(v.has_value(), "oop");

            //     this->owned_animation_name_strings = std::move(*v);

            //     int iters = 0;
            //     for (const std::string& s : this->owned_animation_name_strings)
            //     {
            //         if (s == "Cornel Box")
            //         {
            //             this->animation_combo_box_value = iters;
            //             util::send<u32>("SetAnimationNumber", static_cast<u32>(this->animation_combo_box_value));
            //         }
            //         this->raw_animation_name_strings.push_back(s.c_str());

            //         iters++;
            //     }
            // }

            // if (ImGui::Combo(
            //         "Animation",
            //         &this->animation_combo_box_value,
            //         raw_animation_name_strings.data(),
            //         static_cast<int>(raw_animation_name_strings.size())))
            // {
            //     util::send<u32>("SetAnimationNumber", static_cast<u32>(this->animation_combo_box_value));
            //     util::send<f32>("SetAnimationTime", 0.0f);
            // }

            // if (ImGui::Button("Restart Animation"))
            // {
            //     util::send<f32>("SetAnimationTime", 0.0f);
            // }

            if (this->owned_present_mode_strings.empty())
            {
                std::span<const vk::PresentModeKHR> presentModes = swapchain.getPresentModes();
                for (vk::PresentModeKHR p : presentModes)
                {
                    this->owned_present_mode_strings.push_back(vk::to_string(p));
                }

                for (const std::string& s : this->owned_present_mode_strings)
                {
                    this->raw_present_mode_strings.push_back(s.data());
                }

                const vk::PresentModeKHR activePresentMode = swapchain.getActivePresentMode();

                // I LOVE DEFICIENT STDLIB IMPLEMENTATIONS!
                const decltype(presentModes)::iterator activePresentModeIterator =
                    std::ranges::find(presentModes, activePresentMode);

                assert::critical(activePresentModeIterator != presentModes.end(), "literally how");

                this->present_mode_combo_box_value = static_cast<int>(activePresentModeIterator - presentModes.begin());
            }

            // ImGui::BeginCom

            if (ImGui::Combo(
                    "Present Mode",
                    &this->present_mode_combo_box_value,
                    raw_present_mode_strings.data(),
                    static_cast<int>(raw_present_mode_strings.size())))
            {
                std::span<const vk::PresentModeKHR> presentModes = swapchain.getPresentModes();

                const vk::PresentModeKHR newPresentMode =
                    presentModes[static_cast<usize>(this->present_mode_combo_box_value)];

                log::info(
                    "Requesting new Present mode {}",
                    raw_present_mode_strings.at(static_cast<usize>(this->present_mode_combo_box_value)));

                this->renderer->setDesiredPresentMode(newPresentMode);
            }

            bool needsUpdate = false;
            needsUpdate |=
                ImGui::DragFloat("Light x", &light.position_and_half_intensity_distance.x, 0.05f, -64.0f, 64.0f);
            needsUpdate |=
                ImGui::DragFloat("Light y", &light.position_and_half_intensity_distance.y, 0.05f, -64.0f, 64.0f);
            needsUpdate |=
                ImGui::DragFloat("Light z", &light.position_and_half_intensity_distance.z, 0.05f, -64.0f, 64.0f);
            needsUpdate |= ImGui::DragFloat("Color r", &light.color_and_power.x, 0.001f, 0.0f, 1.0f);
            needsUpdate |= ImGui::DragFloat("Color g", &light.color_and_power.y, 0.001f, 0.0f, 1.0f);
            needsUpdate |= ImGui::DragFloat("Color b", &light.color_and_power.z, 0.001f, 0.0f, 1.0f);
            needsUpdate |= ImGui::DragFloat("Light Power b", &light.color_and_power.w, 0.05f, 0.0f, 512.0f);
            needsUpdate |=
                ImGui::DragFloat("Light Radius b", &light.position_and_half_intensity_distance.w, 0.05f, 0.0f, 256.0f);

            if (needsUpdate)
            {
                util::send<voxel::GpuRaytracedLight>("UpdateLight", voxel::GpuRaytracedLight {light});
            }

            ImGui::PopStyleVar();
            ImGui::PopFont();

            ImGui::End();
        }

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(commandBuffer));
    }

    void ImguiRenderer::renderImageCopyIntoCommandBuffer(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->renderer->getPipelineManager()->getPipeline(this->menu_transfer_pipeline));

        commandBuffer.draw(3, 1, 0, 0);
    }
} // namespace gfx::generators::imgui