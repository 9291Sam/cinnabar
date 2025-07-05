cmake_minimum_required(VERSION 3.31)
project(cinnabar VERSION 0.0.0.1)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(cinnabar
    src/game/game.cpp

    src/gfx/core/vulkan/allocator.cpp
    src/gfx/core/vulkan/buffer.cpp
    src/gfx/core/vulkan/descriptor_manager.cpp
    src/gfx/core/vulkan/device.cpp
    src/gfx/core/vulkan/frame_manager.cpp
    src/gfx/core/vulkan/image.cpp
    src/gfx/core/vulkan/instance.cpp
    src/gfx/core/vulkan/pipeline_manager.cpp
    src/gfx/core/vulkan/swapchain.cpp

    src/gfx/core/renderer.cpp
    src/gfx/core/window.cpp

    src/gfx/generators/imgui/imgui_renderer.cpp

    src/gfx/generators/skybox/skybox_renderer.cpp

    src/gfx/generators/triangle/triangle_renderer.cpp

    src/gfx/generators/voxel/emissive_integer_tree.cpp
    src/gfx/generators/voxel/generator.cpp
    src/gfx/generators/voxel/light_influence_storage.cpp
    src/gfx/generators/voxel/material.cpp
    src/gfx/generators/voxel/model.cpp
    src/gfx/generators/voxel/voxel_renderer.cpp

    src/gfx/camera.cpp
    src/gfx/frame_generator.cpp
    src/gfx/transform.cpp
    src/gfx/slang_compiler.cpp
    src/gfx/voxel_world_manager.cpp

    src/util/allocators/index_allocator.cpp
    src/util/allocators/range_allocator.cpp

    src/util/events.cpp
    src/util/gif.cpp
    src/util/logger.cpp
    src/util/timer.cpp
    src/util/util.cpp

    src/temporary_game_state.cpp
    src/main.cpp
)


# TODO INVESTIGATE
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W")
endif()

include(CPM.cmake)


set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME ON) 
set(BOOST_ENABLE_CMAKE ON)
set(BOOST_INCLUDE_LIBRARIES container unordered dynamic_bitset core type_traits sort pool geometry) # don't forget to target the library

set(SLANG_LIB_TYPE "SHARED" CACHE STRING "" FORCE)
set(SLANG_ENABLE_DXIL OFF)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_ENABLE_SLANGD OFF)
set(SLANG_ENABLE_SLANGC OFF)
set(SLANG_ENABLE_SLANGRT OFF)
set(SLANG_ENABLE_SLANG_GLSLANG OFF)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_REPLAYER OFF)
set(SLANG_EXCLUDE_DAWN ON)
set(SLANG_EXCLUDE_TINT ON)


# set(SLANG_ENABLE_ASAN ON)

CPMAddPackage("gh:fmtlib/fmt#11.1.3")
CPMAddPackage("gh:gabime/spdlog#v1.15.1")
CPMAddPackage("gh:jeremy-rifkin/cpptrace#v0.8.0")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#v3.2.1")
CPMAddPackage("gh:glfw/glfw#3.4")
CPMAddPackage("gh:g-truc/glm#1.0.1")
CPMAddPackage("gh:Neargye/magic_enum#v0.9.7")
CPMAddPackage("gh:boostorg/boost#boost-1.87.0")
CPMAddPackage("gh:ocornut/imgui#v1.91.8")
CPMAddPackage("gh:epezent/implot#v0.16")
CPMAddPackage("gh:sammycage/plutovg#v0.0.13")
CPMAddPackage("gh:jpaver/opengametools#1c185fe9ce7217df814ec51482a030d7aa66324d")
CPMAddPackage("gh:Auburn/FastNoise2#f8facbad699a51f0b5a0800223d0813cca1d34be")
CPMAddPackage("gh:wolfpld/tracy#v0.12.1")
CPMAddPackage(NAME plutosvg GITHUB_REPOSITORY "sammycage/plutosvg" VERSION 0.0.6 DOWNLOAD_ONLY TRUE)
find_package(Vulkan REQUIRED COMPONENTS)
set(FREETYPE_DIR "$ENV{FREETYPE_DIR}")
if (WIN32 AND NOT FREETYPE_DIR)
    message(FATAL_ERROR "You need to install freetype and set the FREETYPE_DIR environment variable https://github.com/ubawurinna/freetype-windows-binaries")
