cmake_minimum_required(VERSION 3.16)
project(videosdk_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: the SDK is a static library built with the static C runtime, so your
# app must use it too. Must come before find_package, which checks it.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

find_package(videosdk REQUIRED)

# NVIDIA Tegra (Jetson): the prebuilt libvideosdk.so references NVIDIA's V4L2
# stack (libnvv4l2 / libnvbufsurface), which lives under the Tegra multimedia
# dir — on the runtime loader path but NOT the compile-time linker search path.
# Add it so the example link resolves them. A no-op elsewhere (no such dir).
# NOTE: link_directories(), NOT target_link_directories(videosdk::cpp ...):
# videosdk::cpp is an IMPORTED target, so a target-scoped call errors with
# "Cannot specify link directories for target ... not built by this project".
if(CMAKE_LIBRARY_ARCHITECTURE AND EXISTS "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra")
    link_directories("/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra")
    message(STATUS "Jetson/Tegra lib dir added to link path")
endif()

add_executable(main main.cpp)
target_link_libraries(main PRIVATE videosdk::cpp)
