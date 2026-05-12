cmake_minimum_required(VERSION 3.6)

project(breakout)

cmake_policy(SET CMP0079 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(freetype)
add_subdirectory(imgui)

add_executable(breakout
   main.cpp
   glad/src/gles2.c
   stb.cpp
)

target_include_directories(breakout PRIVATE
   glad/include
   imgui/backends
   imgui
)

target_link_libraries(breakout PRIVATE freetype)
target_link_libraries(breakout PRIVATE imgui)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  # # android
  # EGL
  # GLESv3
  # log
  glfw
)
