cmake_minimum_required(VERSION 3.16)
project(gus-terminal VERSION 1.0.0 DESCRIPTION "GUS Terminal - terminal emulator of the Colibry desktop" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(GUS_TERMINAL_BUILD_TESTS "Build the tests" ON)
option(GUS_TERMINAL_WERROR "Treat warnings as errors" OFF)

if(NOT UNIX)
    message(FATAL_ERROR "GUS Terminal uses forkpty() and needs a Unix-like system (Linux, BSD, macOS).")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
if(GUS_TERMINAL_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GusGpu)
include(CheckLibraryExists)
include(GNUInstallDirs)

# forkpty() lives in libutil on older glibc and in libc on newer systems and macOS.
set(GUS_PTY_LIBS "")
check_library_exists(util forkpty "" GUS_HAVE_LIBUTIL)
if(GUS_HAVE_LIBUTIL)
    set(GUS_PTY_LIBS util)
endif()

function(gus_terminal_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(GUS_TERMINAL_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Everything except main() lives in a library so that the tests link the same code.
add_library(gus_terminal_core STATIC
    src/gpu/RenderSurface.cpp
    src/term/ColorScheme.cpp
    src/term/Screen.cpp
    src/term/Parser.cpp
    src/term/Input.cpp
    src/pty/Pty.cpp
    src/config/Config.cpp
    src/ui/TerminalWidget.cpp
    src/ui/SplitView.cpp
    src/ui/SearchBar.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/Theme.cpp
    src/resources.qrc
)
target_include_directories(gus_terminal_core PUBLIC src)
target_link_libraries(gus_terminal_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets ${GUS_PTY_LIBS})
gus_enable_gpu(gus_terminal_core)
gus_terminal_warnings(gus_terminal_core)

add_executable(gus-terminal src/main.cpp)
target_link_libraries(gus-terminal PRIVATE gus_terminal_core)
target_compile_definitions(gus-terminal PRIVATE GUS_TERMINAL_VERSION="${PROJECT_VERSION}")
gus_terminal_warnings(gus-terminal)

install(TARGETS gus-terminal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/gus-terminal.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/gus-terminal.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(GUS_TERMINAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
