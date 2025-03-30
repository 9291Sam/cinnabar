#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <fmt/format.h>

namespace cfi
{

    shader_compiler::shader_compiler(std::filesystem::path cache_path)
        : cache_path(cache_path.empty() ? std::nullopt : std::make_optional(cache_path))
    {
        const auto                  slang_global_session_description = SlangGlobalSessionDesc {};
        slang::IGlobalSession*      global_session_ptr               = nullptr;
        [[maybe_unused]] const auto result =
            slang::createGlobalSession(&slang_global_session_description, &global_session_ptr);
        global_session = slang_unique_ptr<slang::IGlobalSession>(global_session_ptr, destroy_slang);

        auto paths_as_c_strings = std::vector<const char*>();
        for (const auto& path : search_paths)
        {
            paths_as_c_strings.push_back(path.c_str());
        }

        auto profile = global_session->findProfile("spirv_1_4");

        auto target = slang::TargetDesc {
            .format  = SlangCompileTarget::SLANG_SPIRV,
            .profile = profile,
            .flags   = 0,
        };

        auto slang_session_descriptor            = slang::SessionDesc {};
        slang_session_descriptor.searchPaths     = paths_as_c_strings.data();
        slang_session_descriptor.searchPathCount = static_cast<int>(paths_as_c_strings.size());
        slang_session_descriptor.targets         = &target;
        slang_session_descriptor.targetCount     = 1;

        slang::ISession* session_ptr = nullptr;
        global_session->createSession(slang_session_descriptor, &session_ptr);
        session = slang_unique_ptr<slang::ISession>(session_ptr, destroy_slang);
    }

    std::vector<std::byte> shader_compiler::compile(const std::string_view module_name)
    {
        const auto initial_path = util::getCanonicalPathOfShaderFile(module_name);

        log::trace("{}", initial_path.generic_string());

        const auto module           = load_module(initial_path.generic_string());
        const auto entry_point      = find_entry_point(module.get(), "main");
        const auto composed_program = create_composed_program(module.get(), entry_point.get());
        const auto spirv_blob       = compile_to_spirv(composed_program.get());

        auto size = spirv_blob->getBufferSize();
        size      = (size + 3) & ~3; // Round up to the nearest multiple of 4
        auto data = std::vector<std::byte>(size);
        std::memcpy(data.data(), spirv_blob->getBufferPointer(), data.size());

        return data;
    }

    shader_compiler::slang_unique_ptr<slang::IModule> shader_compiler::load_module(const std::string_view module_name)
    {
        slang::IBlob* module_blob_ptr = nullptr;
        auto          module          = session->loadModule(module_name.data(), &module_blob_ptr);
        auto          module_blob     = slang_unique_ptr<slang::IBlob>(module_blob_ptr, destroy_slang);

        if (!module)
        {
            throw_error("loading module", module_blob.get());
        }
        return slang_unique_ptr<slang::IModule>(module, destroy_slang);
    }

    void shader_compiler::throw_error(const std::string_view context, slang::IBlob* diagnostic_blob)
    {
        const auto message = fmt::format("{}: {}", context, diagnostic_blob->getBufferPointer());
        throw std::runtime_error(message);
    }
    shader_compiler::slang_unique_ptr<slang::IEntryPoint>
    shader_compiler::find_entry_point(slang::IModule* module, const std::string_view entry_point_name)
    {
        for (int i = 0; i < module->getDependencyFileCount(); ++i)
        {
            const char* const dep = module->getDependencyFilePath(i);

            log::trace("dep {}", dep);
        }
        slang::IEntryPoint* entry_point;
        if (module->findEntryPointByName(entry_point_name.data(), &entry_point) != SLANG_OK)
        {
            throw_error(fmt::format("finding entry point {}", entry_point_name));
        }
        return slang_unique_ptr<slang::IEntryPoint>(entry_point, destroy_slang);
    }

    void shader_compiler::throw_error(const std::string_view context)
    {
        throw std::runtime_error(std::string(context));
    }
    shader_compiler::slang_unique_ptr<slang::IComponentType>
    shader_compiler::create_composed_program(slang::IModule* module, slang::IEntryPoint* entry_point)
    {
        auto diagnostic_blob     = slang_unique_ptr<slang::IBlob>(nullptr, destroy_slang);
        auto diagnostic_blob_ptr = diagnostic_blob.get();

        auto component_types = std::vector<slang::IComponentType*>();
        component_types.push_back(module);
        component_types.push_back(entry_point);

        slang::IComponentType* composed_program = nullptr;

        if (session->createCompositeComponentType(
                component_types.data(), component_types.size(), &composed_program, &diagnostic_blob_ptr)
            != SLANG_OK)
        {
            throw_error("creating composed program", diagnostic_blob.get());
        }

        return slang_unique_ptr<slang::IComponentType>(composed_program, destroy_slang);
    }
    shader_compiler::slang_unique_ptr<slang::IBlob>
    shader_compiler::compile_to_spirv(slang::IComponentType* composed_program)
    {
        auto diagnostic_blob     = slang_unique_ptr<slang::IBlob>(nullptr, destroy_slang);
        auto diagnostic_blob_ptr = diagnostic_blob.get();

        slang::IBlob* spirv_blob = nullptr;

        if (composed_program->getEntryPointCode(0, 0, &spirv_blob, &diagnostic_blob_ptr) != SLANG_OK)
        {
            throw_error("compiling to spirv", diagnostic_blob.get());
        }

        return slang_unique_ptr<slang::IBlob>(spirv_blob, destroy_slang);
    }

} // namespace cfi