cmake_minimum_required(VERSION 3.24)
project(CalcApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

##########################################################################################
# RAYLIB

set(RAYLIB_VERSION 5.5)

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS ${RAYLIB_VERSION}
)
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(raylib)

##########################################################################################
# EXECUTABLE

add_executable(${PROJECT_NAME})

target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/calcapp_pch.h)

set(SOURCES
    src/calcapp_pch.cpp
    src/main.cpp
)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
