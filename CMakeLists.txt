cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)

project(bash-llvm)

set(CMAKE_CXX_SCAN_FOR_MODULES 0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

option(USE_CCACHE "Use ccache for compilation" OFF)
set(CCACHE_PATH "ccache" CACHE STRING "Path to ccache binary")
if(USE_CCACHE)
    find_program(CCACHE_BINARY ${CCACHE_PATH})
    if(CCACHE_BINARY)
        message(STATUS "[UseCCache] Found ccache at: ${CCACHE_BINARY}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_BINARY})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_BINARY})
    else()
        message(FATAL_ERROR "[UseCCache] USE_CCACHE enabled, but no "
            "executable found at: ${CCACHE_PATH}")
    endif()
endif()

include(cmake/CPM.cmake)
add_subdirectory(src)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_subdirectory(tests)
endif()
