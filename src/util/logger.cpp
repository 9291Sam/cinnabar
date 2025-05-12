#include "logger.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <atomic>
#include <fmt/chrono.h>
#include <spdlog/async.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string_view>

namespace util
{
    namespace
    {
        class ThreadFlagFormatter : public spdlog::custom_flag_formatter
        {
        public:
            ThreadFlagFormatter()
                : next_thread_id {std::make_shared<std::atomic<u32>>(0)}
            {}

            void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
            {
                thread_local u32 thisThreadId = static_cast<u32>(-1);

                if (thisThreadId == static_cast<u32>(-1))
                {
                    thisThreadId = this->next_thread_id->fetch_add(1, std::memory_order_relaxed);
                }

                char integerFormatDigits[std::numeric_limits<u32>::digits10 + 3];

                const char* finishIter = std::format_to(&integerFormatDigits[0], "{}", thisThreadId);

                dest.append(&integerFormatDigits[0], finishIter);
            }

            std::unique_ptr<custom_flag_formatter> clone() const override
            {
                return spdlog::details::make_unique<ThreadFlagFormatter>(*this);
            }

        private:
            std::shared_ptr<std::atomic<u32>> next_thread_id;
        };

    } // namespace
    GlobalLoggerContext::GlobalLoggerContext()
    {
        spdlog::init_thread_pool(65536, 1);

        std::vector<spdlog::sink_ptr> sinks {
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                fmt::format("cinnabar_log {:%b %m-%d-%G %H-%M-%S} ", fmt::localtime(std::time(nullptr))), true),
        };

        spdlog::set_default_logger(
            std::make_shared<spdlog::async_logger>(
                "File and Stdout Logger",
                std::make_move_iterator(sinks.begin()),
                std::make_move_iterator(sinks.end()),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block));

        class SourceLocationFormatter : public spdlog::custom_flag_formatter
        {
        public:
            void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
            {
                using namespace std::literals;

                const std::string_view fileName {msg.source.filename};
                const int              fileLine {msg.source.line};

                const std::array<std::string_view, 2> identifiers {"src/"sv, "src\\"sv};

                std::size_t idx = std::string_view::npos;

                for (std::string_view i : identifiers)
                {
                    idx = fileName.find(i);

                    if (idx != std::string_view::npos)
                    {
                        break;
                    }
                }

                {
                    const std::string_view realFilenameView = fileName.substr(idx);

                    static constexpr usize MaxFilenameLength = 128;

                    std::array<char, MaxFilenameLength> buffer {};

                    assert::critical(
                        realFilenameView.size() < MaxFilenameLength,
                        "Tried to construct a filename string with length {}, which is larger than the maximum {}",
                        realFilenameView.size(),
                        MaxFilenameLength);

                    std::ranges::copy(realFilenameView, buffer.begin());

                    for (usize i = 0; i < realFilenameView.size(); ++i)
                    {
                        char& c = buffer[i];

                        if (c == '\\')
                        {
                            c = '/';
                        }
                    }

                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    dest.append(buffer.data(), buffer.data() + realFilenameView.size());
                }

                {
                    std::array<char, std::numeric_limits<int>::digits10 + 3> integerFormatDigits {};

                    const char* finishIter = std::format_to(integerFormatDigits.data(), ":{}", fileLine);

                    dest.append(integerFormatDigits.data(), finishIter);
                }
            }

            std::unique_ptr<custom_flag_formatter> clone() const override
            {
                return spdlog::details::make_unique<SourceLocationFormatter>();
            }
        };

        auto sourceLocationFormatter = std::make_unique<spdlog::pattern_formatter>();
        sourceLocationFormatter->add_flag<SourceLocationFormatter>('@');
        sourceLocationFormatter->add_flag<ThreadFlagFormatter>('t');
        sourceLocationFormatter->set_pattern("[%b %m/%d/%Y %H:%M:%S.%f] %^[%l @ %t]%$ [%@] %v");

        spdlog::set_formatter(std::move(sourceLocationFormatter));

        spdlog::set_level(spdlog::level::trace);

        log::info("Logger context startup");
    }

    GlobalLoggerContext::~GlobalLoggerContext()
    {
        log::info("Logger context shutdown");
        spdlog::shutdown();
    }
} // namespace util