#!/usr/bin/env sh

set -xue

root_dir=$PWD

mkdir -p build && cd build
cmake ..
VERBOSE=1 cmake --build . --target install -- -j$(nproc)

cd $root_dir
