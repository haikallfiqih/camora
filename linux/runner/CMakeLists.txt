cmake_minimum_required(VERSION 3.13)

find_package(PkgConfig REQUIRED)
find_file(
  ONNXRUNTIME_LIBRARY
  NAMES libonnxruntime.so libonnxruntime.so.1.23 libonnxruntime.so.1.23.2
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu
  REQUIRED
)

pkg_check_modules(
  GST REQUIRED
  IMPORTED_TARGET
  gstreamer-1.0
  gstreamer-app-1.0
)

add_executable(${BINARY_NAME}
  "main.cc"
  "my_application.cc"
  "camora_texture.cc"
  "${FLUTTER_MANAGED_DIR}/generated_plugin_registrant.cc"

  "../../native/camora_v4l2.cpp"
  "../../native/capture_engine.cpp"
  "../../native/background_segmenter.cpp"
)

apply_standard_settings(${BINARY_NAME})
target_compile_options(${BINARY_NAME} PRIVATE "$<$<CONFIG:Debug>:-O2>")

target_compile_features(
  ${BINARY_NAME}
  PRIVATE
  cxx_std_17
)

target_compile_definitions(
  ${BINARY_NAME}
  PRIVATE
  FLUTTER_PLUGIN_IMPL
  "APPLICATION_ID=\"${APPLICATION_ID}\""
)

target_include_directories(
  ${BINARY_NAME}
  PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/../native"
)

target_link_libraries(
  ${BINARY_NAME}
  PRIVATE
  flutter
  PkgConfig::GTK
  PkgConfig::GST
  ${ONNXRUNTIME_LIBRARY}
  ${CMAKE_DL_LIBS}
)

add_dependencies(
  ${BINARY_NAME}
  flutter_assemble
)
