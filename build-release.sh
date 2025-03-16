#!/usr/bin/env sh

set -xue

cmake -B build -DIA_BUILD_STATIC_SDL=ON

VERBOSE=1 cmake --build build --target install -- -j$(nproc)
