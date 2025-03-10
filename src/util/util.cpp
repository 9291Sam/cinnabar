#include "util.hpp"
#include "logger.hpp"
#include "util/logger.hpp"
#include <cstdio>

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
