cmake_minimum_required(VERSION 3.16)
project(PulseUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Option for static build
option(BUILD_STATIC "Build with static Qt libraries" OFF)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    main.cpp
    overlaywindow.cpp
)

set(HEADERS
    overlaywindow.h
)

# Build as WIN32 (no console window)
add_executable(PulseUI WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(PulseUI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    dwmapi
    user32
)

# Static build configuration
if(BUILD_STATIC)
    target_link_libraries(PulseUI PRIVATE
        Qt6::QWindowsIntegrationPlugin
        Qt6::QWindowsVistaStylePlugin
    )
    if(MSVC)
        set_property(TARGET PulseUI PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
    target_compile_definitions(PulseUI PRIVATE QT_STATIC_BUILD)
endif()
