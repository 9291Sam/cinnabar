
#include <glm/ext/scalar_common.hpp>
#include <random>
//

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "gfx/core/vulkan/instance.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/frame_generator.hpp"
#include "gfx/renderables/triangle/triangle_renderer.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cstddef>
#include <glm/trigonometric.hpp>
#include <iterator>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

// std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> loadShaders()
// {
//     shaderc::CompileOptions options {};
//     options.SetSourceLanguage(shaderc_source_language_glsl);
//     options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
//     options.SetTargetSpirv(shaderc_spirv_version_1_5);
//     options.SetOptimizationLevel(shaderc_optimization_level_performance);
//     options.SetGenerateDebugInfo();

//     shaderc::Compiler             compiler {};
//     shaderc::SpvCompilationResult vertexShaderResult =
//         compiler.CompileGlslToSpv(vertexShader, shaderc_vertex_shader, "", options);
//     shaderc::SpvCompilationResult fragmentShaderResult =
//         compiler.CompileGlslToSpv(fragmentShader, shaderc_fragment_shader, "", options);

//     assert::critical(vertexShaderResult.GetCompilationStatus() == shaderc_compilation_status_success, "oops v");
//     if (fragmentShaderResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     {
//         assert::critical(false, "{}", fragmentShaderResult.GetErrorMessage());
//     }

//     std::vector<u32> vertexData {std::from_range, vertexShaderResult};
//     std::vector<u32> fragmentData {std::from_range, fragmentShaderResult};

//     assert::critical(!vertexData.empty(), "Vertex Size: {}", vertexData.size());
//     assert::critical(!fragmentData.empty(), "Fragment Size: {}", fragmentData.size());

//     std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> out {};

//     out[vk::ShaderStageFlagBits::eVertex]   = std::move(vertexData);
//     out[vk::ShaderStageFlagBits::eFragment] = std::move(fragmentData);

//     return out;

//     // if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     // {
//     //     log::error(
//     //         "compile failed: {} {}",
//     //         static_cast<int>(compileResult.GetCompilationStatus()),
//     //         compileResult.GetErrorMessage());
//     // }
//     // else
//     // {
//     //     std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

//     //     std::array<char, 16> hexChars {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
//     //     'F'}; std::string          result {}; result.reserve(data.size() * 6);

//     //     for (char d : data)
//     //     {
//     //         const u32 c = static_cast<u8>(d);

//     //         result.push_back('0');
//     //         result.push_back('x');
//     //         result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
//     //         result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
//     //         result.push_back(',');
//     //         result.push_back(' ');
//     //     }

//     //     if (!result.empty())
//     //     {
//     //         result.pop_back();
//     //         result.pop_back();
//     //     }

//     //     log::info("{}", result);
//     // }
// }

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

        gfx::core::Renderer renderer {};

        gfx::renderables::triangle::TriangleRenderer triangleRenderer {&renderer};

        const float fovY = glm::radians(70.0f);

        gfx::Camera camera {gfx::Camera::CameraDescriptor {.fov_y {fovY}}};

        std::vector<gfx::renderables::triangle::TriangleRenderer::Triangle> triangles {};

        std::mt19937                          gen {std::random_device {}()};
        std::uniform_real_distribution<float> dist {-32.0f, 32.0f};

        auto getRandVec3 = [&]
        {
            return glm::vec3 {
                dist(gen),
                dist(gen),
                dist(gen),
            };
        };

        for (int i = 0; i < 38; ++i)
        {
            triangles.push_back(triangleRenderer.createTriangle(
                {.translation {glm::vec4 {getRandVec3(), 1.0f}}, .scale {10.0f, 10.0f, 10.0f}}));
        }

        gfx::FrameGenerator frameGenerator {&renderer};

        while (!renderer.shouldWindowClose())
        {
            const vk::Extent2D frameBufferSize = renderer.getWindow()->getFramebufferSize();

            const float aspectRatio =
                static_cast<float>(frameBufferSize.width) / static_cast<float>(frameBufferSize.height);

            if (frameGenerator.hasResizeOcurred())
            {
                camera = gfx::Camera {gfx::Camera::CameraDescriptor {
                    .aspect_ratio {aspectRatio},
                    .position {camera.getPosition()},
                    .fov_y {fovY},
                    .pitch {camera.getPitch()},
                    .yaw {camera.getYaw()}}};
            }

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::ToggleCursorAttachment))
            {
                renderer.getWindow()->toggleCursor();
            }

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::CloseWindow))
            {
                break;
            }

            glm::vec3 previousPosition = camera.getPosition();

            glm::vec3 newPosition = previousPosition;

            const float deltaTime = renderer.getWindow()->getDeltaTimeSeconds();

            const float moveScale = 32.0f;

            // Adjust the new position based on input for movement directions
            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveForward))
            {
                newPosition += camera.getForwardVector() * deltaTime * moveScale;
            }
            else if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveBackward))
            {
                newPosition -= camera.getForwardVector() * deltaTime * moveScale;
            }

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveLeft))
            {
                newPosition -= camera.getRightVector() * deltaTime * moveScale;
            }
            else if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveRight))
            {
                newPosition += camera.getRightVector() * deltaTime * moveScale;
            }

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveUp))
            {
                newPosition += gfx::Transform::UpVector * deltaTime * moveScale;
            }
            else if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveDown))
            {
                newPosition -= gfx::Transform::UpVector * deltaTime * moveScale;
            }

            camera.setPosition(newPosition);

            const float rotateSpeedScale = 6.0f;

            auto getMouseDeltaRadians = [&]
            {
                // each value from -1.0 -> 1.0 representing how much it moved
                // on the screen
                const auto [nDeltaX, nDeltaY] = renderer.getWindow()->getScreenSpaceMouseDelta();

                const auto deltaRadiansX = (nDeltaX / 2) * aspectRatio * fovY;
                const auto deltaRadiansY = (nDeltaY / 2) * fovY;

                return gfx::core::Window::Delta {.x {deltaRadiansX}, .y {deltaRadiansY}};
            };

            auto [xDelta, yDelta] = getMouseDeltaRadians();

            camera.addYaw(xDelta * rotateSpeedScale);
            camera.addPitch(yDelta * rotateSpeedScale);

            frameGenerator.renderFrame({.maybe_triangle_renderer {&triangleRenderer}}, camera);
        }

        for (auto& t : triangles)
        {
            triangleRenderer.destroyTriangle(std::move(t));
        }

        renderer.getDevice()->getDevice().waitIdle();
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
