cmake_minimum_required(VERSION 3.13) # CMake version check
project(baseball)                         # Create project "baseball"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

add_executable(baseball "main.cpp")

add_subdirectory(loader)
add_subdirectory(math)
add_subdirectory(render)
add_subdirectory(utils)
add_subdirectory(game)
add_subdirectory(external/curl)
add_subdirectory(external/rapidjson)
add_subdirectory(external/stb_image)
add_subdirectory(external/tiny_obj_loader)
add_subdirectory(external/tinyexr)
target_include_directories(baseball PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(baseball PRIVATE ${CMAKE_SOURCE_DIR}/external)

file(GLOB_RECURSE LOADER_DIR 
  loader/*.cpp*
  loader/*.h*
)
source_group("loader" FILES ${LOADER_DIR})

file(GLOB_RECURSE MATH_DIR 
  math/*.cpp*
  math/*.h*
)
source_group("math" FILES ${MATH_DIR})

file(GLOB_RECURSE RENDER_DIR 
  render/*.cpp*
  render/*.h*
)
source_group("render" FILES ${RENDER_DIR})

file(GLOB_RECURSE UTILS_DIR 
  utils/*.cpp*
  utils/*.h*
)
source_group("utils" FILES ${UTILS_DIR})

file(GLOB_RECURSE UTILS_DIR 
  game/*.cpp*
  game/*.h*
)
source_group("game" FILES ${UTILS_DIR})

file(GLOB_RECURSE EXTERNAL_DIR 
  external/*.cpp*
  external/*.h*
  external/*.c*
  external/curl/*.h*
  external/curl/*.c*
  external/rapid_json/*.h*
  external/rapid_json/*.c*
  external/stb_image/*.h*
  external/stb_image/*.c*
  external/tiny_obj_loader/*.h*
  external/tiny_obj_loader/*.c*
  external/tinyexr/*.h*
  external/tinyexr/*.c*
)
source_group("external" FILES ${EXTERNAL_DIR})

file(GLOB_RECURSE TINYEXR_DIR 
    external/tinyexr/*.h*
    external/tinyexr/*.c*
)
source_group("external/tinyexr" FILES ${TINYEXR_DIR})

if(EMSCRIPTEN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -g -O3"
  )
  target_compile_definitions(baseball PUBLIC EMBEDDED_FILES=1)
  set_target_properties(baseball PROPERTIES SUFFIX ".html")
  set_target_properties(baseball PROPERTIES LINK_FLAGS "--embed-file ../assets")
  target_link_options(baseball PRIVATE 
    "-sUSE_WEBGPU=1" 
    "-sUSE_GLFW=3" 
    "-sASYNCIFY" 
    #"-sMODULARIZE" 
    #"-sEXPORTED_RUNTIME_METHODS=['emscripten_fetch']"
    #"-sEXPORT_ES6=1" 
    "-sALLOW_MEMORY_GROWTH"
    "-sINITIAL_MEMORY=327680000"
    "-sENVIRONMENT='worker'" 
    "-sFETCH=1"
    #"-sUSE_PTHREADS=1"
    "-sASSERTIONS"
    "-sSTACK_SIZE=1048576")
elseif(WIN32)
  find_package(CURL REQUIRED)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("dawn" EXCLUDE_FROM_ALL)
  target_link_libraries(baseball PRIVATE dawn::webgpu_dawn glfw webgpu_glfw CURL::libcurl)
else()
  find_package(CURL REQUIRED)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -g -O0"
  )

  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("dawn" EXCLUDE_FROM_ALL)
  target_link_libraries(baseball PRIVATE dawn::webgpu_dawn glfw webgpu_glfw CURL::libcurl)
endif()
