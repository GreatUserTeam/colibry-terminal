# GusGpu.cmake - optional GPU-accelerated rendering for Colibry (GUS) Qt Widgets applications.
#
# Reuse in any GUS application:
#   1. copy cmake/GusGpu.cmake and src/gpu/ into the project;
#   2. include(cmake/GusGpu.cmake)  and  gus_enable_gpu(<target> SOURCES_VAR ...)  -- see below;
#   3. call gus::gpu::configureDefaultFormat() before QApplication is created;
#   4. create the drawing surface with gus::gpu::createRenderSurface().
#
# gus_enable_gpu(<target>) links Qt6::OpenGLWidgets when it is installed and defines
# GUS_GPU_AVAILABLE=1 for the target. Without Qt6::OpenGLWidgets the same code compiles to a
# software-only surface, so the option never makes the build fail.

option(GUS_GPU "Build with OpenGL-accelerated rendering when Qt6::OpenGLWidgets is available" ON)

function(gus_enable_gpu target)
    if(NOT GUS_GPU)
        message(STATUS "GPU rendering: disabled (GUS_GPU=OFF)")
        return()
    endif()
    find_package(Qt6 QUIET OPTIONAL_COMPONENTS OpenGLWidgets)
    if(TARGET Qt6::OpenGLWidgets)
        target_link_libraries(${target} PUBLIC Qt6::OpenGLWidgets)
        target_compile_definitions(${target} PUBLIC GUS_GPU_AVAILABLE=1)
        message(STATUS "GPU rendering: enabled (Qt6::OpenGLWidgets)")
    else()
        message(STATUS "GPU rendering: Qt6::OpenGLWidgets not found, software rendering only")
    endif()
endfunction()
