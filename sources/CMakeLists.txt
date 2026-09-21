#
#	CMakeLists.txt for the GNOME AppImage Integration readers.
#
#	The build tree is generated output and is written to dataflow.out/build/.
#	Warnings are treated as errors per prompts/flavors/02-cpp-conventions.md.
#
cmake_minimum_required(VERSION 3.16)
project(gnome_appimage_integration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

find_package(ZLIB REQUIRED)
find_package(LibLZMA)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(ZSTD IMPORTED_TARGET libzstd)
endif()

add_library(desktop_entry STATIC
    desktop/desktop_entry_reader.cpp
    desktop/desktop_entry_locator.cpp)
target_include_directories(desktop_entry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(desktop-entry-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/desktop_entry_reader_test.cpp")
target_link_libraries(desktop-entry-reader-test PRIVATE desktop_entry)

add_executable(desktop-entry-locator-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/desktop_entry_locator_test.cpp")
target_link_libraries(desktop-entry-locator-test PRIVATE desktop_entry)
