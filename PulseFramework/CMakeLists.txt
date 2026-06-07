cmake_minimum_required(VERSION 3.14)
project(PulseFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 1. Build the proxy version.dll
add_library(version SHARED src/proxy.cpp)
target_link_libraries(version PRIVATE user32.lib)
set_target_properties(version PROPERTIES PREFIX "")

# 2. Build the actual PulseFramework.dll (launches PulseUI.exe)
add_library(PulseFramework SHARED src/pulse.cpp)
target_link_libraries(PulseFramework PRIVATE user32.lib)
set_target_properties(PulseFramework PROPERTIES PREFIX "")
