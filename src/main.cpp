
#include "util.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <fmt/chrono.h>
#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <span>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string_view>

int main()
{
    spdlog::init_thread_pool(65536, 1);

    std::vector<spdlog::sink_ptr> sinks {
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            fmt::format(
                "cinnabar_log {:%b %m-%d-%G %H-%M-%S} ", fmt::localtime(std::time(nullptr))),
            true),
    };

    spdlog::set_default_logger(std::make_shared<spdlog::async_logger>(
        "File and Stdout Logger",
        std::make_move_iterator(sinks.begin()),
        std::make_move_iterator(sinks.end()),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block));

    spdlog::set_pattern("[%b %m/%d/%Y %H:%M:%S.%f] %^[%l @ %t]%$ [%@] %v");
    spdlog::set_level(spdlog::level::trace);

    CPPTRACE_TRYZ
    {
        shaderc::CompileOptions options {};
        options.SetTargetSpirv(shaderc_spirv_version_1_0);

        std::string string = R"(
#version 460

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_quad : require

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer DataBuffer {
    uint data[];
};

void main() {
    uint idx = gl_GlobalInvocationID.x;
    data[idx] += 1;
}

        )";

        shaderc::Compiler compiler {};
        auto              result =
            compiler.CompileGlslToSpvAssembly(string, shaderc_compute_shader, "", options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            log::error(
                "compile failed: {} {}",
                static_cast<int>(result.GetCompilationStatus()),
                result.GetErrorMessage());
        }
        else
        {
            std::span<const char> data {result.cbegin(), result.cend()};

            std::array<char, 16> hexChars {
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            std::string result {};
            result.reserve(data.size() * 6);

            for (char c : data)
            {
                result.push_back('0');
                result.push_back('x');
                result += hexChars[(c & 0xF0) >> 4];
                result += hexChars[(c & 0x0F) >> 0];
                result.push_back(',');
                result.push_back(' ');
            }

            if (!result.empty())
            {
                result.pop_back();
                result.pop_back();
            }

            log::info("{}", result);
        }

        log::info("cinnabar!");
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

    spdlog::shutdown();
}
