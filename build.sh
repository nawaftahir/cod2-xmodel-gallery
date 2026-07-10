#!/usr/bin/env bash
# Zero-dependency build: needs only g++, gcc, and the Mesa/EGL runtime that
# ships with WSLg (or any desktop Mesa). No -dev packages, no apt install.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
echo "[1/2] miniz (C)"
gcc -std=c11 -O2 -c third_party/miniz.c -o build/miniz.o
echo "[2/2] cod2-xmodel-gallery (C++)"
g++ -std=c++17 -O2 -Isrc src/*.cpp build/miniz.o \
    -o build/cod2-xmodel-gallery -l:libEGL.so.1 -l:libGL.so.1
# (src/*.cpp already includes categorize.cpp)
echo "-> build/cod2-xmodel-gallery"
