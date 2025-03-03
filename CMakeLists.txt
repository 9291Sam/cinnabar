cmake_minimum_required(VERSION 3.28)
project(cinnabar VERSION 0.0.0.1)



include(CPM.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME ON) 
set(BOOST_ENABLE_CMAKE ON)
set(BOOST_INCLUDE_LIBRARIES container unordered dynamic_bitset core type_traits sort pool) # don't forget to target the library

CPMAddPackage("gh:fmtlib/fmt#11.1.3")
CPMAddPackage("gh:gabime/spdlog#v1.15.1")
CPMAddPackage("gh:jeremy-rifkin/cpptrace#v0.8.0")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#v3.2.1")
CPMAddPackage("gh:glfw/glfw#3.4")
CPMAddPackage("gh:g-truc/glm#1.0.1")
CPMAddPackage("gh:Neargye/magic_enum#v0.9.7")
CPMAddPackage("gh:boostorg/boost#boost-1.87.0")
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)


add_executable(cinnabar
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

    src/util/allocators/index_allocator.cpp
    src/util/allocators/range_allocator.cpp

    src/util/logger.cpp

    src/main.cpp
)

target_include_directories(cinnabar PRIVATE src)
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_compile_definitions(cinnabar PUBLIC CINNABAR_VERSION_TWEAK=${PROJECT_VERSION_TWEAK})
target_compile_definitions(cinnabar INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
target_compile_definitions(cinnabar INTERFACE GLM_SWIZZLE=1)
target_compile_definitions(cinnabar INTERFACE GLM_FORCE_RADIANS=1)
target_compile_definitions(cinnabar INTERFACE GLM_FORCE_SIZE_T_LENGTH=1)
target_compile_definitions(cinnabar INTERFACE GLM_ENABLE_EXPERIMENTAL=1)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(cinnabar PUBLIC CINNABAR_DEBUG_BUILD=1)
else()
    target_compile_definitions(cinnabar PUBLIC CINNABAR_DEBUG_BUILD=0)
endif()
target_compile_definitions(cinnabar PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_compile_definitions(cinnabar PRIVATE VULKAN_HPP_NO_CONSTRUCTORS=1)
target_compile_definitions(cinnabar PRIVATE VK_NO_PROTOTYPES=1)

target_link_libraries(cinnabar
    fmt::fmt
    spdlog::spdlog
    cpptrace::cpptrace
    VulkanMemoryAllocator
    glfw
    glm
    Vulkan::shaderc_combined
    magic_enum::magic_enum
    Boost::container
    Boost::unordered
    Boost::dynamic_bitset
    Boost::core
    Boost::type_traits
    Boost::sort
    Boost::pool
)



if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
    target_compile_options(cinnabar PUBLIC
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-c++20-compat
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
        -Wno-nan-infinity-disabled
        -Wno-documentation-unknown-command
        -Wno-unsafe-buffer-usage
        -Wno-language-extension-token
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # Add debug symbols that lldb can actually read
        # target_compile_options(cinnabar PUBLIC -glldb)
        # target_compile_options(cinnabar PUBLIC -gdwarf-4)
        # target_compile_options(cinnabar PUBLIC -g3)

        # target_compile_options(cinnabar PUBLIC -fsanitize=thread)
        # target_link_options(cinnabar PUBLIC -fsanitize=thread)

    endif ()

endif()