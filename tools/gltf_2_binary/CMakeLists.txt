cmake_minimum_required(VERSION 3.13) # CMake version check
project(gltf_2_binary)                         # Create project "gltf_2_binary"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

add_executable(gltf_2_binary "main.cpp")

add_subdirectory(math)
add_subdirectory(utils)
add_subdirectory(external/tinyexr)
target_include_directories(gltf_2_binary PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(gltf_2_binary PRIVATE ${CMAKE_SOURCE_DIR}/external)
target_include_directories(gltf_2_binary PRIVATE ${CMAKE_SOURCE_DIR}/external/stb_image)
target_include_directories(gltf_2_binary PRIVATE ${CMAKE_SOURCE_DIR}/external/tiny_gltf)


file(GLOB_RECURSE MATH_DIR 
  math/*.cpp*
  math/*.h*
)
source_group("math" FILES ${MATH_DIR})

file(GLOB_RECURSE UTILS_DIR 
  utils/*.cpp*
  utils/*.h*
)
source_group("utils" FILES ${UTILS_DIR})

file(GLOB_RECURSE EXTERNAL_DIR 
  external/*.cpp*
  external/*.h*
  external/*.c*
)
source_group("external" FILES ${EXTERNAL_DIR})
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
