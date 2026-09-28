cmake_minimum_required(VERSION 3.10)

project(camora_v4l2 LANGUAGES CXX)

add_library(
  camora_v4l2
  SHARED
  camora_v4l2.cpp
)

set_target_properties(
  camora_v4l2
  PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  OUTPUT_NAME "camora_v4l2"
)
