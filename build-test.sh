#!/usr/bin/env sh

set -xue

root_dir=${PWD}

mkdir -p build && cd build
cmake ..
cmake --build . --target ia-test -- -j$(nproc)

cd ${root_dir}
