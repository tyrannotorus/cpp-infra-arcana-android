#!/usr/bin/env sh

set -xue

cmake -B build -DIA_BUILD_STATIC_SDL=OFF -DWIN32=TRUE -DMSVC=FALSE -DARCH=64bit -DCMAKE_TOOLCHAIN_FILE=../Toolchain-cross-mingw32.txt

VERBOSE=1 cmake --build build --target install -- -j$(nproc)
