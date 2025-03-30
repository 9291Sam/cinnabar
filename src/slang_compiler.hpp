#ifndef CFI_SHADER_COMPILER_HPP
#define CFI_SHADER_COMPILER_HPP

#include "util/util.hpp"
#include <filesystem>
#include <optional>
#include <slang.h>
#include <string_view>

namespace cfi
{

    class shader_compiler
    {
    public:
        explicit shader_compiler(std::filesystem::path cache_path = {});

        [[nodiscard]] std::vector<std::byte> compile(const std::string_view module_name);

    private:
        static void destroy_slang(ISlangUnknown* object)
        {
            object->release();
        }
        template<typename T>
        using slang_unique_ptr = std::unique_ptr<T, decltype(&destroy_slang)>;

        std::optional<std::filesystem::path> cache_path;

        slang_unique_ptr<slang::IGlobalSession> global_session = {nullptr, destroy_slang};
        slang_unique_ptr<slang::ISession>       session        = {nullptr, destroy_slang};

        // std::vector<std::filesystem::path> search_paths =
        // {util::getCanonicalPathOfShaderFile("src/gfx/shader_common")};

        [[nodiscard]] slang_unique_ptr<slang::IModule> load_module(const std::string_view module_name);

        [[nodiscard]] slang_unique_ptr<slang::IEntryPoint>
        find_entry_point(slang::IModule* module, const std::string_view entry_point_name);

        [[nodiscard]] slang_unique_ptr<slang::IComponentType>
        create_composed_program(slang::IModule* module, slang::IEntryPoint* entry_point);

        [[nodiscard]] slang_unique_ptr<slang::IBlob> compile_to_spirv(slang::IComponentType* composed_program);

        [[noreturn]] void throw_error(const std::string_view context, slang::IBlob* diagnostic_blob);

        [[noreturn]] void throw_error(const std::string_view context);
    };

} // namespace cfi

#endif // CFI_SHADER_COMPILER_HPP