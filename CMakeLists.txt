cmake_minimum_required(VERSION 3.14)

project(cinnabar)

include(CPM.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)


add_executable(cinnabar src/main.cpp)

CPMAddPackage("gh:fmtlib/fmt#11.1.3")
CPMAddPackage("gh:gabime/spdlog#v1.15.1")
CPMAddPackage("gh:jeremy-rifkin/cpptrace#v0.8.0")

target_link_libraries(cinnabar fmt::fmt spdlog::spdlog cpptrace::cpptrace)