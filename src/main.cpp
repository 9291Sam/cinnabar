
#include "log.hpp"
#include <chrono>
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <fmt/chrono.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
        assert::error(false, "true!");
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

    spdlog::shutdown();
}