endif()
find_package(Freetype REQUIRED)

add_library(imgui_build_target STATIC
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp    
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp

    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp

    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp


    ${plutosvg_SOURCE_DIR}/source/plutosvg.c
)
target_include_directories(imgui_build_target SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(imgui_build_target SYSTEM PUBLIC ${imgui_SOURCE_DIR})
target_include_directories(imgui_build_target SYSTEM PUBLIC ${implot_SOURCE_DIR})
target_include_directories(imgui_build_target SYSTEM PUBLIC ${FREETYPE_INCLUDE_DIRS})
target_include_directories(imgui_build_target SYSTEM PUBLIC ${plutosvg_SOURCE_DIR}/source)
target_compile_definitions(imgui_build_target PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_compile_definitions(imgui_build_target PUBLIC VULKAN_HPP_NO_CONSTRUCTORS=1)
target_compile_definitions(imgui_build_target PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES=1)
target_compile_definitions(imgui_build_target PUBLIC IMGUI_USE_WCHAR32=1)
target_compile_definitions(imgui_build_target PUBLIC IMGUI_ENABLE_FREETYPE=1)
target_compile_definitions(imgui_build_target PUBLIC IMGUI_ENABLE_FREETYPE_PLUTOSVG=1)
target_compile_definitions(imgui_build_target PUBLIC PLUTOSVG_HAS_FREETYPE=1)
target_compile_definitions(imgui_build_target PUBLIC PLUTOSVG_BUILD_STATIC=1)
target_compile_definitions(imgui_build_target PUBLIC VK_NO_PROTOTYPES=1)
target_link_libraries(
    imgui_build_target
    PUBLIC 
    glfw
    plutovg::plutovg
    ${FREETYPE_LIBRARIES}
)

target_include_directories(cinnabar PRIVATE src)
target_include_directories(cinnabar SYSTEM PRIVATE ${CPM_PACKAGE_opengametools_SOURCE_DIR}/src)
target_include_directories(cinnabar SYSTEM PRIVATE ${CPM_PACKAGE_libkdtree_SOURCE_DIR})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_TWEAK=${PROJECT_VERSION_TWEAK})
target_compile_definitions(cinnabar PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
target_compile_definitions(cinnabar PUBLIC GLM_SWIZZLE=1)
target_compile_definitions(cinnabar PUBLIC GLM_FORCE_RADIANS=1)
target_compile_definitions(cinnabar PUBLIC GLM_FORCE_SIZE_T_LENGTH=1)
target_compile_definitions(cinnabar PUBLIC GLM_ENABLE_EXPERIMENTAL=1)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(cinnabar PUBLIC CINNABAR_DEBUG_BUILD=1)
else()
    target_compile_definitions(cinnabar PUBLIC CINNABAR_DEBUG_BUILD=0)
endif()
target_compile_definitions(cinnabar PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_compile_definitions(cinnabar PRIVATE VULKAN_HPP_NO_CONSTRUCTORS=1)
target_compile_definitions(cinnabar PRIVATE VK_NO_PROTOTYPES=1)
target_compile_definitions(cinnabar PRIVATE TRACY_ENABLE=1)
target_link_libraries(cinnabar PUBLIC
    fmt::fmt
    spdlog::spdlog
    cpptrace::cpptrace
    VulkanMemoryAllocator
    glm
    magic_enum::magic_enum
    Boost::container
    Boost::unordered
    Boost::dynamic_bitset
    Boost::core
    Boost::type_traits
    Boost::sort
    Boost::pool
    Boost::geometry
    imgui_build_target
    TracyClient
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
    target_compile_options(cinnabar PUBLIC
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-c++20-compat
        -Wno-c++20-extensions
        -Wno-pre-c++20-compat-pedantic
        -Wno-covered-switch-default
        -Wno-ctad-maybe-unsupported
        -Wno-pre-c++17-compat
        -Wno-braced-scalar-init
        -Wno-unsafe-buffer-usage
        -Wno-gnu-zero-line-directive
        -Wno-missing-prototypes
        -Wno-switch-enum
        -Wno-undefined-func-template
        -Wno-newline-eof
        -Wno-padded
        -Wno-poison-system-directories
        -Wno-switch-default
        -Wno-weak-vtables
        -Wno-incompatible-pointer-types
        # -Wno-nan-infinity-disabled
        -Wno-documentation-unknown-command
        -Wno-language-extension-token
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # Add debug symbols that lldb can actually read
        # target_compile_options(cinnabar PUBLIC -glldb) 
        
        # target_compile_options(cinnabar PUBLIC -fsanitize=address)
        # target_link_options(cinnabar PUBLIC -fsanitize=address)

        # target_compile_options(imgui_build_target PUBLIC -fsanitize=address)
        # target_link_options(imgui_build_target PUBLIC -fsanitize=address)

        

    endif ()

endif()


# When I prompted an LLM for this code, only the LLM understood it. Now with my subscription expired, nobody does!

CPMAddPackage("gh:shader-slang/slang#v2025.6.3")

# Complete list of all dependency targets
set(ALL_DEPENDENCY_TARGETS 
    # Main targets
    slang core compiler-core
    
    # Directory targets
    copy-slang-tint copy-slang-llvm copy-prebuilt-binaries
    
    # External targets
    metal-cpp imgui stb
    
    # FastNoise2 targets (both alias and real target)
    FastNoise # for some reason 2 is an alias to 1 bruh
    
    # Tools targets
    all-generators slang-fiddle slang-embed slang-generate 
    slang-lookup-generator slang-capability-generator 
    slang-spirv-embed-generator slang-bootstrap 
    slang-cpp-parser slangi
    
    # Prelude
    prelude
    
    # Core module targets
    slang-embedded-core-module-source slang-no-embedded-core-module-source
    generate_core_module_headers slang-embedded-core-module 
    slang-no-embedded-core-module
    
    # GLSL module
    slang-glsl-module
    
    # Slang-specific targets
    slang-fiddle-output slang-capability-defs slang-capability-lookup
    slang-lookup-tables copy_slang_headers slang-common-objects
    slang-without-embedded-core-module
)

foreach(target ${ALL_DEPENDENCY_TARGETS})
    if(TARGET ${target})
        # Check target type
        get_target_property(target_type ${target} TYPE)
        
        # Handle alias targets - get the real target and process that instead
        if(target_type STREQUAL "ALIAS")
            get_target_property(aliased_target ${target} ALIASED_TARGET)
            # message(STATUS "Found alias target: ${target} -> ${aliased_target}")
            
            # Process the real target if it exists and is compilable
            if(aliased_target AND TARGET ${aliased_target})
                get_target_property(real_target_type ${aliased_target} TYPE)
                get_target_property(real_is_imported ${aliased_target} IMPORTED)
                
                if(real_target_type STREQUAL "EXECUTABLE" OR 
                   real_target_type STREQUAL "STATIC_LIBRARY" OR 
                   real_target_type STREQUAL "SHARED_LIBRARY" OR 
                   real_target_type STREQUAL "MODULE_LIBRARY" OR
                   real_target_type STREQUAL "OBJECT_LIBRARY")
                    
                    if(real_is_imported)
                        # message(STATUS "Suppressing warnings for imported aliased target: ${aliased_target} (${real_target_type})")
                        target_compile_options(${aliased_target} INTERFACE -w)
                    else()
                        # message(STATUS "Suppressing warnings for aliased target: ${aliased_target} (${real_target_type})")
                        target_compile_options(${aliased_target} PRIVATE -w)
                    endif()
                endif()
            endif()
            # Continue to next iteration - don't process the alias target itself
            continue()
        endif()
        
        # Handle regular targets - only apply compile options to compilable target types
        if(target_type STREQUAL "EXECUTABLE" OR 
           target_type STREQUAL "STATIC_LIBRARY" OR 
           target_type STREQUAL "SHARED_LIBRARY" OR 
           target_type STREQUAL "MODULE_LIBRARY" OR
           target_type STREQUAL "OBJECT_LIBRARY")
            
            get_target_property(is_imported ${target} IMPORTED)
            if(is_imported)
                # message(STATUS "Suppressing warnings for imported target: ${target} (${target_type})")
                target_compile_options(${target} INTERFACE -w)
            else()
                # message(STATUS "Suppressing warnings for target: ${target} (${target_type})")
                target_compile_options(${target} PRIVATE -w)
            endif()
        else()
            # message(STATUS "Skipping non-compilable target: ${target} (${target_type})")
        endif()
    endif()
endforeach()

target_link_libraries(cinnabar PUBLIC slang FastNoise2)