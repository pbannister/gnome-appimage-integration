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
include_directories("${CMAKE_BINARY_DIR}/generated")

find_package(ZLIB REQUIRED)
find_package(LibLZMA)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(ZSTD IMPORTED_TARGET libzstd)
endif()

add_library(version_info STATIC version/version_info.cpp version/version_compare.cpp)
target_include_directories(version_info PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(desktop_entry STATIC
    desktop/desktop_entry_reader.cpp
    desktop/desktop_entry_locator.cpp
    desktop/icon_theme_locator.cpp
    desktop/mime_association_reader.cpp)
target_include_directories(desktop_entry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(appimage_reader STATIC
    appimage/appimage_reader.cpp
    appimage/appimage_signature.cpp
    appimage/appimage_update_information.cpp
    appimage/squashfs_reader.cpp)
target_include_directories(appimage_reader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(appimage_reader PUBLIC ZLIB::ZLIB crypto)
target_compile_definitions(appimage_reader PRIVATE APPIMAGE_HAVE_ZLIB=1)

if(LibLZMA_FOUND)
    target_link_libraries(appimage_reader PUBLIC LibLZMA::LibLZMA)
    target_compile_definitions(appimage_reader PRIVATE APPIMAGE_HAVE_LZMA=1)
endif()

if(TARGET PkgConfig::ZSTD)
    target_link_libraries(appimage_reader PUBLIC PkgConfig::ZSTD)
    target_compile_definitions(appimage_reader PRIVATE APPIMAGE_HAVE_ZSTD=1)
endif()

add_library(crypto STATIC crypto/sha256.cpp)
target_include_directories(crypto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(json_value STATIC json/json_reader.cpp)
target_include_directories(json_value PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(integration STATIC
    integration/appimage_integrator.cpp)
target_include_directories(integration PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(integration PUBLIC appimage_reader desktop_entry version_info)

add_executable(appimage-inspect tools/appimage_inspect.cpp)
target_link_libraries(appimage-inspect PRIVATE appimage_reader desktop_entry version_info)

add_executable(appimage-integrate tools/appimage_integrate.cpp)
target_link_libraries(appimage-integrate PRIVATE integration appimage_reader desktop_entry json_value version_info)

file(COPY tools/icons DESTINATION "${CMAKE_BINARY_DIR}")

# The graphical activator is a GTK4 program. It is built only where the GTK4
# development files are present; the command-line tool works without it and falls
# back to zenity, and then to printed instructions.
if(PkgConfig_FOUND)
    pkg_check_modules(GTK4 IMPORTED_TARGET gtk4)
endif()
if(TARGET PkgConfig::GTK4)
    add_executable(appimage-activator tools/appimage_activator_ui.cpp)
    target_link_libraries(appimage-activator PRIVATE PkgConfig::GTK4 json_value version_info)
else()
    message(STATUS "GTK4 development files not found: appimage-activator is not built")
endif()

add_executable(desktop-inspect tools/desktop_inspect.cpp)
target_link_libraries(desktop-inspect PRIVATE desktop_entry version_info)

add_executable(desktop-entry-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/desktop_entry_reader_test.cpp")
target_link_libraries(desktop-entry-reader-test PRIVATE desktop_entry)

add_executable(desktop-entry-locator-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/desktop_entry_locator_test.cpp")
target_link_libraries(desktop-entry-locator-test PRIVATE desktop_entry)

add_executable(icon-theme-locator-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/icon_theme_locator_test.cpp")
target_link_libraries(icon-theme-locator-test PRIVATE desktop_entry)

add_executable(mime-association-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/mime_association_reader_test.cpp")
target_link_libraries(mime-association-reader-test PRIVATE desktop_entry)

add_executable(integration-plan-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/integration_plan_test.cpp")
target_link_libraries(integration-plan-test PRIVATE integration appimage_reader desktop_entry)

add_executable(appimage-update-information-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/appimage_update_information_test.cpp")
target_link_libraries(appimage-update-information-test PRIVATE appimage_reader)

add_executable(version-compare-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/version_compare_test.cpp")
target_link_libraries(version-compare-test PRIVATE version_info)

add_executable(appimage-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/appimage_reader_test.cpp")
target_link_libraries(appimage-reader-test PRIVATE appimage_reader)

add_executable(squashfs-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/squashfs_reader_test.cpp")
target_link_libraries(squashfs-reader-test PRIVATE appimage_reader)

add_executable(json-reader-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/json_reader_test.cpp")
target_link_libraries(json-reader-test PRIVATE json_value)

add_executable(sha256-test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/sha256_test.cpp")
target_link_libraries(sha256-test PRIVATE crypto)
