
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <exception>
#include <fmt/format.h>
#include <source_location>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

static void trace()
{
    cpptrace::generate_trace().print();
}

void thrower()
{
    throw std::runtime_error {"234"};
}

namespace log
{

    template<class... Ts>
    struct info /* NOLINT */
    {
        info(/* NOLINT*/
             fmt::format_string<Ts...> fmt,
             Ts&&... args,
             const std::source_location& location = std::source_location::current())
        {
            spdlog::default_logger_raw()->log(
                spdlog::source_loc {
                    location.file_name(),
                    static_cast<int>(location.line()),
                    location.function_name()},
                spdlog::level::info,
                fmt,
                std::forward<Ts&&>(args)...);
        }
    };
    template<class... Ts> // NOLINTNEXTLINE
    info(fmt::format_string<Ts...>, Ts&&...) -> info<Ts...>;
    template<class... J> // NOLINTNEXTLINE
    info(fmt::format_string<J...>, J&&..., std::source_location) -> info<J...>;
} // namespace log

int main()
{
    spdlog::set_pattern("[%b %m/%d/%Y %H:%M:%S.%f] %^[%l @ %t]%$ [%@] %v");

    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);

    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");

    log::info("foo");

    CPPTRACE_TRYZ
    {
        thrower();
    }
    CPPTRACE_CATCHZ(const std::exception& e)
    {
        trace();
    }
    CPPTRACE_CATCH_ALT(...)
    {
        spdlog::info("{:<30}", "double");
        cpptrace::from_current_exception().print();
    }

    spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    spdlog::debug("This message should be displayed..");

    // change log pattern
}