cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(GitEditor VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# SQLite via CPM (reusing Geode's bundled CPM.cmake).
include($ENV{GEODE_SDK}/cmake/CPM.cmake)

# Keep SQLite tiny: no shell, no FTS, no RTree.
set(BUILD_SHELL OFF CACHE BOOL "" FORCE)
set(SQLITE_ENABLE_FTS5 OFF CACHE BOOL "" FORCE)
set(SQLITE_ENABLE_RTREE OFF CACHE BOOL "" FORCE)
set(SQLITE_ENABLE_MATH_FUNCTIONS OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME sqlite3
    GITHUB_REPOSITORY azadkuh/sqlite-amalgamation
    GIT_TAG 3.38.2
    OPTIONS
        "BUILD_SHELL OFF"
)

# SQLite must be position-independent since we link it into a shared mod.
if (NOT TARGET SQLite3)
    message(FATAL_ERROR "SQLite3 target not created by CPMAddPackage")
endif()

set_target_properties(SQLite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} SQLite3)

# Upstream only exposes its header via INSTALL_INTERFACE, which CPM never
# triggers. Wire the BUILD_INTERFACE ourselves so we can #include <sqlite3.h>.
target_include_directories(${PROJECT_NAME} PRIVATE ${sqlite3_SOURCE_DIR})

setup_geode_mod(${PROJECT_NAME})
