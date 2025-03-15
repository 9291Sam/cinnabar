

#if defined(_WIN32)
#include <Windows.h>
#include <debugapi.h>
//
#include <Psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define NO_PANIC
#include "logger.hpp"
#include "util.hpp"
#include <cstdio>

namespace util
{

    std::vector<std::byte> loadEntireFileFromPath(const std::filesystem::path& path)
    {
        const std::string stringPath = path.string();

        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        std::FILE* file = std::fopen(stringPath.c_str(), "rb");
        assert::critical(file != nullptr, "Failed to load file |{}|", stringPath);

        std::vector<std::byte> buffer {};

        try
        {
            int maybeErrorCode = std::fseek(file, 0, SEEK_END);
            assert::critical(maybeErrorCode == 0, "Failed to SEEK_END on file path |{}|", stringPath);
            const long maybeSize = std::ftell(file); // NOLINT(google-runtime-int)
            assert::critical(maybeSize != 1L, "Failed to ftell on file {}", stringPath);
            const usize size = static_cast<usize>(maybeSize);

            buffer.resize(size);

            maybeErrorCode = std::fseek(file, 0, SEEK_SET);
            assert::critical(maybeErrorCode == 0, "Failed to SEEK_SET on file path |{}|", stringPath);
            const std::size_t freadBytes = std::fread(buffer.data(), sizeof(std::byte), buffer.size(), file);

            assert::critical(
                freadBytes == size,
                "Failed to load entire file {} requested, {} gathered. EOF: {}, ERROR: {}",
                size,
                freadBytes,
                std::feof(file),
                std::ferror(file));
        }
        catch (...)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
            const int maybeErrorCode = std::fclose(file);

            if (maybeErrorCode != 0)
            {
                log::critical("Failed to close file |{}|", stringPath);
            }

            throw;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        assert::critical(std::fclose(file) == 0, "Failed to close file |{}|", stringPath);

        return buffer;
    }

    std::size_t getMemoryUsage()
    {
#if defined(_WIN32)
        // Windows implementation
        PROCESS_MEMORY_COUNTERS memInfo;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo)))
        {
            return memInfo.WorkingSetSize;
        }
        return 0;

#elif defined(__APPLE__)
        // macOS implementation
        struct mach_task_basic_info info;
        mach_msg_type_number_t      infoCount = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &infoCount)
            == KERN_SUCCESS)
        {
            return info.resident_size;
        }
        return 0;

#elif defined(__linux__)
        // Linux implementation
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            return usage.ru_maxrss * 1024L; // ru_maxrss is in KB on Linux
        }
        return 0;

#else
        // Unsupported platform
        return 0;
#endif
    }

    namespace
    {
        const std::array<std::string_view, 11>& getSuffixes(SuffixType t)
        {
            static std::array<std::string_view, 11> full {
                "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "RiB", "QiB"};

            static std::array<std::string_view, 11> tiny {"B", "K", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};

            static std::array<std::string_view, 11> null {};

            switch (t)
            {
            case SuffixType::Full:
                return full;
            case SuffixType::Short:
                return tiny;
            default:
                return null;
            }
        }
    } // namespace

    std::string bytesAsSiNamed(std::size_t bytes, SuffixType t)
    {
        return bytesAsSiNamed(static_cast<long double>(bytes), t);
    }

    std::string bytesAsSiNamed(long double bytes, SuffixType type)
    {
        const long double unit = 1024.0;

        switch (std::fpclassify(bytes))
        {
        case FP_INFINITE:
            return "Infinity Bytes";
        case FP_NAN:
            return "NaN Bytes";
        case FP_ZERO:
            return "0 B";
        default:
            return "unknown";
        case FP_NORMAL:
            [[fallthrough]];
        case FP_SUBNORMAL:
            break;
        }

        const long double base  = std::log10(bytes) / std::log10(unit);
        const long double value = std::pow(unit, base - std::floor(base));
        std::string       prefix {std::format("{:.3f}", value)};

        if (std::string_view {prefix}.ends_with(".0"))
        {
            prefix.resize(prefix.size() - 2);
        }

        const std::string_view suffix = getSuffixes(type).at(static_cast<std::size_t>(std::floor(base)));

        prefix.push_back(' ');
        prefix.append(suffix);

        for (char& c : prefix)
        {
            if (c == '\0')
            {
                c = '?';
            }
        }

        return prefix;
    }

} // namespace util
