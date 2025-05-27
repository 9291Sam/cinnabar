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
#include "implot.h"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <algorithm>
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
        , menu_transfer_pipeline {this->renderer->getPipelineManager()->createPipelineUnique(
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
        , light {.position_and_half_intensity_distance {11.5, 17.5, 32.4, 8.0}, .color_and_power {1.0, 1.0, 1.0, 0.25}}
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
        ImPlot::CreateContext();

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
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void ImguiRenderer::renderIntoCommandBuffer(
        vk::CommandBuffer commandBuffer, const Camera& camera, const core::vulkan::Swapchain& swapchain)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();
        // ImPlot::ShowDemoWindow();

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
                R"(ん✨ち🍋😍🐶🖨🖨🐱🦊🐼🐻🐘🦒🦋🌲🌸🌞🌈
Ram Usage: {}
Vram Usage: {}
Addressable Vram: {}
FPS: {}{} / {}ms
{}
{})",
                util::bytesAsSiNamed(util::getMemoryUsage()),
                util::bytesAsSiNamed(gfx::core::vulkan::bufferBytesAllocated.load(std::memory_order_acquire)),
                util::bytesAsSiNamed(
                    gfx::core::vulkan::hostVisibleBufferBytesAllocated.load(std::memory_order_acquire)),
                getDeltaTimeEmoji(deltaTime),
                1.0f / deltaTime,
                deltaTime * 1000.0f,
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

            if (ImGui::Button("Restart Hash Map"))
            {
                util::send<bool>("CLEAR_FACE_HASH_MAP", true);
            }

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
                ImGui::DragFloat("Light x", &light.position_and_half_intensity_distance.x, 0.05f, -128.0f, 128.0f);
            needsUpdate |=
                ImGui::DragFloat("Light y", &light.position_and_half_intensity_distance.y, 0.05f, -128.0f, 128.0f);
            needsUpdate |=
                ImGui::DragFloat("Light z", &light.position_and_half_intensity_distance.z, 0.05f, -128.0f, 128.0f);
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

            if (ImGui::Checkbox("Enable Reflections", &this->are_reflections_enabled))
            {
                util::send<bool>("SETTING_ENABLE_REFLECTIONS", this->are_reflections_enabled);
            }

            if (std::optional timestampNames =
                    util::receive<std::array<std::string_view, core::vulkan::MaxQueriesPerFrame>>(
                        "GPU_TIMESTAMP_NAMES"))
            {
                std::ranges::transform(
                    timestampNames->cbegin(),
                    timestampNames->cend(),
                    this->owned_gpu_timestamp_names.begin(),
                    [](std::string_view name)
                    {
                        return std::string {name};
                    });

                std::ranges::transform(
                    this->owned_gpu_timestamp_names.cbegin(),
                    this->owned_gpu_timestamp_names.cend(),
                    this->raw_gpu_timestamp_names.begin(),
                    [](const std::string& name)
                    {
                        return name.c_str();
                    });
            }

            if (std::optional timestamps =
                    util::receive<std::array<u64, core::vulkan::MaxQueriesPerFrame>>("GPU_PER_FRAME_TIMESTAMPS"))
            {
                u64 total = std::accumulate(timestamps->cbegin(), timestamps->cend(), u64 {0});

                std::ranges::transform(
                    timestamps->cbegin(),
                    timestamps->cend(),
                    this->most_recent_timestamps.begin(),
                    [&](u64 v)
                    {
                        return static_cast<f32>(v) / static_cast<f32>(total);
                    });
            }

            const f32 plotSizePx = std::floor((desiredConsoleSize.x - (2 * WindowPadding)) / 2.0);

            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_LegendPadding, ImVec2(0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_LegendInnerPadding, ImVec2(0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_LegendSpacing, ImVec2(0, 0));
            ImPlot::PushStyleColor(ImPlotCol_FrameBg, {0, 0, 0, 0});
            ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
            // ImPlot::PushColormap(ImPlotColormap_Pastel);

            const ImPlotStyle& imPlotStyle = ImPlot::GetStyle();

            const f32 plotTitleSize = imPlotStyle.LabelPadding.y;

            u32 numberOfTextElements = 0;

            for (int i = this->most_recent_timestamps.size() - 1; i > 0; --i)
            {
                if (this->most_recent_timestamps[i] != 0)
                {
                    numberOfTextElements = i;
                    break;
                }
            }

            if (ImPlot::BeginPlot(
                    "Gpu Frame Times##Pie1",
                    ImVec2(
                        plotSizePx,
                        plotSizePx + plotTitleSize
                            + static_cast<f32>(ImGui::CalcTextSize("Gpu Frame Times", nullptr, true).y)
                            + imPlotStyle.LegendPadding.y + (2.0 * imPlotStyle.LegendInnerPadding.y)
                            + (numberOfTextElements * ImGui::CalcTextSize("fuck me", nullptr, true).y)),
                    ImPlotFlags_Equal | ImPlotFlags_NoMouseText))
            {
                ImPlot::SetupLegend(ImPlotLocation_South | ImPlotLocation_Center, ImPlotLegendFlags_Outside);
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(
                    this->raw_gpu_timestamp_names.data() + 1,
                    this->most_recent_timestamps.data() + 1,
                    numberOfTextElements,
                    0.5,
                    0.5,
                    0.5,
                    "",
                    0,
                    ImPlotPieChartFlags_Normalize);
                ImPlot::EndPlot();
            }

            ImGui::SameLine(0, 0);

            static const char* labels2[] = {"A", "B", "C", "D", "E"};
            static int         data2[]   = {1, 1, 2, 3, 5};

            if (ImPlot::BeginPlot(
                    "fff##Pie2",
                    ImVec2(plotSizePx, plotSizePx + plotTitleSize + ImGui::CalcTextSize("fff", nullptr, true).y),
                    ImPlotFlags_NoLegend | ImPlotFlags_Equal | ImPlotFlags_NoMouseText))
            {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(labels2, data2, 5, 0.5, 0.5, 0.49, "%.0f", 180, ImPlotPieChartFlags_Normalize);
                ImPlot::EndPlot();
            }
            // ImPlot::PopColormap();
            ImPlot::PopStyleColor(2);
            ImPlot::PopStyleVar(4);

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