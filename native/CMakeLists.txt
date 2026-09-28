cmake_minimum_required(VERSION 3.10)

project(camora_v4l2 LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(
    GST REQUIRED
    gstreamer-1.0
    gstreamer-app-1.0
)

add_library(
    camora_v4l2
    SHARED

    camora_v4l2.cpp
    capture_engine.cpp
    background_segmenter.cpp
)

target_include_directories(
    camora_v4l2
    PRIVATE
    ${GST_INCLUDE_DIRS}
)

target_link_directories(
    camora_v4l2
    PRIVATE
    ${GST_LIBRARY_DIRS}
)

target_link_libraries(
    camora_v4l2
    PRIVATE
    ${GST_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

target_compile_options(
    camora_v4l2
    PRIVATE
    "$<$<CONFIG:Debug>:-O2>"
    ${GST_CFLAGS_OTHER}
)

set_target_properties(
    camora_v4l2
    PROPERTIES

    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "camora_v4l2"
)
