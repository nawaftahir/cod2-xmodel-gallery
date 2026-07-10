cmake_minimum_required(VERSION 3.16)
project(cod2_xmodel_gallery CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(cod2-xmodel-gallery
    src/main.cpp
    src/gl.cpp
    src/vfs.cpp
    src/image.cpp
    src/xmodel.cpp
    src/material.cpp
    src/renderer.cpp
    src/gallery.cpp
    src/categorize.cpp
    third_party/miniz.c
)

# Headless GL via the Mesa/GLVND runtime. No -dev packages are required: link the
# runtime shared objects by exact soname (works without libEGL.so/libGL.so devel
# symlinks) and vendor the few headers we need. If you have the -dev packages and
# prefer find_package(OpenGL)/EGL, swapping these two lines also works.
target_link_options(cod2-xmodel-gallery PRIVATE -l:libEGL.so.1 -l:libGL.so.1)
